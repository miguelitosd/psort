psort
=====

Originally a Perl script that combines `sort | uniq -c | sort -n` into one
command, with percentages, aligned columns, and filtering options.

```
❯ find Movies -type f -name \*.mp4 -o -name \*.m4v | awk -F\. '{print $NF}' | psort
 2.8649%    42 mp4
97.1351%  1424 m4v
  Total:  1466
```

## Implementations

Four implementations are provided, all supporting identical options:

| Binary     | Language | Notes                              |
|------------|----------|------------------------------------|
| `psort_c`  | C        | Fastest; default `psort` symlink   |
| `psort_go` | Go       |                                    |
| `psort.py` | Python 3 |                                    |
| `psort.pl` | Perl     | Original implementation            |

When installed via the RPM or Debian package, `/usr/bin/psort` is a symlink
managed by `update-alternatives` pointing to `psort_c` by default.  Switch
with:

```
update-alternatives --config psort
```

## Usage

```
Usage: psort [options] [file ...]
    --csv, -c     : Output as CSV (percentage%,count,value)
    --csvall, -a  : CSV output, also replacing whitespace in values with commas
    --decimal, -d : Decimal places for percentages (default 4)
    --numeric     : Sort by numeric value of input instead of by count
    --min, -m     : Exclude entries with count below N
                     (percentages recomputed from remaining; see --fullperc)
    --max, -x     : Exclude entries with count above N
    --fullperc, -f: With --min/--max, compute % from the full original total
    --ignore, -i  : With --min/--max, suppress the [excluded] summary line
    --verbose, -v : With --min/--max, print key/count totals before and after
    --reverse, -r : Reverse sort order (highest count/value first)
    --help, -h    : Output this help info

Synopsis: Takes input and essentially does the equivalent of `sort | uniq -c | sort -n`
with added percentages and total info, default output is nicely formatted for humans to read.
```

### Examples

Top 5 most common file extensions, highest first:
```
find /path -type f | sed 's/.*\.//' | psort -r | head -5
```

HTTP status code distribution, CSV output:
```
awk '{print $9}' access.log | psort -c
```

Word frequencies, minimum 10 occurrences, full-total percentages:
```
tr -cs 'A-Za-z' '\n' < doc.txt | tr 'A-Z' 'a-z' | psort --min 10 --fullperc
```

## Installation

### RPM (Fedora / RHEL / CentOS)

Build and install from the included spec:

```
# Create a source tarball from the repo
git archive --prefix=psort-1.0.0/ HEAD | gzip > ~/rpmbuild/SOURCES/psort-1.0.0.tar.gz

# Build the RPM
rpmbuild -ba psort.spec

# Install
rpm -i ~/rpmbuild/RPMS/$(uname -m)/psort-1.0.0-1.*.rpm
```

### Debian / Ubuntu

Build and install from the included packaging:

```
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../psort_1.0.0-1_$(dpkg --print-architecture).deb
```

### Manual

Copy the binaries you want to `/usr/local/bin` (or anywhere on your `PATH`).
A man page is in `psort.1`:

```
sudo install -m755 psort_c psort_go psort.py psort.pl /usr/local/bin/
sudo install -m644 psort.1 /usr/local/share/man/man1/
```

## Man Page

A man page covering all options and examples is included as `psort.1`.

```
man ./psort.1
```
