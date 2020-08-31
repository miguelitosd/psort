psort
=====

psort - A small perl script I originally wrote a few years back to replace/improve upon the regular usage of the chain of commands: `sort | uniq -c | sort -n` that I find I used to use all the time. Not only is it a quick perl script that avoids a couple fork calls, but I added some nice output with percentages and the total on the bottom.

Example output getting the source IP when blocked by iptables shown in dmesg:
```
$ dmesg -T | awk '{for (i=1;i<=NF;i++) { split($i,a,"="); if (a[1]=="SRC") { print a[2] }}}' | psort
 0.7014%    7 171.235.51.39
 1.1022%   11 62.2.188.198
 1.1022%   11 97.80.66.232
 1.4028%   14 31.184.199.114
 1.4028%   14 5.188.84.119
 1.5030%   15 144.217.72.135
 1.6032%   16 119.29.169.136
91.1824%  910 45.10.88.238
  Total:  998
```

Help output:
```
$ ./psort --help
Usage: ./psort [--csv] [--csvall] [--help]
    --csv       : Do output as csv rather than formatted for a human to read
    --csvall    : Do output as csv replacing any/all whitespaces with commas
    --decimnal  : Specify how many decimal places to compute (default 4)
    --help      : Output this help info

Synopsis: Takes input and essentially does the equivalent of `sort | uniq -c | sort -n`
with added percentages and total info, default output is nicely formatted for humans to read.
```
