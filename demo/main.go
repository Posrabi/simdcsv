package main

import (
	"fmt"
	"io"

	"github.com/Posrabi/simdcsv"
)

// This is a demonstration on how to use the package.

func main() {
	p := simdcsv.NewCSVParser(13)
	p.ParseCSVFile("./../examples/nfl.csv")
	defer p.FreeRecords()
	for i := 0; i < 10000; i++ {
		r, err := p.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			panic(err)
		}
		fmt.Println(r)
	}
}
