#include "buffer.h"

unsigned int buffer_get_char(struct block_iter *bi, uchar *up)
{
	struct block *blk = bi->blk;
	unsigned int offset = bi->offset;

	if (offset == blk->size) {
		if (blk->node.next == bi->head)
			return 0;
		bi->blk = blk = BLOCK(blk->node.next);
		bi->offset = offset = 0;
	}

	*up = blk->data[offset];
	if (*up < 0x80 || !buffer->utf8)
		return 1;

	*up = u_buf_get_char(blk->data, blk->size - offset, &offset);
	return offset - bi->offset;
}

unsigned int buffer_next_char(struct block_iter *bi, uchar *up)
{
	struct block *blk = bi->blk;
	unsigned int offset = bi->offset;

	if (offset == blk->size) {
		if (blk->node.next == bi->head)
			return 0;
		bi->blk = blk = BLOCK(blk->node.next);
		bi->offset = offset = 0;
	}

	// Note: this block can't be empty
	*up = blk->data[offset];
	if (*up < 0x80 || !buffer->utf8) {
		bi->offset++;
		return 1;
	}

	*up = u_buf_get_char(blk->data, blk->size - offset, &bi->offset);
	return bi->offset - offset;
}

unsigned int buffer_prev_char(struct block_iter *bi, uchar *up)
{
	struct block *blk = bi->blk;
	unsigned int offset = bi->offset;

	if (!offset) {
		if (blk->node.prev == bi->head)
			return 0;
		bi->blk = blk = BLOCK(blk->node.prev);
		bi->offset = offset = blk->size;
	}

	// Note: this block can't be empty
	*up = blk->data[offset - 1];
	if (*up < 0x80 || !buffer->utf8) {
		bi->offset--;
		return 1;
	}

	*up = u_prev_char(blk->data, &bi->offset);
	return offset - bi->offset;
}