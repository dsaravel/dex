#include "window.h"
#include "view.h"
#include "file-history.h"
#include "path.h"
#include "lock.h"
#include "load-save.h"
#include "error.h"
#include "move.h"

PTR_ARRAY(windows);
struct window *window;

struct window *new_window(void)
{
	return xnew0(struct window, 1);
}

struct view *window_add_buffer(struct window *w, struct buffer *b)
{
	struct view *v = xnew0(struct view, 1);

	v->buffer = b;
	v->window = w;
	v->cursor.head = &b->blocks;
	v->cursor.blk = BLOCK(b->blocks.next);

	ptr_array_add(&b->views, v);
	ptr_array_add(&w->views, v);
	w->update_tabbar = true;
	return v;
}

struct view *window_open_empty_buffer(struct window *w)
{
	return window_add_buffer(w, open_empty_buffer());
}

struct view *window_open_buffer(struct window *w, const char *filename, bool must_exist, const char *encoding)
{
	char *absolute = path_absolute(filename);
	bool dir_missing = false;
	struct buffer *b = NULL;

	if (absolute == NULL) {
		// Let load_buffer() create error message.
		dir_missing = errno == ENOENT;
	} else {
		// already open?
		b = find_buffer(absolute);
	}
	if (b) {
		if (!streq(absolute, b->abs_filename)) {
			char *s = short_filename(absolute);
			info_msg("%s and %s are the same file", s, b->display_filename);
			free(s);
		}
		free(absolute);
		return window_get_view(w, b);
	}

	/*
	/proc/$PID/fd/ contains symbolic links to files that have been opened
	by process $PID. Some of the files may have been deleted but can still
	be opened using the symbolic link but not by using the absolute path.

	# create file
	mkdir /tmp/x
	echo foo > /tmp/x/file

	# in another shell: keep the file open
	tail -f /tmp/x/file

	# make the absolute path unavailable
	rm /tmp/x/file
	rmdir /tmp/x

	# this should still succeed
	dex /proc/$(pidof tail)/fd/3
	*/
	b = buffer_new(encoding);
	if (load_buffer(b, must_exist, filename)) {
		free_buffer(b);
		return NULL;
	}
	if (b->st.st_mode == 0 && dir_missing) {
		// New file in non-existing directory. This is usually a mistake.
		error_msg("Error opening %s: Directory does not exist", filename);
		free_buffer(b);
		return NULL;
	}
	b->abs_filename = absolute;
	if (b->abs_filename == NULL) {
		// FIXME: obviously wrong
		b->abs_filename = xstrdup(filename);
	}
	update_short_filename(b);

	if (options.lock_files) {
		if (lock_file(b->abs_filename)) {
			b->ro = true;
		} else {
			b->locked = true;
		}
	}
	if (b->st.st_mode != 0 && !b->ro && access(filename, W_OK)) {
		error_msg("No write permission to %s, marking read-only.", filename);
		b->ro = true;
	}
	return window_add_buffer(w, b);
}

struct view *window_get_view(struct window *w, struct buffer *b)
{
	struct view *v;
	int i;

	for (i = 0; i < b->views.count; i++) {
		v = b->views.ptrs[i];
		if (v->window == w) {
			// already open in this window
			return v;
		}
	}
	// open the buffer in other window to this window
	v = window_add_buffer(w, b);
	v->cursor = ((struct view *)b->views.ptrs[0])->cursor;
	return v;
}

struct view *window_find_unclosable_view(struct window *w, bool (*can_close)(struct view *))
{
	long i;

	// check active view first
	if (w->view != NULL && !can_close(w->view)) {
		return w->view;
	}
	for (i = 0; i < w->views.count; i++) {
		struct view *v = w->views.ptrs[i];
		if (!can_close(v)) {
			return v;
		}
	}
	return NULL;
}

void window_remove_views(struct window *w)
{
	while (w->views.count > 0) {
		struct view *v = w->views.ptrs[w->views.count - 1];
		remove_view(v);
	}
}

// NOTE: w->frame isn't removed
void window_free(struct window *w)
{
	window_remove_views(w);
	free(w->views.ptrs);
	w->frame = NULL;
	free(w);
}

// Remove view from v->window and v->buffer->views and free it.
void remove_view(struct view *v)
{
	struct window *w = v->window;
	struct buffer *b = v->buffer;

	if (v == w->prev_view) {
		w->prev_view = NULL;
	}
	if (v == view) {
		view = NULL;
		buffer = NULL;
	}

	ptr_array_remove(&w->views, v);
	w->update_tabbar = true;

	ptr_array_remove(&b->views, v);
	if (b->views.count == 0) {
		if (b->options.file_history && b->abs_filename)
			add_file_history(v->cy + 1, v->cx_char + 1, b->abs_filename);
		free_buffer(b);
	}
	free(v);
}

void close_current_view(void)
{
	long idx = ptr_array_idx(&window->views, view);

	remove_view(view);
	if (window->prev_view != NULL) {
		set_view(window->prev_view);
		return;
	}
	if (window->views.count == 0)
		window_open_empty_buffer(window);
	if (window->views.count == idx)
		idx--;
	set_view(window->views.ptrs[idx]);
}

static void restore_cursor_from_history(struct view *v)
{
	int row, col;

	if (find_file_in_history(v->buffer->abs_filename, &row, &col)) {
		move_to_line(v, row);
		move_to_column(v, col);
	}
}

void set_view(struct view *v)
{
	int i;

	if (view == v)
		return;

	/* forget previous view when changing view using any other command but open */
	if (window != NULL) {
		window->prev_view = NULL;
	}

	view = v;
	buffer = v->buffer;
	window = v->window;

	window->view = v;

	if (!v->buffer->setup) {
		buffer_setup(v->buffer);
		if (v->buffer->options.file_history && v->buffer->abs_filename != NULL) {
			restore_cursor_from_history(v);
		}
	}

	// view.cursor can be invalid if same buffer was modified from another view
	if (view->restore_cursor) {
		view->cursor.blk = BLOCK(view->buffer->blocks.next);
		block_iter_goto_offset(&view->cursor, view->saved_cursor_offset);
		view->restore_cursor = false;
		view->saved_cursor_offset = 0;
	}

	// save cursor states of views sharing same buffer
	for (i = 0; i < buffer->views.count; i++) {
		v = buffer->views.ptrs[i];
		if (v != view) {
			v->saved_cursor_offset = block_iter_get_offset(&v->cursor);
			v->restore_cursor = true;
		}
	}
}

struct view *open_new_file(void)
{
	struct view *prev = view;
	struct view *v = window_open_empty_buffer(window);

	set_view(v);
	window->prev_view = prev;
	return v;
}

/*
If window contains only one buffer and it is untouched then it will be
closed after opening another file. This is done because closing last
buffer causes an empty buffer to be opened (window must contain at least
one buffer).
*/
static bool is_useless_empty_view(struct view *v)
{
	if (v == NULL)
		return false;
	if (v->window->views.count != 1)
		return false;
	// touched?
	if (v->buffer->abs_filename != NULL || v->buffer->change_head.nr_prev != 0)
		return false;
	return true;
}

struct view *open_file(const char *filename, const char *encoding)
{
	struct view *prev = view;
	bool useless = is_useless_empty_view(prev);
	struct view *v = window_open_buffer(window, filename, false, encoding);

	if (v == NULL)
		return NULL;

	set_view(v);
	if (useless) {
		remove_view(prev);
	} else {
		window->prev_view = prev;
	}
	return v;
}

void open_files(char **filenames, const char *encoding)
{
	struct view *empty = view;
	bool useless = is_useless_empty_view(empty);
	bool first = true;
	int i;

	for (i = 0; filenames[i]; i++) {
		struct view *v = window_open_buffer(window, filenames[i], false, encoding);
		if (v && first) {
			set_view(v);
			first = false;
		}
	}
	if (useless && view != empty) {
		remove_view(empty);
	}
}

void mark_buffer_tabbars_changed(struct buffer *b)
{
	long i;
	for (i = 0; i < b->views.count; i++) {
		struct view *v = b->views.ptrs[i];
		v->window->update_tabbar = true;
	}
}

int vertical_tabbar_width(struct window *win)
{
	// line numbers are included in min_edit_w
	int min_edit_w = 80;
	int w = 0;

	if (options.show_tab_bar && options.vertical_tab_bar)
		w = options.tab_bar_width;
	if (win->w - w < min_edit_w)
		w = win->w - min_edit_w;
	if (w < TAB_BAR_MIN_WIDTH)
		w = 0;
	return w;
}

static int line_numbers_width(struct window *win)
{
	int w = 0, min_w = 5;

	if (options.show_line_numbers && win->view) {
		w = number_width(win->view->buffer->nl) + 1;
		if (w < min_w)
			w = min_w;
	}
	return w;
}

static int edit_x_offset(struct window *win)
{
	return line_numbers_width(win) + vertical_tabbar_width(win);
}

static int edit_y_offset(struct window *win)
{
	if (options.show_tab_bar && !options.vertical_tab_bar)
		return 1;
	return 0;
}

static void set_edit_size(struct window *win)
{
	int xo = edit_x_offset(win);
	int yo = edit_y_offset(win);

	win->edit_w = win->w - xo;
	win->edit_h = win->h - yo - 1; // statusline
	win->edit_x = win->x + xo;
}

void calculate_line_numbers(struct window *win)
{
	int w = line_numbers_width(win);

	if (w != win->line_numbers.width) {
		win->line_numbers.width = w;
		win->line_numbers.first = 0;
		win->line_numbers.last = 0;
		mark_all_lines_changed(win->view->buffer);
	}
	set_edit_size(win);
}

void set_window_coordinates(struct window *win, int x, int y)
{
	win->x = x;
	win->y = y;
	win->edit_x = x + edit_x_offset(win);
	win->edit_y = y + edit_y_offset(win);
}

void set_window_size(struct window *win, int w, int h)
{
	win->w = w;
	win->h = h;
	calculate_line_numbers(win);
}

int window_get_scroll_margin(struct window *w)
{
	int max = (w->edit_h - 1) / 2;

	if (options.scroll_margin > max)
		return max;
	return options.scroll_margin;
}
