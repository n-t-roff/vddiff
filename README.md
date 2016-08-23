`vddiff` is a simple directory diff tool for the text terminal.
It uses `vim -dR` for doing the actual file diff.
A short manual page contains the usage documentation.
The tool has
[libavlbst](https://github.com/n-t-roff/libavlbst)
as a dependency which needs to be installed before `vddiff` can be
build and installad with
```
$ ./configure
$ make
$ su
# make install
# exit
$ make distclean
```
