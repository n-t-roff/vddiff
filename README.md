`vddiff` is a simple directory diff tool for the text terminal (see
[screenshot](http://n-t-roff.github.io/vddiff)).
It uses `vim -dR` for doing the actual file diff
(other diff tools can be configured).
A short
[manual page](http://n-t-roff.github.io/vddiff/vddiff.1.html)
contains the usage documentation.
It is recommended (but not required) to install
the speed optimized AVL library
[libavlbst](https://github.com/n-t-roff/libavlbst)
*before* `vddiff` can be build and installad with
```
$ ./configure
$ make
$ su
# make install
# exit
$ make distclean
```
If there are any problems please report them at the
[issue list](https://github.com/n-t-roff/vddiff/issues)
or write a mail to troff@arcor.de
