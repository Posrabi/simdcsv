#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "common_defs.h"
#include "csv_defs.h"
#include "io_util.h"
#include "mem_util.h"
#include "portability.h"
#include "simdcsv.h"

typedef struct simd_input {
#ifdef __AVX2__
  __m256i lo;
  __m256i hi;
#elif defined(__ARM_NEON)
  uint8x16_t i0;
  uint8x16_t i1;
  uint8x16_t i2;
  uint8x16_t i3;
#else
#error "It's called SIMDcsv for a reason, bro"
#endif
} simd_input;

really_inline static simd_input fill_input(const uint8_t * ptr) {
  struct simd_input in;
#ifdef __AVX2__
  in.lo = _mm256_loadu_si256((const __m256i *)(ptr + 0));
  in.hi = _mm256_loadu_si256((const __m256i *)(ptr + 32));
#elif defined(__ARM_NEON)
  in.i0 = vld1q_u8(ptr + 0);
  in.i1 = vld1q_u8(ptr + 16);
  in.i2 = vld1q_u8(ptr + 32);
  in.i3 = vld1q_u8(ptr + 48);
#endif
  return in;
}

// a straightforward comparison of a mask against input. 5 uops; would be
// cheaper in AVX512.
really_inline static uint64_t cmp_mask_against_input(simd_input in, uint8_t m) {
#ifdef __AVX2__
  const __m256i mask = _mm256_set1_epi8(m);
  __m256i cmp_res_0 = _mm256_cmpeq_epi8(in.lo, mask);
  uint64_t res_0 = (uint32_t)(_mm256_movemask_epi8(cmp_res_0));
  __m256i cmp_res_1 = _mm256_cmpeq_epi8(in.hi, mask);
  uint64_t res_1 = _mm256_movemask_epi8(cmp_res_1);
  return res_0 | (res_1 << 32);
#elif defined(__ARM_NEON)
  const uint8x16_t mask = vmovq_n_u8(m); 
  uint8x16_t cmp_res_0 = vceqq_u8(in.i0, mask); 
  uint8x16_t cmp_res_1 = vceqq_u8(in.i1, mask); 
  uint8x16_t cmp_res_2 = vceqq_u8(in.i2, mask); 
  uint8x16_t cmp_res_3 = vceqq_u8(in.i3, mask); 
  return neonmovemask_bulk(cmp_res_0, cmp_res_1, cmp_res_2, cmp_res_3);
#endif
}


// return the quote mask (which is a half-open mask that covers the first
// quote in a quote pair and everything in the quote pair) 
// We also update the prev_iter_inside_quote value to
// tell the next iteration whether we finished the final iteration inside a
// quote pair; if so, this  inverts our behavior of  whether we're inside
// quotes for the next iteration.

really_inline static uint64_t find_quote_mask(simd_input in, uint64_t* prev_iter_inside_quote) {
  uint64_t quote_bits = cmp_mask_against_input(in, '"');

#ifdef __AVX2__
  uint64_t quote_mask = _mm_cvtsi128_si64(_mm_clmulepi64_si128(
      _mm_set_epi64x(0ULL, quote_bits), _mm_set1_epi8(0xFF), 0));
#elif defined(__ARM_NEON)
  uint64_t quote_mask = vmull_p64( -1ULL, quote_bits);
#endif
  quote_mask ^= *prev_iter_inside_quote;

  // right shift of a signed value expected to be well-defined and standard
  // compliant as of C++20,
  // John Regher from Utah U. says this is fine code
  *prev_iter_inside_quote =
      (uint64_t) ((int64_t) quote_mask >> 63);
  return quote_mask;
}


// flatten out values in 'bits' assuming that they are are to have values of idx
// plus their position in the bitvector, and store these indexes at
// base_ptr[base] incrementing base as we go
// will potentially store extra values beyond end of valid bits, so base_ptr
// needs to be large enough to handle this
really_inline static void flatten_bits(uint32_t *base_ptr, uint32_t *base,
                                uint32_t idx, uint64_t bits) {
  if (bits != 0u) {
    uint32_t cnt = hamming(bits);
    uint32_t next_base = *base + cnt;
    base_ptr[*base + 0] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[*base + 1] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[*base + 2] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[*base + 3] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[*base + 4] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[*base + 5] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[*base + 6] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    base_ptr[*base + 7] = idx + trailingzeroes(bits);
    bits = bits & (bits - 1);
    if (cnt > 8) {
      base_ptr[*base + 8] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
      base_ptr[*base + 9] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
      base_ptr[*base + 10] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
      base_ptr[*base + 11] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
      base_ptr[*base + 12] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
      base_ptr[*base + 13] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
      base_ptr[*base + 14] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
      base_ptr[*base + 15] = idx + trailingzeroes(bits);
      bits = bits & (bits - 1);
    }
    if (cnt > 16) {
      (*base) += 16;
      do {
        base_ptr[*base] = idx + trailingzeroes(bits);
        bits = bits & (bits - 1);
        (*base)++;
      } while (bits != 0);
    }
    *base = next_base;
  }
}

//
// This optimization option might be helpful
// When it is OFF:
// $ ./simdcsv ../examples/nfl.csv
// Cycles per byte 0.694172
// GB/s: 4.26847
// When it is ON:
// $ ./simdcsv ../examples/nfl.csv
// Cycles per byte 0.55007
// GB/s: 5.29778
// Explanation: It slightly reduces cache misses, but that's probably irrelevant,
// However, it seems to improve drastically the number of instructions per cycle.
#define SIMDCSV_BUFFERING 
bool find_indexes(const uint8_t * buf, size_t len, ParsedCSV* pcsv) {
  // does the previous iteration end inside a double-quote pair?
  uint64_t prev_iter_inside_quote = 0ULL;  // either all zeros or all ones
#ifdef CRLF
  uint64_t prev_iter_cr_end = 0ULL; 
#endif
  size_t idx = 0;
  uint32_t *base_ptr = pcsv->indexes;
  uint32_t base = 0;
#ifdef SIMDCSV_BUFFERING
  // we do the index decoding in bulk for better pipelining.
#define SIMDCSV_BUFFERSIZE 4 // it seems to be about the sweetspot.
  if(len > 64 * SIMDCSV_BUFFERSIZE) {
    uint64_t fields[SIMDCSV_BUFFERSIZE];
    for (; idx < len - 64 * SIMDCSV_BUFFERSIZE + 1; idx += 64 * SIMDCSV_BUFFERSIZE) {
      for(size_t b = 0; b < SIMDCSV_BUFFERSIZE; b++){
        size_t internal_idx = 64 * b + idx;
#ifndef _MSC_VER
        __builtin_prefetch(buf + internal_idx + 128);
#endif
        simd_input in = fill_input(buf+internal_idx);
        uint64_t quote_mask = find_quote_mask(in, &prev_iter_inside_quote);
        uint64_t sep = cmp_mask_against_input(in, ',');
#ifdef CRLF
        uint64_t cr = cmp_mask_against_input(in, 0x0d);
        uint64_t cr_adjusted = (cr << 1) | prev_iter_cr_end;
        uint64_t lf = cmp_mask_against_input(in, 0x0a);
        uint64_t end = lf & cr_adjusted;
        prev_iter_cr_end = cr >> 63;
#else
        uint64_t end = cmp_mask_against_input(in, 0x0a);
#endif
        fields[b] = (end | sep) & ~quote_mask;
      }
      for(size_t b = 0; b < SIMDCSV_BUFFERSIZE; b++){
        size_t internal_idx = 64 * b + idx;
        flatten_bits(base_ptr, &base, internal_idx, fields[b]);
      }
    }
  }
  // tail end will be unbuffered
#endif // SIMDCSV_BUFFERING
 for (; idx < len; idx += 64) {
#ifndef _MSC_VER
      __builtin_prefetch(buf + idx + 128);
#endif
      simd_input in = fill_input(buf+idx);
      uint64_t quote_mask = find_quote_mask(in, &prev_iter_inside_quote);
      uint64_t sep = cmp_mask_against_input(in, ',');
#ifdef CRLF
      uint64_t cr = cmp_mask_against_input(in, 0x0d);
      uint64_t cr_adjusted = (cr << 1) | prev_iter_cr_end;
      uint64_t lf = cmp_mask_against_input(in, 0x0a);
      uint64_t end = lf & cr_adjusted;
      prev_iter_cr_end = cr >> 63;
#else
      uint64_t end = cmp_mask_against_input(in, 0x0a);
#endif
    // note - a bit of a high-wire act here with quotes
    // we can't put something inside the quotes with the CR
    // then outside the quotes with LF so it's OK to "and off"
    // the quoted bits here. Some other quote convention would
    // need to be thought about carefully
      uint64_t field_sep = (end | sep) & ~quote_mask;
      flatten_bits(base_ptr, &base, idx, field_sep);
  }
#undef SIMDCSV_BUFFERSIZE
  pcsv->n_indexes = base;
  return true;
}

// ---------------- 

Records parse_csv_from_file(const char* filename, int keys) {
  Buffer b = get_corpus(filename, CSV_PADDING);

  ParsedCSV pcsv;
  pcsv.indexes = calloc(b.size, sizeof (uint32_t));
  if (pcsv.indexes == 0) {
    puts("You're running out of memory");
    exit(1);
  }
  
  find_indexes(b.buf, b.size, &pcsv);

  printf("indexes: %u, size: %lu\n", pcsv.n_indexes, b.size);
  
  Records rec = {.p = pcsv, .b = b, .keys = keys, .current_index = keys - 1}; // skip over the header, subtract 1 as the first index is skipped
  return rec;
}

// Get the next "row" limited by the number of keys. TODO: Think of a different, more reliable solution.
char** get_next_row(Records* rec) {
  if (rec->current_index >= rec->p.n_indexes - 1) return 0;

  char** row = calloc(rec->keys, sizeof(char*));
  if (!row) perror("calloc error"), exit(EXIT_FAILURE);

  size_t string_size = 0;
  uint32_t cur = 0;
  for (int i = 0; i < rec->keys && rec->current_index < rec->p.n_indexes; i ++) {
    cur = rec->current_index;
    rec->current_index++;
    if (cur != rec->p.n_indexes - 1) string_size = (rec->p.indexes[cur + 1] - rec->p.indexes[cur]) * sizeof(*rec->b.buf);
    else string_size = (rec->b.size - rec->p.indexes[cur]) * sizeof(*rec->b.buf);
     
    row[i] = malloc(string_size);
    memcpy(row[i], &(rec->b.buf[rec->p.indexes[cur] + 1]), string_size - sizeof(*rec->b.buf)); // -1 ignores the last comma
    row[i][string_size-1] = '\0';
  }

  return row;
}

void free_row(char** row, size_t len) {
  for (size_t i = 0; i < len; i ++) {
    free(row[i]);
  }
  free(row);
}

void free_records(Records rec) {
  free(rec.p.indexes);
  aligned_free(rec.b.buf);
}         

// int main(int argc, char* argv[static argc]) {
//   if (optind < 1) {
//     exit(1);
//   }
//   char *f = argv[optind];
//   Records rec = parse_csv_from_file(f, 13);
//   for (size_t j = 0; j < rec.p.n_indexes / 13; j ++) {
//     char** row = get_next_row(&rec);
//     for (size_t i = 0; i < 13; i ++) {
//       printf("%s \n", row[i]);
//     }
//     free_row(row, 13);
//   }
//   free_records(rec);
// }
