`vddiff` is a simple directory diff tool for the text terminal (see
[screenshot](http://n-t-roff.github.io/vddiff)).
It uses `vim -dR` for doing the actual file diff
(other diff tools can be configured).
A short
[manual page](http://n-t-roff.github.io/vddiff/vddiff.1.html)
contains the usage documentation.
The tool has
**[libavlbst](https://github.com/n-t-roff/libavlbst)**
as a dependency which **needs to be installed before** `vddiff` **can be
build** and installad with
```
$ ./configure
$ make
$ su
# make install
# exit
$ make distclean
```
