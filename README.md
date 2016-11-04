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
On NetBSD it is recommended to install the `ncursesw`
package (note the `w` for wide char support).

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
