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

#define GOLOMBSET_ENCODE_CALC_FIXED_BITS 1

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
        *ctx->dst = 0xff;
        ctx->dst_shift = 0;
    }
    if (!bit)
        *ctx->dst &= ~(0x80 >> ctx->dst_shift);
    ++ctx->dst_shift;
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

static inline int golombset_encode_value(golombset_encoder_t *ctx, unsigned value)
{
    /* emit the unary bits */
    unsigned unary_bits = value >> ctx->fixed_bits;
    for (; unary_bits != 0; --unary_bits)
        if (golombset_encode_bit(ctx, 1) != 0)
            return -1;
    if (golombset_encode_bit(ctx, 0) != 0)
        return -1;
    /* emit the rest */
    unsigned shift = ctx->fixed_bits;
    while (shift != 0) {
        if (golombset_encode_bit(ctx, (value >> --shift) & 1) != 0)
            return -1;
    }

    return 0;
}

static inline int golombset_decode_value(golombset_decoder_t *ctx, unsigned *value)
{
    int bit;
    *value = 0;

    /* decode the unary bits */
    while (1) {
        if ((bit = golombset_decode_bit(ctx)) == -1)
            return -1;
        if (bit == 0)
            break;
        *value += 1 << ctx->fixed_bits;
    }
    /* decode the rest */
    unsigned shift = ctx->fixed_bits;
    while (shift != 0) {
        if ((bit = golombset_decode_bit(ctx)) == -1)
            return -1;
        *value |= bit << --shift;
    }

    return 0;
}

static inline unsigned golombset_calc_fixed_bits(golombset_encoder_t *ctx, unsigned max_key, size_t num_keys)
{
    unsigned delta, fixed_bits;

    if (num_keys == 0)
        return 0;
    delta = max_key / num_keys;
    if (delta < 1)
        return 0;
    fixed_bits = sizeof(unsigned) * 8 - __builtin_clz(delta) - 1;
    if (fixed_bits >= 1 << ctx->fixed_bits_length)
        fixed_bits = (1 << ctx->fixed_bits_length) - 1;
    return fixed_bits;
}

static inline int golombset_encode(golombset_encoder_t *ctx, const unsigned *keys, size_t num_keys, int flags)
{
    size_t i;
    unsigned next_min = 0;

    if ((flags & GOLOMBSET_ENCODE_CALC_FIXED_BITS) != 0)
        ctx->fixed_bits = golombset_calc_fixed_bits(ctx, keys[num_keys - 1], num_keys);

    *(unsigned char *)ctx->dst = 0xff;
    for (i = 0; i != ctx->fixed_bits_length; ++i) {
        if (golombset_encode_bit(ctx, (ctx->fixed_bits >> (ctx->fixed_bits_length - 1 - i)) & 1) != 0)
            return -1;
    }
    for (i = 0; i != num_keys; ++i) {
        if (golombset_encode_value(ctx, keys[i] - next_min) != 0)
            return -1;
        next_min = keys[i] + 1;
    }

    ++ctx->dst;
    return 0;
}

static inline int golombset_decode(golombset_decoder_t *ctx, unsigned *keys, size_t *num_keys, int flags)
{
    size_t i, index = 0;
    unsigned next_min = 0;

    for (i = 0; i != ctx->fixed_bits_length; ++i) {
        int bit = golombset_decode_bit(ctx);
        if (bit == -1)
            return -1;
        ctx->fixed_bits = (ctx->fixed_bits << 1) | bit;
    }
    while (1) {
        unsigned value;
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
