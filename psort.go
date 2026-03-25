package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"regexp"
	"sort"
	"strconv"
)

var reWhitespace = regexp.MustCompile(`\s+`)

func main() {
	csv := flag.Bool("csv", false, "Do output as csv rather than formatted for a human to read")
	flag.BoolVar(csv, "c", false, "")
	csvall := flag.Bool("csvall", false, "Do output as csv replacing any/all whitespaces with commas")
	flag.BoolVar(csvall, "a", false, "")
	decimal := flag.Int("decimal", 4, "Specify how many decimal places to compute (default 4)")
	flag.IntVar(decimal, "d", 4, "")
	numeric := flag.Bool("numeric", false, "Sort output by numeric value of data instead of by counts")
	flag.Usage = usage
	flag.Parse()

	counts := make(map[string]int)
	var tot int

	readLines := func(f *os.File) {
		scanner := bufio.NewScanner(f)
		for scanner.Scan() {
			line := scanner.Text()
			counts[line]++
			tot++
		}
	}

	if flag.NArg() > 0 {
		for _, path := range flag.Args() {
			f, err := os.Open(path)
			if err != nil {
				fmt.Fprintln(os.Stderr, err)
				continue
			}
			readLines(f)
			f.Close()
		}
	} else {
		readLines(os.Stdin)
	}

	if tot == 0 {
		return
	}

	keys := make([]string, 0, len(counts))
	for k := range counts {
		keys = append(keys, k)
	}

	if *numeric {
		sort.Slice(keys, func(i, j int) bool {
			vi, _ := strconv.ParseFloat(keys[i], 64)
			vj, _ := strconv.ParseFloat(keys[j], 64)
			return vi < vj
		})
	} else {
		sort.Slice(keys, func(i, j int) bool {
			return counts[keys[i]] < counts[keys[j]]
		})
	}

	lenTot := len(strconv.Itoa(tot))

	for _, key := range keys {
		pout(key, counts[key], tot, lenTot, *decimal, *csv, *csvall)
	}

	if *csv || *csvall {
		fmt.Printf("%8s,%*d\n", "Total:", lenTot, tot)
	} else {
		fmt.Printf("%8s  %*d\n", "Total:", lenTot, tot)
	}
}

func pout(key string, count, tot, lenTot, decimal int, csv, csvall bool) {
	p := (float64(count) / float64(tot)) * 100.0

	outKey := key
	if csvall {
		outKey = reWhitespace.ReplaceAllString(key, ",")
	}

	// Width: 1 digit before decimal if p < 10, else 2
	width := 2
	if p < 10 {
		width = 1
	}
	// Total width = width + 1 (decimal point) + decimal + 1 (%) = width + decimal + 2
	// We use %*.*f by computing total field width manually.
	if csv || csvall {
		fmt.Printf("%*.*f%%,%d,%s\n", width, decimal, p, count, outKey)
	} else {
		if p < 10 {
			fmt.Printf(" %*.*f%%  %*d %s\n", width, decimal, p, lenTot, count, outKey)
		} else {
			fmt.Printf("%*.*f%%  %*d %s\n", width, decimal, p, lenTot, count, outKey)
		}
	}
}

func usage() {
	fmt.Fprintf(os.Stderr, `Usage: %s [--csv] [--csvall] [--decimal=N] [--numeric] [--help] [file ...]
    --csv       : Do output as csv rather than formatted for a human to read
    --csvall    : Do output as csv replacing any/all whitespaces with commas
    --decimal   : Specify how many decimal places to compute (default 4)
    --numeric   : If the data being fed to psort is numeric this flag will sort the
                :  output by that data instead of by counts
    --help      : Output this help info

Synopsis: Takes input and essentially does the equivalent of `+"`"+`sort | uniq -c | sort -n`+"`"+`
with added percentages and total info, default output is nicely formatted for humans to read.
`, os.Args[0])
}
