#include "bio.h"
#include "common.h"

#include <assert.h>
#include <limits.h>

static void bio_reset_after_flush(struct bio *bio)
{
	assert(bio != NULL);

	bio->b = 0;
	bio->c = 0;
}

void bio_open(struct bio *bio, uchar *ptr, int mode)
{
	assert(bio != NULL);
	assert(ptr != NULL);

	bio->mode = mode;
	bio->ptr = ptr;

	switch (mode) {
		case BIO_MODE_READ:
			bio->c = 32;
			break;
		case BIO_MODE_WRITE:
			bio_reset_after_flush(bio);
			break;
	}
}

static void bio_flush_buffer(struct bio *bio)
{
	assert(bio != NULL);
	assert(bio->ptr != NULL);
	assert(sizeof(uint32) * CHAR_BIT == 32);

	*((uint32 *)bio->ptr) = bio->b;

	bio->ptr += 4;
}

static void bio_reload_buffer(struct bio *bio)
{
	assert(bio != NULL);
	assert(bio->ptr != NULL);

	bio->b = *(uint32 *)bio->ptr;

	bio->ptr += 4;
}

static void bio_put_nonzero_bit(struct bio *bio)
{
	assert(bio != NULL);
	assert(bio->c < 32);

	bio->b |= (uint32)1 << bio->c;

	bio->c++;

	if (bio->c == 32) {
		bio_flush_buffer(bio);
		bio_reset_after_flush(bio);
	}
}

static size_t minsize(size_t a, size_t b)
{
	return a < b ? a : b;
}

static size_t ctzu32(uint32 n)
{
	if (n == 0) {
		return 32;
	}

	switch (sizeof(uint32)) {
		static const int lut[32] = {
			0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
			31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
		};
#ifdef __GNUC__
		case sizeof(unsigned):
			return __builtin_ctz((unsigned)n);
		case sizeof(unsigned long):
			return __builtin_ctzl((unsigned long)n);
#endif
		default:
			/* http://graphics.stanford.edu/~seander/bithacks.html */
			return lut[((uint32)((n & -n) * 0x077CB531U)) >> 27];
	}
}

static uint32 bio_get_zeros_and_drop_bit(struct bio *bio)
{
	uint32 total_zeros = 0;

	assert(bio != NULL);

	do {
		size_t s;

		/* reload? */
		if (bio->c == 32) {
			bio_reload_buffer(bio);

			bio->c = 0;
		}

		/* get trailing zeros */
		s = minsize(32 - bio->c, ctzu32(bio->b));

		bio->b >>= s;
		bio->c += s;

		total_zeros += s;
	} while (bio->c == 32);

	assert(bio->c < 32);

	bio->b >>= 1;

	bio->c++;

	return total_zeros;
}

static void bio_write_bits(struct bio *bio, uint32 b, size_t n)
{
	assert(n <= 32);

	while (n > 0) {
		size_t m;

		assert(bio->c < 32);

		m = minsize(32 - bio->c, n);

		assert(32 >= bio->c + m);

		bio->b |= (uint32)((b & (((uint32)1 << m) - 1)) << bio->c);

		bio->c += m;

		if (bio->c == 32) {
			bio_flush_buffer(bio);
			bio_reset_after_flush(bio);
		}

		b >>= m;
		n -= m;
	}
}

static void bio_write_zero_bits(struct bio *bio, size_t n)
{
	assert(n <= 32);

	while (n > 0) {
		size_t m;

		assert(bio->c < 32);

		m = minsize(32 - bio->c, n);

		assert(32 >= bio->c + m);

		bio->c += m;

		if (bio->c == 32) {
			bio_flush_buffer(bio);
			bio_reset_after_flush(bio);
		}

		n -= m;
	}
}

static uint32 bio_read_bits(struct bio *bio, size_t n)
{
	uint32 w;
	size_t s;

	/* reload? */
	if (bio->c == 32) {
		bio_reload_buffer(bio);

		bio->c = 0;
	}

	/* get the avail. least-significant bits */
	s = minsize(32 - bio->c, n);

	w = bio->b & (((uint32)1 << s) - 1);

	bio->b >>= s;
	bio->c += s;

	n -= s;

	/* need more bits? reload & get the most-significant bits */
	if (n > 0) {
		assert(bio->c == 32);

		bio_reload_buffer(bio);

		bio->c = 0;

		w |= (bio->b & (((uint32)1 << n) - 1)) << s;

		bio->b >>= n;
		bio->c += n;
	}

	return w;
}

void bio_close(struct bio *bio)
{
	assert(bio != NULL);

	if (bio->mode == BIO_MODE_WRITE && bio->c > 0) {
		bio_flush_buffer(bio);
	}
}

static void bio_write_unary(struct bio *bio, uint32 N)
{
	while (N > 32) {
		bio_write_zero_bits(bio, 32);

		N -= 32;
	}

	bio_write_zero_bits(bio, N);

	bio_put_nonzero_bit(bio);
}

static uint32 bio_read_unary(struct bio *bio)
{
	return bio_get_zeros_and_drop_bit(bio);
}

void bio_write_gr(struct bio *bio, size_t k, uint32 N)
{
	uint32 Q = N >> k;

	bio_write_unary(bio, Q);

	assert(k <= 32);

	bio_write_bits(bio, N, k);
}

uint32 bio_read_gr(struct bio *bio, size_t k)
{
	uint32 Q;
	uint32 N;

	Q = bio_read_unary(bio);

	N = Q << k;

	assert(k <= 32);

	N |= bio_read_bits(bio, k);

	return N;
}
