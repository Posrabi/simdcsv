#ifndef SIMDCSV_API
#define SIMDCSV_API

#include "io_util.h"

typedef struct ParsedCSV {
  uint32_t n_indexes;
  uint32_t *indexes; 
} ParsedCSV;

// Records contain all necessary components to parse.
typedef struct Records {
    ParsedCSV p;
    Buffer b;
    int keys;
    uint32_t current_index;
} Records;

Records parse_csv_from_file(const char* filename, int keys);
void free_records(Records rec);
void free_row(char** row, size_t len);
char** get_next_row(Records* rec);

#endif // SIMDCSV_API
