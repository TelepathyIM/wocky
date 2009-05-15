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
m4_define([wockybase_version],
          [wockymajor_version.wockyminor_version.wockymicro_version])
m4_define([wockyversion],
          [m4_if(wockynano_version, 0, [wockybase_version], [wockybase_version].[wockynano_version])])dnl

AC_INIT([Wocky], [wockyversion])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 -Wno-portability tar-ustar])
AM_PROG_LIBTOOL
AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AM_PROG_AS
AM_PROG_MKDIR_P

dnl decide error flags
ifelse(wockynano_version, 0,
    [ official_release=yes ],
    [ official_release=no ])

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

ifelse(wockynano_version, 0,
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

dnl GTK docs
GTK_DOC_CHECK

dnl Check for Glib
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.16, gobject-2.0 >= 2.16, gthread-2.0 >=
2.4, gnio])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

dnl Check for libxml2
PKG_CHECK_MODULES(LIBXML2, [libxml-2.0])

dnl check for libsasl2 (for sasl test)
AC_CHECK_LIB(sasl2, sasl_server_new,
             [LIBSASL2_LIBS="-lsasl2"
              LIBSASL2_CFLAGS=""
              HAVE_LIBSASL2=yes
              AC_DEFINE(HAVE_LIBSASL2, 1,
                        [Define if libsasl2 support is available])
             ],
             [ AC_MSG_WARN(libsasl2 not found: not building sasl tests) ] )

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

GTK_DOC_CHECK([1.10])

AC_OUTPUT( Makefile            \
           wocky/Makefile      \
           m4/Makefile         \
           tools/Makefile      \
           examples/Makefile   \
           tests/Makefile      \
           docs/Makefile      \
           docs/reference/Makefile      \
)
