package simdcsv

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func Test_ParseFlow_NFL(t *testing.T) {
	rec := parseCSVFromFile("./../examples/nfl.csv", 13)
	defer freeRecords(rec)
	for i := 0; i < 10000; i++ {
		r, err := getNextRow(&rec)
		if err != nil {
			assert.Equal(t, []string(nil), r)
		}
	}
}

func Test_CSVParser(t *testing.T) {
	parser := NewCSVParser(13)
	parser.ParseCSVFile("./../examples/nfl.csv")
	defer parser.FreeRecords()
	for i := 0; i < 10000; i++ {
		r, err := parser.Next()
		if err != nil {
			assert.Equal(t, []string(nil), r)
		}
	}
}
