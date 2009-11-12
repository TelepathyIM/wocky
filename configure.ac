AC_PREREQ([2.59])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set wockynano_version to 0.

m4_define([wocky_major_version], [0])
m4_define([wocky_minor_version], [0])
m4_define([wocky_micro_version], [0])
m4_define([wocky_nano_version], [0])

# Some magic
m4_define([wocky_base_version],
          [wocky_major_version.wocky_minor_version.wocky_micro_version])
m4_define([wocky_version],
          [m4_if(wocky_nano_version, 0, [wocky_base_version], [wocky_base_version].[wocky_nano_version])])dnl

AC_INIT([Wocky], [wocky_version])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 -Wno-portability tar-ustar])
AM_PROG_LIBTOOL
AM_CONFIG_HEADER(config.h)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AM_PROG_AS
AM_PROG_MKDIR_P

dnl decide error flags
dnl ifelse(wocky_nano_version, 0,
dnl     [ official_release=yes ],
dnl     [ official_release=no ])
official_release=no

TP_COMPILER_WARNINGS([ERROR_CFLAGS], [test "x$official_release" = xno],
  [all \
   extra \
   declaration-after-statement \
   shadow \
   strict-prototypes \
   missing-prototypes \
   sign-compare \
   nested-externs \
   pointer-arith \
   format-security \
   init-self],
  [missing-field-initializers \
   unused-parameter])
AC_SUBST([ERROR_CFLAGS])

ifelse(wocky_nano_version, 0,
    [ # Wocky is version x.y.z - disable coding style checks by default
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--enable-coding-style-checks],
                 [check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=no] )
    ],
    [ # Wocky is version x.y.z.1 - enable coding style checks by default
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--disable-coding-style-checks],
                 [don't check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])
    ])

if test x$enable_debug = xyes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])

dnl debugging stuff
AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[compile without debug code]),
  [
    case "${enableval}" in
      yes|no) enable="${enableval}" ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
    esac
  ],
  [enable=yes])

if test "$enable" = yes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi


dnl Check for code generation tools
AC_HEADER_STDC([])
AC_C_INLINE

dnl Check endianness (Needed for the sha1 implementation)
AC_C_BIGENDIAN

dnl Check for Glib
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.16, gobject-2.0 >= 2.16, gthread-2.0 >=
2.4, gio-2.0 >= 2.21])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

dnl Choose an SSL/TLS backend (default gnutls)
AC_ARG_WITH([tls],
  AC_HELP_STRING([--with-tls=BACKEND],
      [which TLS backend to use (gnutls, openssl, or auto) @<:@default=auto@:>@]),
  [],
  [with_tls=auto])


AS_CASE([$with_tls],
    [gnutls],  [PKG_CHECK_MODULES(TLS, [gnutls  >= 2.8.2 ])],
    [openssl], [USING_OPENSSL=yes
                AC_DEFINE(USING_OPENSSL, 1, [Define if using openssl])
                PKG_CHECK_MODULES(TLS, [openssl >= 0.9.8g])],
    [auto],    [PKG_CHECK_MODULES(TLS, [gnutls  >= 2.8.2 ],
                    [],
                    [USING_OPENSSL=yes
                     AC_DEFINE(USING_OPENSSL, 1, [Define if using openssl])
                     PKG_CHECK_MODULES(TLS, [openssl >= 0.9.8g],[],
                         AC_MSG_ERROR([Neither gnutls nor openssl found]))])],
    [*],       AC_MSG_ERROR([Must have a TLS backend (gnutls or openssl)]))

AC_SUBST(TLS_CFLAGS)
AC_SUBST(TLS_LIBS)
AM_CONDITIONAL(USING_OPENSSL, test x$USING_OPENSSL = xyes)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

dnl Check for libxml2
PKG_CHECK_MODULES(LIBXML2, [libxml-2.0])

dnl check for libsasl2 (for sasl test)
dnl must check for headers as well as library (no .pc so can't use pkg-config)
AC_CHECK_LIB(sasl2, sasl_server_new,
    [ AC_CHECK_HEADER(sasl/sasl.h,
        [ LIBSASL2_LIBS="-lsasl2"
          LIBSASL2_CFLAGS=""
          HAVE_LIBSASL2=yes
          AC_DEFINE(HAVE_LIBSASL2, 1, [Define if libsasl2 is available]) ],
        [ AC_MSG_WARN(libsasl2 headers missing: skipping sasl tests)]) ],
    [ AC_MSG_WARN(libsasl2 not found: skipping sasl tests) ])

AC_SUBST(LIBSASL2_LIBS)
AC_SUBST(LIBSASL2_CFLAGS)
AM_CONDITIONAL(HAVE_LIBSASL2, test "x$HAVE_LIBSASL2" = "xyes")

AC_SUBST(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_LIBS)

AC_ARG_ENABLE(coverage, AC_HELP_STRING([--enable-coverage],
      [compile with coverage profiling instrumentation (gcc only)]),
    [
      case "${enableval}" in
        "yes"|"no") coverage="${enableval}" ;;
        *)   AC_MSG_ERROR(bad value ${enableval} for --enable-coverage) ;;
      esac
    ]
)

WOCKY_GCOV(${coverage})
WOCKY_LCOV(${coverage})

AC_SUBST(PACKAGE_STRING)

dnl To be used by tests and examples
WOCKY_CFLAGS='-I${top_builddir} -I${top_srcdir}'
AC_SUBST(WOCKY_CFLAGS)

GTK_DOC_CHECK([1.10])

AC_OUTPUT( Makefile            \
           wocky/Makefile      \
           wocky/wocky-uninstalled.pc \
           m4/Makefile         \
           tools/Makefile      \
           examples/Makefile   \
           tests/Makefile      \
           docs/Makefile      \
           docs/reference/Makefile
)
