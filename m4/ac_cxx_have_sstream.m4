dnl @synopsis AC_CXX_HAVE_SSTREAM
dnl
dnl If the C++ library has a working stringstream, define HAVE_SSTREAM.
dnl
dnl @author Ben Stanley
dnl @version $Id: ac_cxx_have_sstream.m4 3830 2005-06-24 07:01:15Z waananen $
dnl
AC_DEFUN([AC_CXX_HAVE_SSTREAM],
[AC_CACHE_CHECK(whether the compiler has stringstream,
ac_cv_cxx_have_sstream,
[AC_REQUIRE([AC_CXX_NAMESPACES])
 AC_LANG_SAVE
 AC_LANG_CPLUSPLUS
 AC_TRY_COMPILE([#include <sstream>
#ifdef HAVE_NAMESPACES
using namespace std;
#endif],[stringstream message; message << "Hello"; return 0;],
 ac_cv_cxx_have_sstream=yes, ac_cv_cxx_have_sstream=no)
 AC_LANG_RESTORE
])
if test "$ac_cv_cxx_have_sstream" = yes; then
  AC_DEFINE(HAVE_SSTREAM,,[define if the compiler has stringstream])
fi
])
