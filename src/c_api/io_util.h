#ifndef SIMDCSV_JSONIOUTIL_H
#define SIMDCSV_JSONIOUTIL_H

#include <stdlib.h>
#include <stdint.h>
#include "common_defs.h"

// Wrapper struct around u_char to keep track of its size. Must FREE after .buf field after use.
typedef struct Buffer {
    u_char* buf;
    size_t size;
} Buffer;

// low-level function to allocate memory with padding so we can read passed the "length" bytes
// safely.
// if you must provide a pointer to some data, create it with this function:
// length is the max. size in bytes of the string
// caller is responsible to free the memory (free(...))
uint8_t * allocate_padded_buffer(size_t length, size_t padding);


// load a file in memory...
// get a corpus; pad out to some distance so we can use SIMD, unroll, etc
// throws exceptions in case of failure
// first element of the pair is a string (null terminated)
// whereas the second element is the length.
// caller is responsible to free (aligned_free((void*)result.data())))
// 
// throws an exception if the file cannot be opened, use try/catch
//      try {
//        p = get_corpus(filename);
//      } catch (const std::exception& e) { 
//        aligned_free((void*)p.data());
//        std::cout << "Could not load the file " << filename << std::endl;
//      }
Buffer get_corpus(const char* filename, size_t padding);

#endif