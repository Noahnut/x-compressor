#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef unsigned char uchar;

/* co-occurence table [first][second] */
size_t coo[256][256];

/* frequencies */
size_t freq[256];

void init_freq()
{
	int i;

	for (i = 0; i < 256; ++i) {
		freq[i] = i;
	}
}

void swap_freq(int i, int j)
{
	size_t t = freq[i]; freq[i] = freq[j]; freq[j] = t;
}

void inc_freq(int j)
{
	size_t new_freq = freq[j] + 1;
	int i;

	/* check whether any other freq[] == new_freq */
	for (i = 0; i < 256; ++i) {
		if (i == j) {
			continue;
		}

		if (freq[i] == new_freq) {
			swap_freq(i, j);
			inc_freq(j);
			return;
		}
	}

	/* all freq[] are different from new_freq */
	freq[j] = new_freq;
}

void assert_uniq_freq()
{
	int i, j;

	for (i = 0; i < 256; ++i) {
		for (j = i + 1; j < 256; ++j) {
			assert(freq[i] != freq[j]);
		}
	}
}

struct rec {
	uchar c;
	size_t freq;
};

int compar(const void *l, const void *r)
{
	const struct rec *lr = l;
	const struct rec *rr = r;

	if (lr->freq > rr->freq) {
		return +1;
	}

	if (lr->freq < rr->freq) {
		return -1;
	}

	return 0;
}

int is_above_median(uchar pair[2])
{
	struct rec R[256];
	int i;

	for (i = 0; i < 256; ++i) {
		R[i].c = i;
		R[i].freq = coo[pair[0]][i];
	}

	qsort(R, 256, sizeof(struct rec), compar);

	for (i = 0; i < 128; ++i) {
		if (pair[1] == R[i].c) {
			return 1;
		}
	}

	return 0;
}

int compare_coo_pairs(uchar pair[2])
{
	int i;
	/* first after second N0 / D0 */

	size_t N0 = coo[pair[1]][pair[0]];
	size_t N1 = coo[pair[0]][pair[1]];
	size_t D0 = 0;
	size_t D1 = 0;

	for (i = 0; i < 256; ++i) {
		D0 += coo[pair[1]][i];
		D1 += coo[pair[0]][i];
	}

	/* N0/D0 > N1/D1 <=> N0*D1 > N1*D0 */

	assert(N0 <= ~0UL / D1);
	assert(N1 <= ~0UL / D0);

	return N0 * D1 > N1 * D0; /* FIXME: overflow */
}

int is_in_order_coo(uchar pair[2])
{
#if 1
	/* Type 0 */
	return coo[pair[0]][pair[1]] > coo[pair[1]][pair[0]];
#endif
#if 0
	/* Type 1 */
	uchar r[2];
	r[0] = pair[1]; r[1] = pair[0];
	return is_above_median(pair) && !is_above_median(r);
#endif
#if 0
	/* Type 2 */
	return compare_coo_pairs(pair);
#endif
}

/* TODO: more_recently_used first than second */
/* change < to > if you want */
int is_in_order_freq(uchar pair[2])
{
	return freq[pair[0]] > freq[pair[1]];
}

void update_model_coo(uchar pair[2])
{
	coo[pair[0]][pair[1]]++;
}

void update_model_freq(uchar pair[2])
{
#if 0
	inc_freq(pair[0]);
	inc_freq(pair[1]);
#else
	inc_freq(pair[0]);
#endif

	/* assert_uniq_freq(); */
}

void swap(uchar pair[2])
{
	uchar t = pair[0]; pair[0] = pair[1]; pair[1] = t;
}

#define is_in_order_subordinate(pair) is_in_order_freq(pair)

void transform(int in_order, uchar in[2], uchar out[2])
{
	out[0] = in[0];
	out[1] = in[1];

	if (in_order) {
		if (!is_in_order_subordinate(out)) {
			swap(out);
			assert( is_in_order_subordinate(out));
		}
	} else {
		if ( is_in_order_subordinate(out)) {
			swap(out);
			assert(!is_in_order_subordinate(out));
		}
	}
}

void process(FILE *istream, FILE *lstream, FILE *hstream)
{
	uchar pair[2] = { 0, 0 };
	uchar semi[2] = { 0, 0 };

	init_freq();

	while (fread(pair, 1, 2, istream) == 2) {
		int in_order = is_in_order_coo(pair);
		uchar out[2];

		transform(in_order, pair, out);

		fwrite(out+0, 1, 1, lstream);
		fwrite(out+1, 1, 1, hstream);

		/* decoder can decide whether swap(out) */
		assert(is_in_order_subordinate(out) == in_order);

		{
			semi[1] = pair[0];
			update_model_coo(semi);
		}
		update_model_coo(pair);
		update_model_freq(out);

		semi[0] = pair[1];
	}
}

/* L0 -> L1 H1 */
int main(int argc, char *argv[])
{
	FILE *istream = fopen(argc > 1 ? argv[1] : "enwik8", "r");
	FILE *lstream = fopen(argc > 2 ? argv[2] : "L", "w");
	FILE *hstream = fopen(argc > 3 ? argv[3] : "H", "w");

	if (istream == NULL) {
		abort();
	}

	if (lstream == NULL) {
		abort();
	}

	if (hstream == NULL) {
		abort();
	}

	process(istream, lstream, hstream);

	fclose(istream);
	fclose(lstream);
	fclose(hstream);

	return 0;
}
