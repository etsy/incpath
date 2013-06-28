dnl PHP incpath extension
dnl
dnl Copyright (c) 2013 Etsy
dnl Authors: Keyur Govande, Rasmus Lerdorf
dnl
dnl See the LICENSE file in this directory for details.

PHP_ARG_ENABLE(incpath, whether to enable incpath support,
[  --enable-incpath           Enable incpath support])

if test "$PHP_INCPATH" != "no"; then
  PHP_NEW_EXTENSION(incpath, incpath.c, $ext_shared)
fi
