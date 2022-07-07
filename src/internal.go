package simdcsv

// #cgo CFLAGS: -march=native -Wall -Wextra -O3
// #include <stdlib.h>
// #include "simdcsv.h"
// #include "io_util.h"
import "C"
import (
	"io"
	"unsafe"
)

func parseCSVFromFile(filename string, keys int) C.struct_Records {
	return C.parse_csv_from_file(C.CString(filename), C.int(keys))
}

// Free the buffer.
func freeRecords(rec C.struct_Records) {
	C.free_records(rec)
}

func getNextRow(rec *C.struct_Records) ([]string, error) {
	r := C.get_next_row(rec)
	if r == nil {
		return nil, io.EOF
	}

	cRow := unsafe.Slice(r, int(rec.keys))
	row := make([]string, int(rec.keys))
	for i, j := range cRow {
		row[i] = C.GoString(j)
	}

	C.free_row(r, C.ulong(rec.keys))
	return row, nil
}
