#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "rs.h"
#include "utils.h"

static uint8_t gfmul(uint8_t x, uint8_t y, const uint8_t *alpha, const uint8_t *logtable, int n);
static uint8_t gfdiv(uint8_t x, uint8_t y, const uint8_t *alpha, const uint8_t *logtable, int n);
static uint8_t gfpow(uint8_t x, int exp, const uint8_t *alpha, const uint8_t *logtable, int n);
static int rs_init_internal(RSDecoder *d, int n, int k, unsigned gen_poly);
static void poly_deriv(uint8_t *dst, const uint8_t *poly, int len);
static uint8_t poly_eval(const uint8_t *poly, uint8_t x, int len, const uint8_t *alpha, const uint8_t *logtable, int n);
static void poly_mul(uint8_t *dst, const uint8_t *poly1, const uint8_t *poly2, int len_1, int len_2, const uint8_t *alpha, const uint8_t *logtable, int n);

int
rs_init(RSDecoder *d, int n, int k, unsigned gen_poly, uint8_t first_root, int root_skip)
{
	const int t = n - k;
	int i, exp;
	int ret;

	ret = rs_init_internal(d, n, k, gen_poly);

	d->first_root = first_root;

	/* Compute polynomial roots */
	for (i=0; i<t; i++) {
		exp = ((i + first_root) * root_skip) % n;
		d->zeroes[i] = d->alpha[exp];
	}

	/* Initialize the gap'th log table */
	for (i=0; i<n+1; i++) {
		d->gaproots[gfpow(i, root_skip, d->alpha, d->logtable, n)] = i;
	}

	return ret;
}

int
bch_init(RSDecoder *d, int n, int k, unsigned gen_poly, uint8_t *roots)
{
	const int t = n - k;
	int i;
	int ret;

	ret = rs_init_internal(d, n, k, gen_poly);
	memcpy(d->zeroes, roots, t * sizeof(roots[0]));
	d->first_root = -1;

	/* Initialize the gap'th log table */
	for (i=0; i<n+1; i++) {
		d->gaproots[i] = i;
	}

	return ret;
}

int
rs_init_internal(RSDecoder *d, int n, int k, unsigned gen_poly)
{
	int i;
	unsigned tmp;

	if (!(d->alpha = malloc(n+1))) return 1;
	if (!(d->logtable = malloc(n+1))) return 1;
	if (!(d->zeroes = malloc(n+1))) return 1;
	if (!(d->gaproots = malloc(n+1))) return 1;

	d->n = n;
	d->k = k;

	/* Initialize alpha and logtable */
	d->alpha[0] = 1;
	d->logtable[1] = 0;

	for (i=1; i<n+1; i++) {
		tmp = (int)d->alpha[i-1] << 1;
		tmp = (tmp >= (unsigned)n+1 ? tmp ^ gen_poly : tmp);
		d->alpha[i] = tmp;
		d->logtable[tmp] = i;
	}


	return 0;
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
	const int rs_n = self->n;
	const int rs_k = self->k;
	const int rs_t = rs_n - rs_k;
	const int rs_t2 = rs_t / 2;
	const uint8_t *alpha = self->alpha;
	const uint8_t *logtable = self->logtable;
	const uint8_t *gaproots = self->gaproots;
	const uint8_t *zeroes = self->zeroes;


	int i, m, n, delta, prev_delta;
	int lambda_deg;
	int has_errors;
	int error_count;
	uint8_t syndrome[rs_t];
	uint8_t lambda[rs_t2+1], prev_lambda[rs_t2+1], tmp[rs_t2+1];
	uint8_t lambda_root[rs_t2], error_pos[rs_t2];
	uint8_t omega[rs_t], lambda_prime[rs_t2];
	uint8_t num, den, fcr;

	/* Compute syndromes */
	has_errors = 0;
	for (i=0; i<rs_t; i++) {
		syndrome[i] = poly_eval(data, zeroes[i], rs_n, alpha, logtable, rs_n);
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

	for (n=0; n<rs_t; n++) {
		delta = syndrome[n];
		for (i=1; i<=lambda_deg; i++) {
			delta ^= gfmul(syndrome[n-i], lambda[i], alpha, logtable, rs_n);
		}

		if (delta == 0) {
			m++;
		} else if (2*lambda_deg <= n) {
			for (i=0; i<rs_t2+1; i++) {
				tmp[i] = lambda[i];
			}
			for (i=m; i<rs_t2+1; i++) {
				lambda[i] ^= gfmul(
						gfdiv(delta, prev_delta, alpha, logtable, rs_n),
						prev_lambda[i-m],
						alpha, logtable, rs_n
						);
			}
			for (i=0; i<rs_t2+1; i++) {
				prev_lambda[i] = tmp[i];
			}

			prev_delta = delta;
			lambda_deg = n + 1 - lambda_deg;
			m = 1;
		} else {
			for (i=m; i<rs_t2+1; i++) {
				lambda[i] ^= gfmul(
						gfdiv(delta, prev_delta, alpha, logtable, rs_n),
						prev_lambda[i-m],
						alpha, logtable, rs_n);
			}
			m++;
		}
	}

	/* Roots bruteforcing */
	error_count = 0;
	for (i=1; i<=rs_n && error_count < lambda_deg; i++) {
		if (poly_eval(lambda, i, lambda_deg+1, alpha, logtable, rs_n) == 0) {
			lambda_root[error_count] = i;
			error_pos[error_count] = logtable[gaproots[gfdiv(1, i, alpha, logtable, rs_n)]];
			error_count++;
		}
	}

	if (error_count != lambda_deg) {
		return -1;
	}

	poly_mul(omega, syndrome, lambda, rs_t, rs_t2+1, alpha, logtable, rs_n);
	poly_deriv(lambda_prime, lambda, rs_t2+1);

	/* Fix errors in the block */
	for (i=0; i<error_count; i++) {
		if (self->first_root >= 0) {
			/* lambda_root[i] = 1/Xi, Xi being the i-th error locator */
			fcr = gfpow(lambda_root[i], (self->first_root - 1 + rs_n) % rs_n, alpha, logtable, rs_n);
			num = poly_eval(omega, lambda_root[i], rs_t, alpha, logtable, rs_n);
			den = poly_eval(lambda_prime, lambda_root[i], rs_t2, alpha, logtable, rs_n);

			data[error_pos[i]] ^= gfdiv(
					gfmul(num, fcr, alpha, logtable, rs_n),
					den,
					alpha, logtable, rs_n
					);
		} else {
			/* BCH code */
			data[error_pos[i]] ^= 0x1;
		}
	}

	return error_count;
}

static uint8_t
poly_eval(const uint8_t *poly, uint8_t x, int len, const uint8_t *alpha, const uint8_t *logtable, int n)
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
poly_mul(uint8_t *dst, const uint8_t *poly1, const uint8_t *poly2, int len_1, int len_2, const uint8_t *alpha, const uint8_t *logtable, int n)
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
gfmul(uint8_t x, uint8_t y, const uint8_t *alpha, const uint8_t *logtable, int n)
{
	if (x==0 || y==0) {
		return 0;
	}

	return alpha[(logtable[x] + logtable[y]) % n];
}

static uint8_t
gfdiv(uint8_t x, uint8_t y, const uint8_t *alpha, const uint8_t *logtable, int n)
{
	if (x == 0 || y == 0) {
		return 0;
	}

	return alpha[(logtable[x] - logtable[y] + n) % n];
}

static uint8_t
gfpow(uint8_t x, int exp, const uint8_t *alpha, const uint8_t *logtable, int n)
{
	return x == 0 ? 0 : alpha[(logtable[x] * exp) % n];
}
/* }}} */
