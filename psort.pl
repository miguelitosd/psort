#!/usr/bin/perl -w
use strict;
use Getopt::Long qw(:config no_ignore_case);
use Time::Piece;

my @DATE_FMTS = ('%d %b %Y', '%b %d %Y', '%m/%d/%Y', '%m/%d/%y', '%Y-%m-%d', '%d-%b-%Y');

sub parse_date {
    my $s = shift;
    for my $fmt (@DATE_FMTS) {
        my $t = eval { Time::Piece->strptime($s, $fmt) };
        return $t->epoch if $t;
    }
    return 0;
}
my $tot=0;
my %data=();
my $decimal=4;

GetOptions(
    "csv|c"         => \my $csv,
    "csvall|a"      => \my $csvall,
    "decimal|d=i"   => \$decimal,
    "help|h"        => sub { usage(); exit 0; },
    "numeric"       => \my $numeric,
    "date"          => \my $date,
    "min|m=i"       => \my $mincount,
    "max|x=i"       => \my $maxcount,
    "verbose|v"     => \my $verbose,
    "fullperc|f"    => \my $fullperc,
    "ignore|i"      => \my $ignoreExcluded,
    "reverse|r"     => \my $reverse,
    "histogram|H"   => \my $histogram,
);

if (($fullperc) and (!$mincount) and (!$maxcount)) {
        print "ERROR: cannot use fullperc without either mincount or maxcount\n";
        usage();
        exit 1;
}

while (<>) {
		chomp;
		$data{$_}++; # Gets us uniq -c equiv
		$tot++;
}

# New bits to drop things with count < $mincount or > $maxcount for when we want to trim some data outliers
if (($mincount) or ($maxcount)) {
    my $c = 0;
    my $tc = 0;
    if ($verbose) {
        foreach my $key (keys %data) {
            $c++;
            $tc+=$data{$key};
        }
        print "# keys/counts before: $c / $tc\n";
    }
    foreach my $key (keys %data) {
        if (($mincount) and ($data{$key} < $mincount)) {
            $tot-=$data{$key} unless($fullperc);
            $data{'[excluded]'}+=$data{$key} unless($ignoreExcluded);
            delete($data{$key});
        } elsif (($maxcount) and ($data{$key} > $maxcount)) {
            $tot-=$data{$key} unless($fullperc);
            $data{'[excluded]'}+=$data{$key} unless($ignoreExcluded);
            delete($data{$key});
        }
    }
    if ($verbose) {
        $c = 0;
        $tc = 0;
        foreach my $key (keys %data) {
            $c++;
            $tc+=$data{$key};
        }
        print "# keys/counts after: $c / $tc\n";
    }
}

if ($tot eq 0) { exit; }

my $len = length($tot);

my $bar_inner = 0;
if ($histogram && !$csv && !$csvall) {
    my $max_key = 0;
    foreach my $k (keys %data) {
        $max_key = length($k) if length($k) > $max_key;
    }
    $bar_inner = term_width() - $decimal - $len - $max_key - 11;
    $bar_inner = 10 if $bar_inner < 10;
}
if ($date) {
    foreach my $key ($reverse ? (sort { parse_date($b) <=> parse_date($a) } keys %data)
                              : (sort { parse_date($a) <=> parse_date($b) } keys %data)) {
        pout($key);
    }
} elsif ($numeric) {
    foreach my $key ($reverse ? (sort { $b <=> $a } keys %data)
                              : (sort { $a <=> $b } keys %data)) {
        pout($key);
    }
} else {
    foreach my $key ($reverse ? (sort { $data{$b} <=> $data{$a} } keys %data)
                              : (sort { $data{$a} <=> $data{$b} } keys %data)) {
        pout($key);
    }
}
if ($histogram && !$csv && !$csvall) {
    my $w = $bar_inner + 3 + $decimal + 4;
    printf("%${w}s  %${len}d\n", 'Total:', $tot);
} elsif ($csv) {
    printf("%8s,%${len}d\n",'Total:',$tot);
} else {
    printf("%8s  %${len}d\n",'Total:',$tot);
}

sub term_width {
    return $ENV{COLUMNS} if $ENV{COLUMNS} && $ENV{COLUMNS} =~ /^\d+$/;
    my $winsize = "\x00" x 8;
    if (ioctl(STDOUT, 0x5413, $winsize)) {
        my (undef, $cols) = unpack('SS', $winsize);
        return $cols if $cols > 0;
    }
    return 80;
}

sub bar_str {
    my ($p, $w) = @_;
    my $filled = int($p / 100.0 * $w);
    $filled = $w if $filled > $w;
    return '[' . '#' x $filled . ' ' x ($w - $filled) . ']';
}

sub pout {
    my $key = shift;
    my $p=(($data{$key}/$tot)*100);
    if (($csv) or ($csvall)) {
        if ($csvall) { $key =~ s/\s+/,/g; }
        if ( $p < 10 ) {
            printf("%1.${decimal}f%s,%d,%s\n",$p,'%',$data{$key},$key);
        } else {
            printf("%2.${decimal}f%s,%d,%s\n",$p,'%',$data{$key},$key);
        }
    } elsif ($histogram) {
        my $perc = $p < 10
            ? sprintf(" %1.${decimal}f%%", $p)
            : sprintf("%2.${decimal}f%%", $p);
        printf("%s %s  %${len}d %s\n", bar_str($p, $bar_inner), $perc, $data{$key}, $key);
    } else {
        if ( $p < 10 ) {
            printf(" %1.${decimal}f%s  %${len}d %s\n",$p,'%',$data{$key},$key);
        } else {
            printf("%2.${decimal}f%s  %${len}d %s\n",$p,'%',$data{$key},$key);
        }
    }
}


sub usage {
    print "Usage: $0 [--csv] [--csvall] [--help]
    --csv       : Do output as csv rather than formatted for a human to read
    --csvall    : Do output as csv replacing any/all whitespaces with commas
    --decimal   : Specify how many decimal places to compute (default 4)
    --numeric   : If the data being fed to psort is numeric this flag will sort the
                :  output by that data instead of by counts
    --date      : Sort output by date value of data; recognises common formats:
                :  \"22 Jun 2026\", \"Jun 22 2026\", \"06/22/26\", \"06/22/2026\",
                :  \"2026-06-22\", \"22-Jun-2026\"
    --min       : Give a minimum count below which we should throw out those, to
                   allow trimming smaller values (compute % based upon what's left)
    --max       : Give a maximum count above which we should throw out those, to
                   allow trimming larger values  (compute % based upon what's left)
    --fullperc  : Only with a min/max, still output the % based upon the full count
                   vs only what matched the filter
    --ignore    : When using min/max and fullperc, don't include the [excluded] line
                   that shows us the full 100%
    --reverse,-r: Reverse the sort order (highest count/value first)
    --histogram,-H: Show a bar histogram scaled to terminal width
    --help      : Output this help info

Synopsis: Takes input and essentially does the equivalent of `sort | uniq -c | sort -n` 
with added percentages and total info, default output is nicely formatted for humans to read.
";
}
