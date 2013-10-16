#ifndef HT_HASH_H
#define HT_HASH_H

/*
 * ht-hash.h
 *
 * Copyright (C) 2013  Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program for any
 * purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is
 * granted, provided the above notices are retained, and a notice that
 * the code was modified is included with the above copyright notice.
 */

#include <stdint.h>
#include <urcu/compiler.h>

/*
 * Hash function
 * Source: http://burtleburtle.net/bob/c/lookup3.c
 * Originally Public Domain
 */

#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

#define mix(a, b, c) \
do { \
	a -= c; a ^= rot(c,  4); c += b; \
	b -= a; b ^= rot(a,  6); a += c; \
	c -= b; c ^= rot(b,  8); b += a; \
	a -= c; a ^= rot(c, 16); c += b; \
	b -= a; b ^= rot(a, 19); a += c; \
	c -= b; c ^= rot(b,  4); b += a; \
} while (0)

#define final(a, b, c) \
{ \
	c ^= b; c -= rot(b, 14); \
	a ^= c; a -= rot(c, 11); \
	b ^= a; b -= rot(a, 25); \
	c ^= b; c -= rot(b, 16); \
	a ^= c; a -= rot(c,  4);\
	b ^= a; b -= rot(a, 14); \
	c ^= b; c -= rot(b, 24); \
}

static inline
void hashword2(
	const uint32_t *k,	/* the key, an array of uint32_t values */
	size_t length,		/* the length of the key, in uint32_ts */
	uint32_t *pc,		/* IN: seed OUT: primary hash value */
	uint32_t *pb)		/* IN: more seed OUT: secondary hash value */
{
	uint32_t a, b, c;

	/* Set up the internal state */
	a = b = c = 0xdeadbeef + ((uint32_t) (length << 2)) + *pc;
	c += *pb;

	/*----------------------------------------- handle most of the key */
	while (length > 3) {
		a += k[0];
		b += k[1];
		c += k[2];
		mix(a, b, c);
		length -= 3;
		k += 3;
	}

	/*----------------------------------- handle the last 3 uint32_t's */
	switch (length) {	/* all the case statements fall through */
	case 3: c += k[2];
	case 2: b += k[1];
	case 1: a += k[0];
		final(a, b, c);
	case 0:			/* case 0: nothing left to add */
		break;
	}
	/*---------------------------------------------- report the result */
	*pc = c;
	*pb = b;
}

static inline
unsigned long hash_u64(const uint64_t *_key, unsigned long seed)
{
	union {
		uint64_t v64;
		uint32_t v32[2];
	} v;
	union {
		uint64_t v64;
		uint32_t v32[2];
	} key;

	v.v64 = (uint64_t) seed;
	key.v64 = (uint64_t) _key;
	hashword2(key.v32, 2, &v.v32[0], &v.v32[1]);
#if (CAA_BITS_PER_LONG == 32)
	return (unsigned long) v.v64 | (unsigned long) (v.v64 >> 32);
#else
	return (unsigned long) v.v64;
#endif
}

#endif /* HT_HASH_H */
