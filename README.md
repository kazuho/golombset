golombset
===

Golombset is a pure-C, header-file-only implementation of Golomb compressed set, which is an compressed form of [Bloom filter](https://en.wikipedia.org/Bloom_filter).

It compresses every zero-range of Bloom filter (e.g. `0000...1`) using [Golomb coding](https://en.wikipedia.org/wiki/Golomb_coding).
Please refer to [Golomb-coded sets: smaller than Bloom filters](http://giovanni.bajo.it/post/47119962313/golomb-coded-sets-smaller-than-bloom-filters) for more information about the algorithm.

Encode
---

```
// buffer to store the encoded data
char buf[1024];

// setup encoder context; dst, dst_max, fixed_bits_length MUST be set
golombset_encoder_t ctx;
memset(&ctx, 0, sizeof(ctx));
ctx.dst = buf;
ctx.dst_max = buf + siezeof(buf);
ctx.fixed_bits_length = 5; // number of bits used to store `fixed_bits`

// encode values (values must be pre-sorted)
int ret = golombset_encode(&ctx, values, num_values, 0);
```

The encode function returns zero on success.
The function returns a non-zero value if size of the buffer is too small; applications must repeat the entire process with a larger buffer to generate the encoded data.

Decode
---

```
// buffer to srote the decoded data
unsigned values[1024];
size_t num_values = sizeof(values) / sizeof(values[0]);

// setup decoder context; src, src_max, fixd_bits_length MUST be set
golombset_decoder_t ctx;
memset(&ctx, 0, sizeof(ctx));
ctx.src = encoded_bytes;
ctx.src_max = length_of_encoded_bytes;
ctx.fixed_bits_length = 5;

// decode the values
int ret = golombset_decode(&ctx, &values, &num_values, 0);
```

The decode function returns zero on success.
The function returns a non-zero value if size of the buffer is too small; applications must repeat the entire process with a larger buffer to generate the decoded data.
