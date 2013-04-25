dnl $Id$
dnl config.m4 for extension xsplit

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(xsplit, for xsplit support,
dnl Make sure that the comment is aligned:
dnl [  --with-xsplit             Include xsplit support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(xsplit, whether to enable xsplit support,
Make sure that the comment is aligned:
[  --enable-xsplit           Enable xsplit support])

if test "$PHP_XSPLIT" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-xsplit -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/xsplit.h"  # you most likely want to change this
  dnl if test -r $PHP_XSPLIT/$SEARCH_FOR; then # path given as parameter
  dnl   XSPLIT_DIR=$PHP_XSPLIT
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for xsplit files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       XSPLIT_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$XSPLIT_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the xsplit distribution])
  dnl fi

  dnl # --with-xsplit -> add include path
  dnl PHP_ADD_INCLUDE($XSPLIT_DIR/include)

  dnl # --with-xsplit -> check for lib and symbol presence
  dnl LIBNAME=xsplit # you may want to change this
  dnl LIBSYMBOL=xsplit # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $XSPLIT_DIR/lib, XSPLIT_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_XSPLITLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong xsplit lib version or lib not found])
  dnl ],[
  dnl   -L$XSPLIT_DIR/lib -lm -ldl
  dnl ])
  dnl
  dnl PHP_SUBST(XSPLIT_SHARED_LIBADD)

  PHP_REQUIRE_CXX()
  PHP_ADD_LIBRARY(stdc++, "", EXTRA_LDFLAGS)
  PHP_NEW_EXTENSION(xsplit, xsplit.cpp, $ext_shared)
fi
