#ifndef OPTIONS_H
#define OPTIONS_H

#include "libc.h"

enum newline_sequence {
	NEWLINE_UNIX,
	NEWLINE_DOS,
};

enum {
	// trailing whitespace
	WSE_TRAILING		= 1 << 0,

	// spaces in indentation
	// does not include less than tab-width spaces at end of indentation
	WSE_SPACE_INDENT	= 1 << 1,

	// less than tab-width spaces at end of indentation
	WSE_SPACE_ALIGN		= 1 << 2,

	// tab in indentation
	WSE_TAB_INDENT		= 1 << 3,

	// tab anywhere but in indentation
	WSE_TAB_AFTER_INDENT	= 1 << 4,

	// special whitespace characters
	WSE_SPECIAL		= 1 << 5,

	// expand-tab = false: WSE_SPACE_INDENT
	// expand-tab = true:  WSE_TAB_AFTER_INDENT | WSE_TAB_INDENT
	WSE_AUTO_INDENT		= 1 << 6,
};

enum case_sensitive_search {
	CSS_FALSE,
	CSS_TRUE,
	CSS_AUTO,
};

struct common_options {
	int auto_indent;
	int detect_indent;
	int emulate_tab;
	int expand_tab;
	int file_history;
	int indent_width;
	int syntax;
	int tab_width;
	int text_width;
	int ws_error;
};

struct local_options {
	/* these have also global values */
	int auto_indent;
	int detect_indent;
	int emulate_tab;
	int expand_tab;
	int file_history;
	int indent_width;
	int syntax;
	int tab_width;
	int text_width;
	int ws_error;

	/* only local */
	int brace_indent;
	char *filetype;
	char *indent_regex;
};

struct global_options {
	/* these have also local values */
	int auto_indent;
	int detect_indent;
	int emulate_tab;
	int expand_tab;
	int file_history;
	int indent_width;
	int syntax;
	int tab_width;
	int text_width;
	int ws_error;

	/* only global */
	enum case_sensitive_search case_sensitive_search;
	int display_special;
	int esc_timeout;
	int lock_files;
	enum newline_sequence newline; // default value for new files
	int scroll_margin;
	int show_line_numbers;
	int show_tab_bar;
	char *statusline_left;
	char *statusline_right;
	int tab_bar_max_components;
	int tab_bar_width;
	int vertical_tab_bar;
};

extern struct global_options options;
extern const char *case_sensitive_search_enum[];

#define TAB_BAR_MIN_WIDTH 12

void set_option(const char *name, const char *value, bool local, bool global);
void set_bool_option(const char *name, bool local, bool global);
void toggle_option(const char *name, bool global, bool verbose);
void toggle_option_values(const char *name, bool global, bool verbose, char **values);
bool validate_local_options(char **strs);
void collect_options(const char *prefix);
void collect_toggleable_options(const char *prefix);
void collect_option_values(const char *name, const char *prefix);
void free_local_options(struct local_options *opt);

#endif
