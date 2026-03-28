psort
=====

Initially my original perl script I wrote ages ago that not only combines the commands:
`| sort | uniq -c | sort -n`
into one script, but includes percentages (the thing I initially wrote it to get) and
aligns columns and such.

e.g.
```
❯ find Movies -type f -name \*.mp4 -o -name \*.m4v | awk -F\. '{print $NF}' | ./psort
 2.8649%    42 mp4
97.1351%  1424 m4v
  Total:  1466
```

Later I kept on improving with some options that came in handy.

## Usage / Help
```
❯ ./psort --help
Usage: ./psort [--csv] [--csvall] [--help]
    --csv       : Do output as csv rather than formatted for a human to read
    --csvall    : Do output as csv replacing any/all whitespaces with commas
    --decimal  : Specify how many decimal places to compute (default 4)
    --numeric   : If the data being fed to psort is numeric this flag will sort the
                :  output by that data instead of by counts
    --min       : Give a minimum count below which we should throw out those, to
                   allow trimming smaller values (compute % based upon what's left)
    --max       : Give a maximum count above which we should throw out those, to
                   allow trimming larger values  (compute % based upon what's left)
    --fullperc  : Only with a min/max, still output the % based upon the full count
                   vs only what matched the filter
    --ignore    : When using min/max and fullperc, don't include the [deleted] line
                   that shows us the full 100%
    --help      : Output this help info

Synopsis: Takes input and essentially does the equivalent of `sort | uniq -c | sort -n` 
with added percentages and total info, default output is nicely formatted for humans to read.
```

### Ports
Playing with Claude, I already ported to C to see how it would do.  Actually worked pretty well out of the box.
Probably will play with other language ports as a way to get some experience.
