#include "iter.h"
#include "common.h"

void block_iter_normalize(struct block_iter *bi)
{
	struct block *blk = bi->blk;

	if (bi->offset == blk->size && blk->node.next != bi->head) {
		bi->blk = BLOCK(blk->node.next);
		bi->offset = 0;
	}
}

/*
 * Move after next newline (beginning of next line or end of file).
 * Returns number of bytes iterator advanced.
 */
long block_iter_eat_line(struct block_iter *bi)
{
	long offset;

	block_iter_normalize(bi);

	offset = bi->offset;
	if (offset == bi->blk->size)
		return 0;

	// there must be at least one newline
	if (bi->blk->nl == 1) {
		bi->offset = bi->blk->size;
	} else {
		const unsigned char *end;
		end = memchr(bi->blk->data + offset, '\n', bi->blk->size - offset);
		bi->offset = end + 1 - bi->blk->data;
	}
	return bi->offset - offset;
}

/*
 * Move to beginning of next line.
 * If there is no next line iterator is not advanced.
 * Returns number of bytes iterator advanced.
 */
long block_iter_next_line(struct block_iter *bi)
{
	long offset;
	long new_offset;

	block_iter_normalize(bi);

	offset = bi->offset;
	if (offset == bi->blk->size)
		return 0;

	// there must be at least one newline
	if (bi->blk->nl == 1) {
		new_offset = bi->blk->size;
	} else {
		const unsigned char *end;
		end = memchr(bi->blk->data + offset, '\n', bi->blk->size - offset);
		new_offset = end + 1 - bi->blk->data;
	}
	if (new_offset == bi->blk->size && bi->blk->node.next == bi->head)
		return 0;

	bi->offset = new_offset;
	return bi->offset - offset;
}

/*
 * Move to beginning of previous line.
 * Returns number of bytes moved which is zero if there's no previous line.
 */
long block_iter_prev_line(struct block_iter *bi)
{
	struct block *blk = bi->blk;
	long offset = bi->offset;
	long start = offset;

	while (offset && blk->data[offset - 1] != '\n')
		offset--;

	if (!offset) {
		if (blk->node.prev == bi->head)
			return 0;
		bi->blk = blk = BLOCK(blk->node.prev);
		offset = blk->size;
		start += offset;
	}

	offset--;
	while (offset && blk->data[offset - 1] != '\n')
		offset--;
	bi->offset = offset;
	return start - offset;
}

long block_iter_bol(struct block_iter *bi)
{
	long offset, ret;

	block_iter_normalize(bi);

	offset = bi->offset;
	if (!offset || offset == bi->blk->size)
		return 0;

	if (bi->blk->nl == 1) {
		offset = 0;
	} else {
		while (offset && bi->blk->data[offset - 1] != '\n')
			offset--;
	}

	ret = bi->offset - offset;
	bi->offset = offset;
	return ret;
}

long block_iter_eol(struct block_iter *bi)
{
	struct block *blk;
	long offset;
	const unsigned char *end;

	block_iter_normalize(bi);

	blk = bi->blk;
	offset = bi->offset;
	if (offset == blk->size) {
		// cursor at end of last block
		return 0;
	}
	if (blk->nl == 1) {
		bi->offset = blk->size - 1;
		return bi->offset - offset;
	}
	end = memchr(blk->data + offset, '\n', blk->size - offset);
	bi->offset = end - blk->data;
	return bi->offset - offset;
}

void block_iter_back_bytes(struct block_iter *bi, long count)
{
	while (count > bi->offset) {
		count -= bi->offset;
		bi->blk = BLOCK(bi->blk->node.prev);
		bi->offset = bi->blk->size;
	}
	bi->offset -= count;
}

void block_iter_skip_bytes(struct block_iter *bi, long count)
{
	long avail = bi->blk->size - bi->offset;

	while (count > avail) {
		count -= avail;
		bi->blk = BLOCK(bi->blk->node.next);
		bi->offset = 0;
		avail = bi->blk->size;
	}
	bi->offset += count;
}

void block_iter_goto_offset(struct block_iter *bi, long offset)
{
	struct block *blk;

	list_for_each_entry(blk, bi->head, node) {
		if (offset <= blk->size) {
			bi->blk = blk;
			bi->offset = offset;
			return;
		}
		offset -= blk->size;
	}
}

void block_iter_goto_line(struct block_iter *bi, long line)
{
	struct block *blk = BLOCK(bi->head->next);
	long nl = 0;

	while (blk->node.next != bi->head && nl + blk->nl < line) {
		nl += blk->nl;
		blk = BLOCK(blk->node.next);
	}

	bi->blk = blk;
	bi->offset = 0;
	while (nl < line) {
		if (!block_iter_eat_line(bi))
			break;
		nl++;
	}
}

long block_iter_get_offset(const struct block_iter *bi)
{
	struct block *blk;
	long offset = 0;

	list_for_each_entry(blk, bi->head, node) {
		if (blk == bi->blk)
			break;
		offset += blk->size;
	}
	return offset + bi->offset;
}

bool block_iter_is_bol(const struct block_iter *bi)
{
	long offset = bi->offset;

	if (!offset)
		return true;
	return bi->blk->data[offset - 1] == '\n';
}

char *block_iter_get_bytes(const struct block_iter *bi, long len)
{
	struct block *blk = bi->blk;
	long offset = bi->offset;
	long pos = 0;
	char *buf;

	if (!len)
		return NULL;

	buf = xnew(char, len);
	while (pos < len) {
		long avail = blk->size - offset;
		long count = len - pos;

		if (count > avail)
			count = avail;
		memcpy(buf + pos, blk->data + offset, count);
		pos += count;

		BUG_ON(pos < len && blk->node.next == bi->head);
		blk = BLOCK(blk->node.next);
		offset = 0;
	}
	return buf;
}

// bi should be at bol
void fill_line_ref(struct block_iter *bi, struct lineref *lr)
{
	long max;
	const unsigned char *nl;

	block_iter_normalize(bi);

	lr->line = bi->blk->data + bi->offset;
	max = bi->blk->size - bi->offset;
	if (max == 0) {
		// cursor at end of last block
		lr->size = 0;
		return;
	}
	if (bi->blk->nl == 1) {
		lr->size = max - 1;
		return;
	}
	nl = memchr(lr->line, '\n', max);
	lr->size = nl - lr->line;
}

void fill_line_nl_ref(struct block_iter *bi, struct lineref *lr)
{
	long max;
	const unsigned char *nl;

	block_iter_normalize(bi);

	lr->line = bi->blk->data + bi->offset;
	max = bi->blk->size - bi->offset;
	if (max == 0) {
		// cursor at end of last block
		lr->size = 0;
		return;
	}
	if (bi->blk->nl == 1) {
		lr->size = max;
		return;
	}
	nl = memchr(lr->line, '\n', max);
	lr->size = nl - lr->line + 1;
}

long fetch_this_line(const struct block_iter *bi, struct lineref *lr)
{
	struct block_iter tmp = *bi;
	long count = block_iter_bol(&tmp);

	fill_line_ref(&tmp, lr);
	return count;
}
