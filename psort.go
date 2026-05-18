package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"unsafe"
)

var reWhitespace = regexp.MustCompile(`\s+`)

type winsize struct{ Rows, Cols, X, Y uint16 }

func termWidth() int {
	if s := os.Getenv("COLUMNS"); s != "" {
		if n, err := strconv.Atoi(s); err == nil && n > 0 {
			return n
		}
	}
	var ws winsize
	r, _, _ := syscall.Syscall(syscall.SYS_IOCTL, 1, 0x5413, uintptr(unsafe.Pointer(&ws)))
	if r == 0 && ws.Cols > 0 {
		return int(ws.Cols)
	}
	return 80
}

func makeBar(p float64, barWidth int) string {
	filled := int(p / 100.0 * float64(barWidth))
	if filled > barWidth {
		filled = barWidth
	}
	return "[" + strings.Repeat("#", filled) + strings.Repeat(" ", barWidth-filled) + "]"
}

func main() {
	csv := flag.Bool("csv", false, "Do output as csv rather than formatted for a human to read")
	flag.BoolVar(csv, "c", false, "")
	csvall := flag.Bool("csvall", false, "Do output as csv replacing any/all whitespaces with commas")
	flag.BoolVar(csvall, "a", false, "")
	decimal := flag.Int("decimal", 4, "Specify how many decimal places to compute (default 4)")
	flag.IntVar(decimal, "d", 4, "")
	numeric := flag.Bool("numeric", false, "Sort output by numeric value of data instead of by counts")
	mincount := flag.Int("min", 0, "Minimum count; entries below this are excluded")
	flag.IntVar(mincount, "m", 0, "")
	maxcount := flag.Int("max", 0, "Maximum count; entries above this are excluded")
	flag.IntVar(maxcount, "x", 0, "")
	fullperc := flag.Bool("fullperc", false, "With min/max, compute % based on full count rather than filtered count")
	flag.BoolVar(fullperc, "f", false, "")
	ignoreExcluded := flag.Bool("ignore", false, "With min/max, suppress the [excluded] summary line")
	flag.BoolVar(ignoreExcluded, "i", false, "")
	verbose := flag.Bool("verbose", false, "With min/max, print key/count totals before and after filtering")
	flag.BoolVar(verbose, "v", false, "")
	reverse := flag.Bool("reverse", false, "Reverse the sort order (highest count/value first)")
	flag.BoolVar(reverse, "r", false, "")
	histogram := flag.Bool("histogram", false, "Show a bar histogram scaled to terminal width")
	flag.BoolVar(histogram, "H", false, "")
	flag.Usage = usage
	flag.Parse()

	if *fullperc && *mincount == 0 && *maxcount == 0 {
		fmt.Fprintln(os.Stderr, "ERROR: cannot use fullperc without either min or max")
		usage()
		os.Exit(1)
	}

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

	if *mincount > 0 || *maxcount > 0 {
		if *verbose {
			tc := 0
			for _, v := range counts {
				tc += v
			}
			fmt.Fprintf(os.Stderr, "# keys/counts before: %d / %d\n", len(counts), tc)
		}

		excluded := 0
		for key, count := range counts {
			drop := (*mincount > 0 && count < *mincount) || (*maxcount > 0 && count > *maxcount)
			if drop {
				excluded += count
				if !*fullperc {
					tot -= count
				}
				delete(counts, key)
			}
		}

		if excluded > 0 && !*ignoreExcluded {
			counts["[excluded]"] += excluded
		}

		if *verbose {
			tc := 0
			for _, v := range counts {
				tc += v
			}
			fmt.Fprintf(os.Stderr, "# keys/counts after: %d / %d\n", len(counts), tc)
		}
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
			if *reverse {
				return vi > vj
			}
			return vi < vj
		})
	} else {
		sort.Slice(keys, func(i, j int) bool {
			if *reverse {
				return counts[keys[i]] > counts[keys[j]]
			}
			return counts[keys[i]] < counts[keys[j]]
		})
	}

	lenTot := len(strconv.Itoa(tot))

	barWidth := 0
	if *histogram && !*csv && !*csvall {
		maxKey := 0
		for _, k := range keys {
			if len(k) > maxKey {
				maxKey = len(k)
			}
		}
		barWidth = termWidth() - *decimal - lenTot - maxKey - 11
		if barWidth < 10 {
			barWidth = 10
		}
	}

	for _, key := range keys {
		pout(key, counts[key], tot, lenTot, *decimal, *csv, *csvall, *histogram, barWidth)
	}

	if *histogram && !*csv && !*csvall {
		fmt.Printf("%*s  %*d\n", barWidth+3+*decimal+4, "Total:", lenTot, tot)
	} else if *csv || *csvall {
		fmt.Printf("%8s,%*d\n", "Total:", lenTot, tot)
	} else {
		fmt.Printf("%8s  %*d\n", "Total:", lenTot, tot)
	}
}

func pout(key string, count, tot, lenTot, decimal int, csv, csvall, histogram bool, barWidth int) {
	p := (float64(count) / float64(tot)) * 100.0

	outKey := key
	if csvall {
		outKey = reWhitespace.ReplaceAllString(key, ",")
	}

	width := 2
	if p < 10 {
		width = 1
	}
	if csv || csvall {
		fmt.Printf("%*.*f%%,%d,%s\n", width, decimal, p, count, outKey)
	} else if histogram {
		var percStr string
		if p < 10 {
			percStr = " " + fmt.Sprintf("%.*f%%", decimal, p)
		} else {
			percStr = fmt.Sprintf("%.*f%%", decimal, p)
		}
		fmt.Printf("%s %s  %*d %s\n", makeBar(p, barWidth), percStr, lenTot, count, outKey)
	} else {
		if p < 10 {
			fmt.Printf(" %*.*f%%  %*d %s\n", width, decimal, p, lenTot, count, outKey)
		} else {
			fmt.Printf("%*.*f%%  %*d %s\n", width, decimal, p, lenTot, count, outKey)
		}
	}
}

func usage() {
	fmt.Fprintf(os.Stderr, `Usage: %s [options] [file ...]
    --csv, -c    : Do output as csv rather than formatted for a human to read
    --csvall, -a : Do output as csv replacing any/all whitespaces with commas
    --decimal, -d: Specify how many decimal places to compute (default 4)
    --numeric    : If the data being fed to psort is numeric this flag will sort the
                 :  output by that data instead of by counts
    --min, -m    : Give a minimum count below which we should throw out those, to
                    allow trimming smaller values (compute %% based upon what's left)
    --max, -x    : Give a maximum count above which we should throw out those, to
                    allow trimming larger values  (compute %% based upon what's left)
    --fullperc, -f: Only with a min/max, still output the %% based upon the full count
                    vs only what matched the filter
    --ignore, -i : When using min/max, don't include the [excluded] line
                    that shows us the full 100%%
    --verbose, -v: When using min/max, print key/count totals before and after filtering
    --reverse,-r : Reverse the sort order (highest count/value first)
    --histogram,-H: Show a bar histogram scaled to terminal width
    --help, -h   : Output this help info

Synopsis: Takes input and essentially does the equivalent of `+"`"+`sort | uniq -c | sort -n`+"`"+`
with added percentages and total info, default output is nicely formatted for humans to read.
`, os.Args[0])
}
