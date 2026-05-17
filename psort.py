#!/usr/bin/env python3
"""psort — sort | uniq -c | sort -n with percentages."""

import argparse
import re
import sys


def pout(key, count, tot, len_tot, decimal, csv_mode, csvall):
    p = (count / tot) * 100.0
    out_key = re.sub(r'\s+', ',', key) if csvall else key
    p_str = f"{p:.{decimal}f}"
    if csv_mode or csvall:
        print(f"{p_str}%,{count},{out_key}")
    else:
        if p < 10:
            print(f" {p_str}%  {count:{len_tot}d} {out_key}")
        else:
            print(f"{p_str}%  {count:{len_tot}d} {out_key}")


def main():
    parser = argparse.ArgumentParser(
        prog='psort',
        add_help=False,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        usage='%(prog)s [options] [file ...]',
        description=(
            "Synopsis: Takes input and essentially does the equivalent of\n"
            "`sort | uniq -c | sort -n` with added percentages and total info,\n"
            "default output is nicely formatted for humans to read."
        ),
    )
    parser.add_argument('-c', '--csv',      action='store_true', help='Do output as csv rather than formatted for a human to read')
    parser.add_argument('-a', '--csvall',   action='store_true', help='Do output as csv replacing any/all whitespaces with commas')
    parser.add_argument('-d', '--decimal',  type=int, default=4, metavar='N', help='Specify how many decimal places to compute (default 4)')
    parser.add_argument(      '--numeric',  action='store_true', help='Sort output by numeric value of data instead of by counts')
    parser.add_argument('-m', '--min',      type=int, default=0, metavar='N', help='Minimum count; entries below this are excluded')
    parser.add_argument('-x', '--max',      type=int, default=0, metavar='N', help='Maximum count; entries above this are excluded')
    parser.add_argument('-f', '--fullperc', action='store_true', help='With min/max, compute %% based on full count rather than filtered count')
    parser.add_argument('-i', '--ignore',   action='store_true', help='With min/max, suppress the [excluded] summary line')
    parser.add_argument('-v', '--verbose',  action='store_true', help='With min/max, print key/count totals before and after filtering')
    parser.add_argument('-h', '--help',     action='help',       help='Output this help info')
    parser.add_argument('files', nargs='*', metavar='file')

    args = parser.parse_args()

    if args.fullperc and args.min == 0 and args.max == 0:
        print("ERROR: cannot use fullperc without either min or max", file=sys.stderr)
        parser.print_usage(sys.stderr)
        sys.exit(1)

    counts = {}
    tot = 0

    def read_lines(f):
        nonlocal tot
        for line in f:
            line = line.rstrip('\n').rstrip('\r')
            counts[line] = counts.get(line, 0) + 1
            tot += 1

    if args.files:
        for path in args.files:
            try:
                with open(path) as f:
                    read_lines(f)
            except OSError as e:
                print(e, file=sys.stderr)
    else:
        read_lines(sys.stdin)

    if tot == 0:
        return

    if args.min > 0 or args.max > 0:
        if args.verbose:
            print(f"# keys/counts before: {len(counts)} / {sum(counts.values())}", file=sys.stderr)

        excluded = 0
        for key in list(counts):
            count = counts[key]
            if (args.min > 0 and count < args.min) or (args.max > 0 and count > args.max):
                excluded += count
                if not args.fullperc:
                    tot -= count
                del counts[key]

        if excluded > 0 and not args.ignore:
            counts['[excluded]'] = counts.get('[excluded]', 0) + excluded

        if args.verbose:
            print(f"# keys/counts after: {len(counts)} / {sum(counts.values())}", file=sys.stderr)

    if tot == 0:
        return

    if args.numeric:
        def numeric_key(k):
            try:
                return float(k)
            except ValueError:
                return 0.0
        keys = sorted(counts, key=numeric_key)
    else:
        keys = sorted(counts, key=lambda k: counts[k])

    len_tot = len(str(tot))

    for key in keys:
        pout(key, counts[key], tot, len_tot, args.decimal, args.csv, args.csvall)

    if args.csv or args.csvall:
        print(f"{'Total:':>8},{tot:{len_tot}d}")
    else:
        print(f"{'Total:':>8}  {tot:{len_tot}d}")


if __name__ == '__main__':
    main()
