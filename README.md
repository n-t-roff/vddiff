For information on `vddiff` please visit the
[project web page](http://n-t-roff.github.io/vddiff).

The source code is downloaded with
```
git clone https://github.com/n-t-roff/vddiff.git
```
and can be updated with
```
git pull
```
(latest changes are notified in the
[change log](https://github.com/n-t-roff/vddiff/commits/master)).

It is suggested (but not required) to install
the speed optimized AVL library
[libavlbst](https://github.com/n-t-roff/libavlbst).

Some configuration can be done in
[Makefile.in](https://github.com/n-t-roff/vddiff/blob/master/Makefile.in):
* Path `PREFIX` (default `/usr/local`) is prepended
for each file to install and for searching the library `libavlbst`.
If this library is used and is not installed in the `PREFIX` tree,
`INCDIR` and `LIBDIR` need to be adjusted.
* The manpage is installed in `${MANDIR}/man1`.
Default for `MANDIR` is `${PREFIX}/share/man`.
On some systems this might better be changed to `${PREFIX}/man`.

`vddiff` is build and installed with
```
$ ./configure
$ make
$ su
# make install
# exit
$ make distclean
```
Please report problems and feature requests on the
[issue list](https://github.com/n-t-roff/vddiff/issues)
or write a mail to troff@arcor.de
in case of any question or suggestion.
