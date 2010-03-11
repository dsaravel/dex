#ifndef UTIL_H
#define UTIL_H

#include "common.h"

extern char *home_dir;

const char *editor_file(const char *name);
ssize_t xread(int fd, void *buf, size_t count);
ssize_t xwrite(int fd, const void *buf, size_t count);
char *path_absolute(const char *filename);
const char *get_file_type(mode_t mode);

#define mmap_empty ((void *)8UL)

static inline void *xmmap(int fd, off_t offset, size_t len)
{
	void *buf;
	if (!len)
		return mmap_empty;
	buf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, offset);
	if (buf == MAP_FAILED)
		return NULL;
	return buf;
}

static inline void xmunmap(void *start, size_t len)
{
	if (start != mmap_empty) {
		BUG_ON(munmap(start, len));
	} else {
		BUG_ON(len);
	}
}

#endif
