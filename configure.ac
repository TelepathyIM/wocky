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
m4_define([wocky_nano_version], [1])

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
   cast-function-type \
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
                 [do not check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])
    ])

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])

dnl debugging stuff
AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[compile without debug code]),
  [
    case "${enableval}" in
      yes|no) enable_debug="${enableval}" ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
    esac
  ],
  [enable_debug=yes])

if test "$enable_debug" = yes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
else
  enable_debug=no
fi

AC_ARG_WITH(installed-headers,
  AC_HELP_STRING([--with-installed-headers=DIR],
                 [install development headers to DIR @<:@default=nowhere@:>@]),
  [],
  [with_installed_headers=])

AM_CONDITIONAL(INSTALL_HEADERS, test x$with_installed_headers != x)

HEADER_DIR=$with_installed_headers
AC_SUBST(HEADER_DIR)

dnl Build a shared library even though Wocky isn't stable yet?
AC_ARG_ENABLE([shared-suffix],
  AC_HELP_STRING([--enable-shared-suffix=],
    [install a shared library with a version-specific suffix]),
  [],
  [enable_shared_suffix=])
AM_CONDITIONAL([ENABLE_SHARED_SUFFIX], [test x$enable_shared_suffix != x])
SHARED_SUFFIX="$enable_shared_suffix"
AC_SUBST([SHARED_SUFFIX])

dnl Check for code generation tools
AC_HEADER_STDC([])
AC_CHECK_HEADERS_ONCE([unistd.h])
AC_C_INLINE

dnl Check endianness (Needed for the sha1 implementation)
AC_C_BIGENDIAN

dnl Check for Glib
PKG_CHECK_MODULES(GLIB,
  [glib-2.0 >= 2.44, gobject-2.0 >= 2.44, gthread-2.0 >= 2.44, gio-2.0 >= 2.44])

AC_DEFINE([GLIB_VERSION_MIN_REQUIRED], [GLIB_VERSION_2_44], [Ignore post 2.44 deprecations])
AC_DEFINE([GLIB_VERSION_MAX_ALLOWED], [GLIB_VERSION_2_44], [Prevent post 2.44 APIs])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(TLS, [gnutls >= 3.0], [ HAVE_TLS=yes ], [ HAVE_TLS=no ])
AC_SUBST(TLS_CFLAGS)
AC_SUBST(TLS_LIBS)
AM_CONDITIONAL(HAVE_TLS, test x$HAVE_TLS = xyes)


GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

dnl Check for libxml2
PKG_CHECK_MODULES(LIBXML2, [libxml-2.0])
AC_SUBST(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_LIBS)

dnl Check for sqlite
PKG_CHECK_MODULES(SQLITE, [sqlite3])
AC_SUBST(SQLITE_CFLAGS)
AC_SUBST(SQLITE_LIBS)

dnl check for libsasl2 (for sasl test)
dnl must check for headers as well as library (no .pc so can't use pkg-config)
AC_CHECK_LIB(sasl2, sasl_server_new,
    [ AC_CHECK_HEADER(sasl/sasl.h,
        [ LIBSASL2_LIBS="-lsasl2"
          LIBSASL2_CFLAGS=""
          HAVE_LIBSASL2=yes
          AC_DEFINE(HAVE_LIBSASL2, 1, [Define if libsasl2 is available]) ],
        [ AC_MSG_WARN(libsasl2 headers missing: skipping sasl tests)]) ],
    [ AC_MSG_WARN(libsasl2 not found: skipping sasl tests)
      HAVE_LIBSASL2=no
    ])

AC_SUBST(LIBSASL2_LIBS)
AC_SUBST(LIBSASL2_CFLAGS)
AM_CONDITIONAL(HAVE_LIBSASL2, test "x$HAVE_LIBSASL2" = "xyes")

PKG_CHECK_MODULES(LIBIPHB, [libiphb >= 0.61.31],
    [AC_DEFINE(HAVE_IPHB, 1, [libiphb is available])
     have_iphb=yes
    ],
    [have_iphb=no])
AC_SUBST(LIBIPHB_CFLAGS)
AC_SUBST(LIBIPHB_LIBS)

AC_ARG_ENABLE(google-relay,
  AC_HELP_STRING([--disable-google-relay],
                 [disable Google Jingle relay support]),
    [enable_google_relay=$enableval], [enable_google_relay=yes])

if test x$enable_google_relay = xyes; then
  AC_DEFINE(ENABLE_GOOGLE_RELAY, [], [Enable Google Jingle relay support])

  dnl Check for libsoup
  PKG_CHECK_MODULES(SOUP, libsoup-2.4 >= 2.42)
fi
AM_CONDITIONAL([ENABLE_GOOGLE_RELAY], [test "x$enable_google_relay" = xyes])

AC_ARG_ENABLE(coverage, AC_HELP_STRING([--enable-coverage],
      [compile with coverage profiling instrumentation (gcc only)]),
    [
      case "${enableval}" in
        "yes"|"no") enable_coverage="${enableval}" ;;
        *)   AC_MSG_ERROR(bad value ${enableval} for --enable-coverage) ;;
      esac
    ]
)

WOCKY_GCOV(${enable_coverage})
WOCKY_LCOV(${enable_coverage})

if test "x$enable_coverage" = "x"; then
  enable_coverage=no
fi

AC_SUBST(PACKAGE_STRING)

dnl To be used by tests and examples
WOCKY_CFLAGS='-I${top_builddir} -I${top_srcdir}'
AC_SUBST(WOCKY_CFLAGS)

# check for gtk-doc
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.17],[--flavour no-tmpl])
],[
AM_CONDITIONAL([ENABLE_GTK_DOC], false)
])


AC_OUTPUT( Makefile            \
           wocky/Makefile      \
           wocky/wocky-uninstalled.pc \
           wocky/wocky.pc \
           m4/Makefile         \
           tools/Makefile      \
           examples/Makefile   \
           tests/Makefile      \
           docs/Makefile      \
           docs/reference/Makefile
)

echo "
Configure summary:

        Compiler Flags.......: ${CFLAGS}
        Prefix...............: ${prefix}
        Coverage profiling...: ${enable_coverage}
        Coding style checks..: ${ENABLE_CODING_STYLE_CHECKS}
        Debug................: ${enable_debug}

    Features:
        SASL2 Tests..........: ${HAVE_LIBSASL2}
        gtk-doc documentation: ${enable_gtk_doc}
        libiphb integration..: ${have_iphb}
        Google relay support.: ${enable_google_relay}
"
