package simdcsv

import "C"

type CSVParser struct {
	keys int
	rec  C.struct_Records
}

// NewCSVParser creates a new parser using k keys.
func NewCSVParser(k int) *CSVParser {
	return &CSVParser{
		keys: k,
	}
}

// Next returns the next row until EOF.
func (c *CSVParser) Next() ([]string, error) {
	return getNextRow(&c.rec)
}

// ParseCSVFile parses and load the file into memory. For now that's the only way to utilize SIMD.
func (c *CSVParser) ParseCSVFile(filename string) error {
	c.rec = parseCSVFromFile(filename, c.keys)
	return nil
}

func (c *CSVParser) FreeRecords() {
	freeRecords(c.rec)
}
