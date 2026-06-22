Name:           psort
Version:        1.0.1
Release:        1%{?dist}
Summary:        Count input lines, sort by frequency, display with percentages
License:        MIT
URL:            https://forgejo.miguelito.org/miguelito/psort
Source0:        %{name}-%{version}.tar.gz

#BuildRequires:  gcc
#BuildRequires:  golang
BuildRequires:  texinfo
Requires:       perl
Requires:       python3
Requires(post): %{_sbindir}/update-alternatives
Requires(preun): %{_sbindir}/update-alternatives

%description
psort reads lines from standard input or one or more files, counts the
occurrences of each unique line, and prints them sorted by frequency
with percentages and a total count.  It is equivalent to
"sort | uniq -c | sort -n" with cleaner formatting and additional
filtering and output options.

Four implementations are installed:

  psort_c   C        fastest; default via update-alternatives
  psort_go  Go
  psort.py  Python 3
  psort.pl  Perl

/usr/bin/psort is a symlink managed by update-alternatives and points
to psort_c by default.  Use "update-alternatives --config psort" to
switch implementations.

%prep
%autosetup

%build
%{__cc} %{optflags} -o psort_c psort.c
go build -o psort_go psort.go
makeinfo psort.texi

%install
install -Dm755 psort.pl   %{buildroot}%{_bindir}/psort.pl
install -Dm755 psort_c    %{buildroot}%{_bindir}/psort_c
install -Dm755 psort_go   %{buildroot}%{_bindir}/psort_go
install -Dm755 psort.py   %{buildroot}%{_bindir}/psort.py
install -Dm644 psort.1    %{buildroot}%{_mandir}/man1/psort.1
install -Dm644 psort.info %{buildroot}%{_infodir}/psort.info

%post
update-alternatives --install %{_bindir}/psort psort %{_bindir}/psort_c  40
update-alternatives --install %{_bindir}/psort psort %{_bindir}/psort_go 30
update-alternatives --install %{_bindir}/psort psort %{_bindir}/psort.py 20
update-alternatives --install %{_bindir}/psort psort %{_bindir}/psort.pl 10
install-info %{_infodir}/psort.info %{_infodir}/dir || :

%preun
if [ $1 -eq 0 ]; then
    install-info --delete %{_infodir}/psort.info %{_infodir}/dir || :
    update-alternatives --remove psort %{_bindir}/psort_c  || :
    update-alternatives --remove psort %{_bindir}/psort_go || :
    update-alternatives --remove psort %{_bindir}/psort.py || :
    update-alternatives --remove psort %{_bindir}/psort.pl || :
fi

%files
%{_bindir}/psort_c
%{_bindir}/psort_go
%{_bindir}/psort.py
%{_bindir}/psort.pl
%{_mandir}/man1/psort.1*
%{_infodir}/psort.info*
%ghost %{_bindir}/psort

%changelog
* Mon 22 Jun 2026 Mike Marion <mm-claudeai@miguelito.org> - 1.0.1-1
- Add --date option to sort output by date string value
- Move histogram bar to right of percent/count/data columns

* Sun 17 May 2026 Mike Marion <mm-claudeai@miguelito.org> - 1.0.0-1
- Initial package
