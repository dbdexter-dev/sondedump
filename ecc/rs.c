#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "rs.h"
#include "utils.h"

static int fix_block(uint8_t *data);

static uint8_t gfmul(uint8_t x, uint8_t y, uint8_t *alpha, uint8_t *logtable, int n);
static uint8_t gfdiv(uint8_t x, uint8_t y, uint8_t *alpha, uint8_t *logtable, int n);
static uint8_t gfpow(uint8_t x, int exp, uint8_t *alpha, uint8_t *logtable, int n);
static void poly_deriv(uint8_t *dst, const uint8_t *poly, int len);
static uint8_t poly_eval(const uint8_t *poly, uint8_t x, int len, uint8_t *alpha, uint8_t *logtable, int n);
static void poly_mul(uint8_t *dst, const uint8_t *poly1, const uint8_t *poly2, int len_1, int len_2, uint8_t *alpha, uint8_t *logtable, int n);

void
rs_init(RSDecoder *d, int n, int k, unsigned gen_poly, uint8_t first_root, int root_skip)
{
	const int t = n - k;
	int i, exp;
	unsigned tmp;

	d->alpha = malloc(n);
	d->logtable = malloc(n);
	d->zeroes = malloc(t);
	d->gaproots = malloc(n);

	d->n = n;
	d->k = k;
	d->t = t;
	d->first_root = first_root;

	/* Initialize alpha and logtable */
	d->alpha[0] = 1;
	for (i=1; i<n; i++) {
		tmp = (int)d->alpha[i-1] << 1;
		tmp = (tmp > (unsigned)n ? tmp ^ gen_poly : tmp);
		d->alpha[i] = tmp;
		d->logtable[tmp] = i;
	}

	/* Initialize the gap'th roots */
	for (i=0; i<n; i++) {
		d->gaproots[gfpow(i, root_skip, d->alpha, d->logtable, n)] = i;
	}

	/* Compute polynomial roots */
	for (i=0; i<t; i++) {
		exp = ((i + first_root) * root_skip) % n;
		d->zeroes[i] = d->alpha[exp];
	}
}

void
rs_deinit(RSDecoder *d)
{
	free(d->alpha);
	free(d->logtable);
	free(d->zeroes);
	free(d->gaproots);
}

int
rs_fix_block(const RSDecoder *self, uint8_t *data)
{
	int i, m, n, delta, prev_delta;
	int lambda_deg;
	int has_errors;
	int error_count;
	uint8_t syndrome[self->t];
	uint8_t lambda[self->t/2+1], prev_lambda[self->t/2+1], tmp[self->t/2+1];
	uint8_t lambda_root[self->t/2], error_pos[self->t/2];
	uint8_t omega[self->t], lambda_prime[self->t/2];
	uint8_t num, den, fcr;

	/* Compute syndromes */
	has_errors = 0;
	for (i=0; i<self->t; i++) {
		syndrome[i] = poly_eval(data, self->zeroes[i], self->n, self->alpha, self->logtable, self->n);
		has_errors |= syndrome[i];
	}
	if (!has_errors) {
		return 0;
	}



	/* Berlekamp-Massey algorithm */
	memset(lambda, 0, sizeof(lambda));
	memset(prev_lambda, 0, sizeof(prev_lambda));
	lambda_deg = 0;
	prev_delta = 1;
	lambda[0] = prev_lambda[0] = 1;
	m = 1;

	for (n=0; n<self->t; n++) {
		delta = syndrome[n];
		for (i=1; i<=lambda_deg; i++) {
			delta ^= gfmul(syndrome[n-i], lambda[i], self->alpha, self->logtable, self->n);
		}

		if (delta == 0) {
			m++;
		} else if (2*lambda_deg <= n) {
			for (i=0; i<self->t/2+1; i++) {
				tmp[i] = lambda[i];
			}
			for (i=m; i<self->t/2+1; i++) {
				lambda[i] ^= gfmul(
						gfdiv(delta, prev_delta, self->alpha, self->logtable, self->n),
						prev_lambda[i-m],
						self->alpha, self->logtable, self->n
						);
			}
			for (i=0; i<self->t/2+1; i++) {
				prev_lambda[i] = tmp[i];
			}

			prev_delta = delta;
			lambda_deg = n + 1 - lambda_deg;
			m = 1;
		} else {
			for (i=m; i<self->t/2+1; i++) {
				lambda[i] ^= gfmul(gfdiv(delta, prev_delta, self->alpha, self->logtable, self->n), prev_lambda[i-m], self->alpha, self->logtable, self->n);
			}
			m++;
		}
	}

	/* Roots bruteforcing */
	error_count = 0;
	for (i=1; i<=self->n && error_count < lambda_deg; i++) {
		if (poly_eval(lambda, i, self->t/2+1, self->alpha, self->logtable, self->n) == 0) {
			lambda_root[error_count] = i;
			error_pos[error_count] = self->logtable[self->gaproots[gfdiv(1, i, self->alpha, self->logtable, self->n)]];
			error_count++;
		}
	}

	if (error_count != lambda_deg) {
		return -1;
	}

	poly_mul(omega, syndrome, lambda, self->t, self->t/2+1, self->alpha, self->logtable, self->n);
	poly_deriv(lambda_prime, lambda, self->t/2+1);

	/* Fix errors in the block */
	for (i=0; i<error_count; i++) {
		/* lambda_root[i] = 1/Xi, Xi being the i-th error locator */
		fcr = gfpow(lambda_root[i], self->first_root-1, self->alpha, self->logtable, self->n);
		num = poly_eval(omega, lambda_root[i], self->t, self->alpha, self->logtable, self->n);
		den = poly_eval(lambda_prime, lambda_root[i], self->t/2, self->alpha, self->logtable, self->n);

		data[error_pos[i]] ^= gfdiv(
				gfmul(num, fcr, self->alpha, self->logtable, self->n),
				den,
				self->alpha, self->logtable, self->n
				);
	}

	return error_count;
}

static uint8_t
poly_eval(const uint8_t *poly, uint8_t x, int len, uint8_t *alpha, uint8_t *logtable, int n)
{
	uint8_t ret;

	ret = 0;
	for (len--; len>=0; len--) {
		ret = gfmul(ret, x, alpha, logtable, n) ^ poly[len];
	}

	return ret;
}

static void
poly_deriv(uint8_t *dst, const uint8_t *poly, int len)
{
	int i, j;

	for (i=1; i<len; i++) {
		dst[i-1] = 0;
		for (j=0; j<i; j++) {
			dst[i-1] ^= poly[i];
		}
	}
}

static void
poly_mul(uint8_t *dst, const uint8_t *poly1, const uint8_t *poly2, int len_1, int len_2, uint8_t *alpha, uint8_t *logtable, int n)
{
	int i, j;

	for (i=0; i<len_1; i++) {
		dst[i] = 0;
	}

	for (j=0; j<len_2; j++) {
		for (i=0; i<len_1; i++) {
			if (i+j < len_1) {
				dst[i+j] ^= gfmul(poly1[i], poly2[j], alpha, logtable, n);
			}
		}
	}
}

static uint8_t
gfmul(uint8_t x, uint8_t y, uint8_t *alpha, uint8_t *logtable, int n)
{
	if (x==0 || y==0) {
		return 0;
	}

	return alpha[(logtable[x] + logtable[y]) % n];
}

static uint8_t
gfdiv(uint8_t x, uint8_t y, uint8_t *alpha, uint8_t *logtable, int n)
{
	if (x == 0 || y == 0) {
		return 0;
	}

	return alpha[(logtable[x] - logtable[y] + n) % n];
}

static uint8_t
gfpow(uint8_t x, int exp, uint8_t *alpha, uint8_t *logtable, int n)
{
	return x == 0 ? 0 : alpha[(logtable[x] * exp) % n];
}
/* }}} */
