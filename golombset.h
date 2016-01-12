/*
 * Copyright (c) 2015 Kazuho Oku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef GOLOMBSET_H
#define GOLOMBSET_H

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>

#define GOLOMBSET_ENCODE_CALC_FIXED_BITS 0x1

typedef struct st_golombset_encoder_t {
    unsigned char *dst;
    unsigned char *dst_max;
    unsigned dst_shift;
    unsigned fixed_bits_length;
    unsigned fixed_bits;
} golombset_encoder_t;

typedef struct st_golombset_decoder_t {
    const unsigned char *src;
    const unsigned char *src_max;
    unsigned src_shift;
    unsigned fixed_bits_length;
    unsigned fixed_bits;
} golombset_decoder_t;

static inline int golombset_encode_bit(golombset_encoder_t *ctx, int bit)
{
    if (ctx->dst_shift == 8) {
        if (++ctx->dst == ctx->dst_max)
            return -1;
        ctx->dst_shift = 0;
    }
    if (ctx->dst_shift == 0)
        *ctx->dst = 0xff;
    if (!bit)
        *ctx->dst &= ~(0x80 >> ctx->dst_shift);
    ++ctx->dst_shift;
    return 0;
}

static inline int golombset_encode_bits(golombset_encoder_t *ctx, uint64_t value, unsigned nbits)
{
    while (nbits != 0)
        if (golombset_encode_bit(ctx, (value >> --nbits) & 1) != 0)
            return -1;
    return 0;
}

static inline int golombset_decode_bit(golombset_decoder_t *ctx)
{
    if (ctx->src_shift == 8) {
        if (++ctx->src == ctx->src_max)
            return -1;
        ctx->src_shift = 0;
    }
    ++ctx->src_shift;
    return (*ctx->src >> (8 - ctx->src_shift)) & 1;
}

static inline int golombset_decode_bits(golombset_decoder_t *ctx, uint64_t *value, unsigned bits)
{
    *value = 0;
    for (; bits != 0; --bits) {
        int bit = golombset_decode_bit(ctx);
        if (bit == -1)
            return -1;
        *value = (*value << 1) | bit;
    }
    return 0;
}

static inline int golombset_encode_value(golombset_encoder_t *ctx, uint64_t value)
{
    /* emit the unary bits */
    unsigned unary_bits = value >> ctx->fixed_bits;
    for (; unary_bits != 0; --unary_bits)
        if (golombset_encode_bit(ctx, 1) != 0)
            return -1;
    if (golombset_encode_bit(ctx, 0) != 0)
        return -1;
    /* emit the rest */
    return golombset_encode_bits(ctx, value, ctx->fixed_bits);
}

static inline int golombset_decode_value(golombset_decoder_t *ctx, uint64_t *value)
{
    int bit;
    uint64_t unary = 0, fixed;

    /* decode the unary bits */
    while (1) {
        if ((bit = golombset_decode_bit(ctx)) == -1)
            return -1;
        if (bit == 0)
            break;
        ++unary;
    }
    /* decode the rest */
    if (golombset_decode_bits(ctx, &fixed, ctx->fixed_bits) != 0)
        return -1;

    *value = (unary << ctx->fixed_bits) | fixed;
    return 0;
}

static inline unsigned golombset_calc_fixed_bits(golombset_encoder_t *ctx, uint64_t max_key, size_t num_keys)
{
    uint64_t delta;
    unsigned fixed_bits;

    if (num_keys == 0)
        return 0;
    delta = max_key / num_keys;
    if (delta < 1)
        return 0;
    fixed_bits = sizeof(unsigned long long) * 8 - __builtin_clzll((unsigned long long)delta) - 1;
    if (fixed_bits >= 1 << ctx->fixed_bits_length)
        fixed_bits = (1 << ctx->fixed_bits_length) - 1;
    return fixed_bits;
}

static inline int golombset_encode(golombset_encoder_t *ctx, const uint64_t *keys, size_t num_keys, int flags)
{
    size_t i;
    uint64_t next_min = 0;

    if ((flags & GOLOMBSET_ENCODE_CALC_FIXED_BITS) != 0)
        ctx->fixed_bits = golombset_calc_fixed_bits(ctx, keys[num_keys - 1], num_keys);

    if (ctx->fixed_bits_length != 0 && golombset_encode_bits(ctx, ctx->fixed_bits, ctx->fixed_bits_length) != 0)
        return -1;

    for (i = 0; i != num_keys; ++i) {
        if (golombset_encode_value(ctx, keys[i] - next_min) != 0)
            return -1;
        next_min = keys[i] + 1;
    }

    ++ctx->dst;
    return 0;
}

static inline int golombset_decode(golombset_decoder_t *ctx, uint64_t *keys, size_t *num_keys, int flags)
{
    size_t index = 0;
    uint64_t next_min = 0;

    if (ctx->fixed_bits_length != 0) {
        uint64_t tmp;
        if (golombset_decode_bits(ctx, &tmp, ctx->fixed_bits_length) != 0)
            return -1;
        ctx->fixed_bits = tmp;
    }

    while (1) {
        uint64_t value;
        if (golombset_decode_value(ctx, &value) != 0)
            break;
        if (index == *num_keys) {
            /* not enough space */
            return -1;
        }
        value += next_min;
        keys[index++] = value;
        next_min = value + 1;
    }
    *num_keys = index;
    return 0;
}

#endif
