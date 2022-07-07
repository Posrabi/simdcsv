#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "io_util.h"
#include "mem_util.h"


uint8_t * allocate_padded_buffer(size_t length, size_t padding) {
    // we could do a simple malloc
    //return (char *) malloc(length + padding);
    // However, we might as well align to cache lines...
    size_t totalpaddedlength = length + padding;
    uint8_t * padded_buffer = (uint8_t *) aligned_malloc(64, totalpaddedlength);
    return padded_buffer;
}

Buffer get_corpus(const char* filename, size_t padding) {
  FILE *fp = fopen(filename, "rb");
  if (fp != 0) {
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    u_char* buf = allocate_padded_buffer(len, padding);
    if(buf == 0) {
      fclose(fp);
      puts("could not allocate memory");
      exit(1);
    }
    rewind(fp);
    size_t readb = fread(buf, 1, len, fp);
    fclose(fp);
    if(readb != len) {
      puts("could not read the data");
      exit(1);
    }
    Buffer b = {.buf = buf, .size = len};
    return b;
  }
  puts("could not load corpus");
  exit(1);
}
