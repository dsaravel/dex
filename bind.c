#include "bind.h"
#include "common.h"
#include "error.h"
#include "command.h"
#include "ptr-array.h"

struct key_chain {
	unsigned int keys[3];
	unsigned char types[3]; // enum term_key_type fits in char
	unsigned char count;
};

struct binding {
	char *command;
	struct key_chain chain;
};

static struct key_chain pressed_keys;

static PTR_ARRAY(bindings);
static const char *special_names[NR_SKEYS] = {
	"insert",
	"delete",
	"home",
	"end",
	"pgup",
	"pgdown",
	"left",
	"right",
	"up",
	"down",
	"f1",
	"f2",
	"f3",
	"f4",
	"f5",
	"f6",
	"f7",
	"f8",
	"f9",
	"f10",
	"f11",
	"f12",
	"sleft",
	"sright",
};

static bool parse_key(unsigned char *type, unsigned int *key, const char *str)
{
	int len = strlen(str);
	int i;

	if (len == 1) {
		*type = KEY_NORMAL;
		*key = str[0];
		return true;
	}
	if (!strcasecmp(str, "sp") || !strcasecmp(str, "space")) {
		*type = KEY_NORMAL;
		*key = ' ';
		return true;
	}
	if (str[0] == '^' && len == 2) {
		unsigned char ch = toupper(str[1]);

		if (ch >= 0x40 && ch < 0x60) {
			*type = KEY_NORMAL;
			*key = ch - 0x40;
			return true;
		}
		if (ch == '?') {
			*type = KEY_NORMAL;
			*key = 0x7f;
			return true;
		}
	}
	if (toupper(str[0]) == 'M' && str[1] == '-' && parse_key(type, key, str + 2)) {
		*type = KEY_META;
		return true;
	}
	for (i = 0; i < NR_SKEYS; i++) {
		if (!strcasecmp(str, special_names[i])) {
			*type = KEY_SPECIAL;
			*key = i;
			return true;
		}
	}
	return false;
}

static bool parse_keys(struct key_chain *chain, const char *str)
{
	char *keys = xstrdup(str);
	int len = strlen(keys);
	int i = 0;

	// convert all whitespace to \0
	for (i = 0; i < len; i++) {
		if (isspace(keys[i])) {
			keys[i] = 0;
		}
	}

	clear(chain);
	for (i = 0; i < len; ) {
		const char *key;

		while (i < len && keys[i] == 0) {
			i++;
		}
		key = keys + i;
		while (i < len && keys[i] != 0) {
			i++;
		}
		if (key == keys + i)
			break;

		if (chain->count >= ARRAY_COUNT(chain->keys)) {
			error_msg("Too many keys.");
			free(keys);
			return false;
		}
		if (!parse_key(&chain->types[chain->count], &chain->keys[chain->count], key)) {
			error_msg("Invalid key %s", key);
			free(keys);
			return false;
		}
		chain->count++;
	}
	free(keys);
	if (chain->count == 0) {
		error_msg("Empty key not allowed.");
		return false;
	}
	return true;
}

void add_binding(const char *keys, const char *command)
{
	struct binding *b;

	b = xnew(struct binding, 1);
	if (!parse_keys(&b->chain, keys)) {
		free(b);
		return;
	}

	b->command = xstrdup(command);
	ptr_array_add(&bindings, b);
}

void remove_binding(const char *keys)
{
	struct key_chain chain;
	int i = bindings.count;

	if (!parse_keys(&chain, keys))
		return;

	while (i > 0) {
		struct binding *b = bindings.ptrs[--i];

		if (memcmp(&b->chain, &chain, sizeof(chain)) == 0) {
			ptr_array_remove_idx(&bindings, i);
			free(b->command);
			free(b);
			return;
		}
	}
}

void handle_binding(enum term_key_type type, unsigned int key)
{
	int i;

	pressed_keys.types[pressed_keys.count] = type;
	pressed_keys.keys[pressed_keys.count] = key;
	pressed_keys.count++;

	for (i = bindings.count; i > 0; i--) {
		struct binding *b = bindings.ptrs[i - 1];
		struct key_chain *c = &b->chain;

		if (c->count < pressed_keys.count)
			continue;

		if (memcmp(c->keys, pressed_keys.keys, pressed_keys.count * sizeof(pressed_keys.keys[0])))
			continue;
		if (memcmp(c->types, pressed_keys.types, pressed_keys.count * sizeof(pressed_keys.types[0])))
			continue;

		if (c->count > pressed_keys.count)
			return;

		handle_command(commands, b->command);
		break;
	}
	clear(&pressed_keys);
}

int nr_pressed_keys(void)
{
	return pressed_keys.count;
}
