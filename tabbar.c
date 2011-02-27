#include "tabbar.h"
#include "window.h"
#include "term.h"
#include "uchar.h"

static int leftmost_tab_idx;

static unsigned int number_width(unsigned int n)
{
	unsigned int width = 0;

	do {
		n /= 10;
		width++;
	} while (n);
	return width;
}

static void update_tab_title_width(struct view *v, int idx)
{
	const char *filename = v->buffer->filename;
	unsigned int w;

	if (!filename)
		filename = "(No name)";

	w = 3 + number_width(idx + 1);
	if (term_flags & TERM_UTF8) {
		unsigned int i = 0;
		while (filename[i])
			w += u_char_width(u_buf_get_char(filename, i + 4, &i));
	} else {
		w += strlen(filename);
	}

	v->tt_width = w;
	v->tt_truncated_width = w;
}

static void update_leftmost_tab_idx(int count)
{
	int min_left_idx, max_left_idx, w;
	struct view *v;

	w = 0;
	max_left_idx = count;
	list_for_each_entry_reverse(v, &window->views, node) {
		w += v->tt_truncated_width;
		if (w > window->w)
			break;
		max_left_idx--;
	}

	w = 0;
	min_left_idx = count;
	list_for_each_entry_reverse(v, &window->views, node) {
		if (w || v == view)
			w += v->tt_truncated_width;
		if (w > window->w)
			break;
		min_left_idx--;
	}

	if (leftmost_tab_idx < min_left_idx)
		leftmost_tab_idx = min_left_idx;
	if (leftmost_tab_idx > max_left_idx)
		leftmost_tab_idx = max_left_idx;
}

int calculate_tab_bar(void)
{
	int truncated_w = 20;
	int count = 0, total_w = 0;
	int truncated_count = 0, total_truncated_w = 0;
	int extra;
	struct view *v;

	list_for_each_entry(v, &window->views, node) {
		if (v == view) {
			// make sure current tab is visible
			if (leftmost_tab_idx > count)
				leftmost_tab_idx = count;
		}
		update_tab_title_width(v, count);
		total_w += v->tt_width;
		count++;

		if (v->tt_width > truncated_w) {
			total_truncated_w += truncated_w;
			truncated_count++;
		} else {
			total_truncated_w += v->tt_width;
		}
	}

	if (total_w <= window->w) {
		// all tabs fit without truncating
		leftmost_tab_idx = 0;
		return leftmost_tab_idx;
	}

	// truncate all wide tabs
	list_for_each_entry(v, &window->views, node) {
		if (v->tt_width > truncated_w)
			v->tt_truncated_width = truncated_w;
	}

	if (total_truncated_w > window->w) {
		// not all tabs fit even after truncating wide tabs
		update_leftmost_tab_idx(count);
		return leftmost_tab_idx;
	}

	// all tabs fit after truncating wide tabs
	extra = window->w - total_truncated_w;

	// divide extra space between truncated tabs
	while (extra > 0) {
		int extra_avg = extra / truncated_count;
		int extra_mod = extra % truncated_count;

		list_for_each_entry(v, &window->views, node) {
			int add = v->tt_width - v->tt_truncated_width;
			int avail;

			if (add == 0)
				continue;

			avail = extra_avg;
			if (extra_mod) {
				// this is needed for equal divide
				if (extra_avg == 0) {
					avail++;
					extra_mod--;
				}
			}
			if (add > avail)
				add = avail;
			else
				truncated_count--;

			v->tt_truncated_width += add;
			extra -= add;
		}
	}

	leftmost_tab_idx = 0;
	return leftmost_tab_idx;
}