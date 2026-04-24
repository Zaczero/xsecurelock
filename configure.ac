# Copyright 2014 Google Inc. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

define([thisversion],[1.9.0])
define([nextversion],[1.9.0])
AC_INIT([xsecurelock],[thisversion],[https://github.com/Zaczero/xsecurelock/issues])
AM_INIT_AUTOMAKE([-Wall subdir-objects foreign])
AC_PROG_CC
AC_LANG([C])
AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE([[
    #if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
    #error A C99 language mode is required.
    #endif

    #include <stdint.h>

    struct c99_probe {
      int value;
    };

    int main(void) {
      struct c99_probe probe = (struct c99_probe){ .value = 1 };
      int64_t total = probe.value;

      for (int i = 0; i < 1; ++i) {
        total += i;
      }

      return (int)total;
    }
  ]])],
  [],
  [AC_MSG_ERROR([a C99-capable compiler is required])])
AC_CONFIG_HEADERS([config.h])

WARNING_CFLAGS=
AC_DEFUN([RP_TRY_CC_FLAG], [
	AC_MSG_CHECKING([whether the compiler supports $1])
	saved_CFLAGS=$CFLAGS
	CFLAGS="$saved_CFLAGS $WARNING_CFLAGS $1"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
	                  [AC_MSG_RESULT([yes])
	                   AS_IF([test "x$WARNING_CFLAGS" = x],
	                         [WARNING_CFLAGS="$1"],
	                         [WARNING_CFLAGS="$WARNING_CFLAGS $1"])],
	                  [AC_MSG_RESULT([no])])
	CFLAGS=$saved_CFLAGS])
RP_TRY_CC_FLAG([-Wall])
RP_TRY_CC_FLAG([-Wextra])
RP_TRY_CC_FLAG([-Wcast-align])
RP_TRY_CC_FLAG([-Wcast-qual])
RP_TRY_CC_FLAG([-Wformat-security])
RP_TRY_CC_FLAG([-Wmissing-prototypes])
RP_TRY_CC_FLAG([-Wmissing-declarations])
RP_TRY_CC_FLAG([-Wold-style-definition])
RP_TRY_CC_FLAG([-Wstrict-prototypes])
RP_TRY_CC_FLAG([-Wundef])
RP_TRY_CC_FLAG([-Wvla])
RP_TRY_CC_FLAG([-Wwrite-strings])
RP_TRY_CC_FLAG([-Werror=implicit-function-declaration])
RP_TRY_CC_FLAG([-Werror=return-type])
AC_SUBST([WARNING_CFLAGS])

dnl Feature-test macros must be compiler flags so they apply before any system
dnl headers in translation units that start with build-config.h or system
dnl includes instead of config.h.
FEATURE_TEST_CPPFLAGS='-D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600'
AC_SUBST([FEATURE_TEST_CPPFLAGS])

dnl We only detect libraries globally, but not all binaries need all of them.
dnl So we use --as-needed to work around.
AC_MSG_CHECKING([whether the linker supports --as-needed])
saved_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -Wl,--as-needed"
AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
               [AC_MSG_RESULT([yes])],
               [AC_MSG_RESULT([no])
                LDFLAGS=$saved_LDFLAGS])

AC_CHECK_HEADER(X11/Xlib.h,
                [], [AC_MSG_ERROR([X includes not found.])])
AC_CHECK_LIB(X11, XOpenDisplay,
             [], [AC_MSG_ERROR([X library not found.])])
AC_CHECK_LIB(Xmuu, XmuClientWindow,
             [], [AC_CHECK_LIB(Xmu, XmuClientWindow,
             [], [AC_MSG_ERROR([Xmuu or Xmu library not found.])])])
AC_CHECK_LIB(m, sqrt,
             [], [AC_MSG_ERROR([Math library not found.])])

# RP_SEARCH_LIBS(sym, lib, macro, flag, default, description)
AC_DEFUN([RP_SEARCH_LIBS], [
          AC_ARG_WITH([$4],
                      [AS_HELP_STRING([--with-$4],
                                      [$6 @<:@default=$5@:>@])],
                      [with_$4=$withval],
                      [with_$4=$5])
          AS_IF([test "x$with_$4" = xno],
                [have_$4=false],
                [AC_SEARCH_LIBS([$1], [$2],
                                [have_$4=true],
                                [AS_IF([test "x$with_$4" = xcheck],
                                       [have_$4=false],
                                       [AC_MSG_ERROR([--with-$4 was enabled, but test for $4 failed])])])])
          AM_CONDITIONAL([$3], [test x$have_$4 = xtrue])])

# RP_SEARCH_PROG(prog, path, macro, flag, default, description)
AC_DEFUN([RP_SEARCH_PROG], [
          AC_ARG_WITH([$4],
                      [AS_HELP_STRING([--with-$4],
                                      [$6 @<:@default=$5@:>@])],
                      [with_$4=$withval],
                      [with_$4=$5])
          AS_IF([test "x$with_$4" = xno],
                [have_$4=false],
                [path_to_$4=$with_$4
                 AC_PATH_PROG([path_to_$4], [$1], [], [$2])
                 AS_IF([test "x$path_to_$4" = x],
                       [AS_IF([test "x$with_$4" = xcheck],
                              [have_$4=false],
                              [AC_MSG_ERROR([--with-$4 was enabled, but test for $4 failed])])],
                       [have_$4=true])])
          AM_CONDITIONAL([$3], [test x$have_$4 = xtrue])])

# The DPMS extension is used to save power of the screen when turned off. Saves
# electric power.
RP_SEARCH_LIBS(DPMSQueryExtension, Xext,
               [HAVE_DPMS_EXT], [dpms], [check],
               [Use the DPMS extension to save power])
if test x$have_dpms = xtrue; then
  AC_DEFINE([HAVE_DPMS_EXT], [1], [Use the DPMS extension to save power.])
fi

# The X11 Screen Saver extension is used to turn off the screen saver when X11
# handles screen blanking (e.g. via timeout) anyway; not needed for when we
# handle blanking explicitly (XSECURELOCK_BLANK_TIMEOUT). Saves CPU power.
RP_SEARCH_LIBS(XScreenSaverQueryExtension, Xss,
               [HAVE_XSCREENSAVER_EXT], [xss], [check],
               [Use the X11 Screen Saver extension to save power])
if test x$have_xss = xtrue; then
  AC_DEFINE([HAVE_XSCREENSAVER_EXT], [1],
            [Use the X11 Screen Saver extension to save power.])
fi

# The X Synchronization extension is used to get per-device idle times. Used by
# until_nonidle only.
RP_SEARCH_LIBS(XSyncQueryExtension, Xext,
               [HAVE_XSYNC_EXT], [xsync], [check],
               [Use the X Synchronization extension to detect idle time])
if test x$have_xsync = xtrue; then
  AC_DEFINE([HAVE_XSYNC_EXT], [1],
            [Use the X Synchronization extension to detect idle time.])
fi

# The Composite extension needs an explicit opt-out because not having it can
# cause desktop notifications to appear on top of the screen saver (privacy
# risk).
RP_SEARCH_LIBS(XCompositeQueryExtension, Xcomposite,
               [HAVE_XCOMPOSITE_EXT], [xcomposite], [yes],
               [Use the X11 Composite extension to cover desktop notifications])
if test x$have_xcomposite = xtrue; then
  AC_DEFINE([HAVE_XCOMPOSITE_EXT], [1],
            [Use the X11 Composite extension to cover desktop notifications.])
fi

# This extension doesn't really exist anymore, but served to counteract the
# (also no longer existing) AllowClosedownGrabs and similar X server settings.
RP_SEARCH_LIBS(XF86MiscSetGrabKeysState, Xxf86misc,
               [HAVE_XF86MISC_EXT], [xf86misc], [check],
               [Use the XFree86-Misc extension to disable ungrab keys])
if test x$have_xf86misc = xtrue; then
  AC_DEFINE([HAVE_XF86MISC_EXT], [1],
            [Use the XFree86-Misc extension to disable ungrab keys.])
fi

# PAM-less operation is... unfortunate. Let's have users explicitly disable this
# if they need to use other auth modules, as most likely they should rather
# install PAM development libraries instead.
RP_SEARCH_LIBS(pam_authenticate, pam,
               [HAVE_PAM], [pam], [yes],
               [Enable support for the PAM library to authenticate the user])
if test x$have_pam = xtrue; then
  AC_DEFINE([HAVE_PAM], [1],
            [Enable support for the PAM library to authenticate the user.])
fi

AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CHECK_FUNCS([clock_gettime explicit_bzero mlock mlockall pipe2])

# Xft optionally provides nicer font rendering.
PKG_PROG_PKG_CONFIG
AC_ARG_WITH([fontconfig],
            [AS_HELP_STRING([--with-fontconfig],
                            [Use fontconfig @<:@default=check@:>@])],
            [with_fontconfig=$withval],
            [with_fontconfig=check])
if test "x$with_fontconfig" = xno; then
  have_fontconfig=false
else
  PKG_CHECK_MODULES([FONTCONFIG], [fontconfig],
                    [have_fontconfig=true],
                    [have_fontconfig=false])
  if test x$have_fontconfig != xtrue &&
     test "x$with_fontconfig" != xcheck; then
    AC_MSG_ERROR([--with-fontconfig was enabled, but test for fontconfig failed])
  fi
fi
AM_CONDITIONAL([HAVE_FONTCONFIG], [test x$have_fontconfig = xtrue])
if test x$have_fontconfig = xtrue; then
  AC_DEFINE([HAVE_FONTCONFIG], [1], [Use fontconfig.])
  check_if_fontconfig_else_no=check
else
  check_if_fontconfig_else_no=no
fi
AC_ARG_WITH([xft],
            [AS_HELP_STRING([--with-xft],
                            [Use the Xft extension for nicer fonts (requires Xrender and fontconfig too) @<:@default=$check_if_fontconfig_else_no@:>@])],
            [with_xft=$withval],
            [with_xft=$check_if_fontconfig_else_no])
if test "x$with_xft" = xno; then
  have_xft=false
else
  PKG_CHECK_MODULES([XFT], [xft],
                    [have_xft=true],
                    [have_xft=false])
  if test x$have_xft != xtrue &&
     test "x$with_xft" != xcheck; then
    AC_MSG_ERROR([--with-xft was enabled, but test for xft failed])
  fi
fi
AM_CONDITIONAL([HAVE_XFT_EXT], [test x$have_xft = xtrue])
if test x$have_xft = xtrue; then
  AC_DEFINE([HAVE_XFT_EXT], [1], [Use the Xft extension for nicer fonts.])
fi

# Querying the keyboard LEDs (and layout name) makes auth_x11 more user
# friendly.
RP_SEARCH_LIBS(XkbGetIndicatorState, X11,
               [HAVE_XKB_EXT], [xkb], [check],
               [Use the X Keyboard extension to show the keyboard LEDs])
if test x$have_xkb = xtrue; then
  AC_DEFINE([HAVE_XKB_EXT], [1],
            [Use the X Keyboard extension to show the keyboard LEDs.])
fi

# XRandR provides information about monitor layouts and is strongly recommended
# on systems which can use more than one monitor (which includes most laptops).
RP_SEARCH_LIBS(XRRGetScreenResources, Xrandr,
               [HAVE_XRANDR_EXT], [xrandr], [check],
               [Use the XRandR extension to query monitor layouts])
if test x$have_xrandr = xtrue; then
  AC_DEFINE([HAVE_XRANDR_EXT], [1],
            [Use the XRandR extension to query monitor layouts.])
fi

# The XFixes extension is used to work around possible weird leftover state from
# compositors.
RP_SEARCH_LIBS(XFixesSetWindowShapeRegion, Xfixes,
               [HAVE_XFIXES_EXT], [xfixes], [check],
               [Use the XFixes extension to work around some compositors])
if test x$have_xfixes = xtrue; then
  AC_DEFINE([HAVE_XFIXES_EXT], [1],
            [Use the XFixes extension to work around some compositors.])
fi

RP_SEARCH_PROG(htpasswd, [$PATH],
               [HAVE_HTPASSWD], [htpasswd], [check],
               [Install auth_htpasswd (specify --with-htpasswd=/usr/bin/htpasswd to set the path to use)])

RP_SEARCH_PROG(mplayer, [$PATH],
               [HAVE_MPLAYER], [mplayer], [check],
               [Install saver_mplayer (use --with-mplayer=/usr/bin/mplayer to force a runtime path)])

RP_SEARCH_PROG(mpv, [$PATH],
               [HAVE_MPV], [mpv], [check],
               [Install saver_mpv (use --with-mpv=/usr/bin/mpv to force a runtime path)])

RP_SEARCH_PROG(pamtester, [$PATH],
               [HAVE_PAMTESTER], [pamtester], [check],
               [Install authproto_pamtester (specify --with-pamtester=/usr/bin to set the path to use)])

RP_SEARCH_PROG(pandoc, [$PATH],
               [HAVE_PANDOC], [pandoc], [check],
               [Use pandoc to generate man pages])

RP_SEARCH_PROG(slidescreen, [/usr/libexec/xscreensaver:/usr/lib/xscreensaver],
               [HAVE_XSCREENSAVER], [xscreensaver], [check],
               [Install saver_xscreensaver (specify --with-xscreensaver=/usr/libexec/xscreensaver to set the saver plugin directory to use)])
# However, all we really want is the directory name...
path_to_xscreensaver=${path_to_xscreensaver%/slidescreen}

pam_service_name=
enable_pam_check_account_type=no

if test x$have_pam = xtrue; then
  AC_ARG_WITH([pam_service_name],
              AS_HELP_STRING([--with-pam-service-name],
                             [The name of the PAM service that xsecurelock's
                              auth helpers will authenticate as by default.
                              This flag is mandatory. Be sure to verify that
                              /etc/pam.d/SERVICENAME actually exists, as failure
                              of doing so may prevent unlocking the screen.]),
              [pam_service_name=$withval],
              [AC_MSG_ERROR([--with-pam-service-name is mandatory.])])
  if test ! -f "/etc/pam.d/$pam_service_name"; then
    AC_MSG_WARN([/etc/pam.d/$pam_service_name doesn't exist. Sure?])
  fi

  AC_ARG_ENABLE([pam_check_account_type],
                AS_HELP_STRING([--enable-pam-check-account-type],
                               [Enable checking ordinary PAM accounting
                                failures. Required expired-password changes are
                                always enforced. Only enable this if accounting
                                is properly configured on your system;
                                especially keep this disabled if
                                --with-pam-service-name is set to an
                                authentication-only service name like
                                common-auth.]),
                [enable_pam_check_account_type=$enableval],
                [enable_pam_check_account_type=no])
fi

AC_ARG_WITH([default_auth_module],
            AS_HELP_STRING([--with-default-auth-module],
                           [The default authentication module to use when none
                            was specified. Can be overridden via the
                            XSECURELOCK_AUTH environment variable or a command
                            line argument.]),
                            [auth_executable=$withval],
                            [auth_executable=auth_x11])

AC_MSG_NOTICE([Configured default auth module: $auth_executable])

AC_ARG_WITH([default_authproto_module],
            AS_HELP_STRING([--with-default-authproto-module],
                           [The default authentication protocol module to use when none was
                            specified. Can be overridden via the
                            XSECURELOCK_AUTHPROTO environment variable. By
                            default this is autodetected to be one of the
                            supported authentication modules.]),
                            [authproto_executable=$withval],
                            [authproto_executable=
                             if test x$have_htpasswd = xtrue; then
                               authproto_executable=authproto_htpasswd
                             fi
                             if test x$have_pamtester = xtrue; then
                               authproto_executable=authproto_pamtester
                             fi
                             if test x$have_pam = xtrue; then
                               authproto_executable=authproto_pam
                             fi
                             if test -z "$authproto_executable"; then
                               AC_MSG_ERROR([No authproto executable found.])
                             fi])

AC_MSG_NOTICE([Configured default authproto module: $authproto_executable])

AC_ARG_WITH([default_global_saver_module],
            AS_HELP_STRING([--with-default-global-saver-module],
                           [The default global saver module to use when none was
                            specified. Can be overridden via the
                            XSECURELOCK_GLOBAL_SAVER environment variable.]),
                            [global_saver_executable=$withval],
                            [global_saver_executable=saver_multiplex])

AC_MSG_NOTICE([Configured default global saver module: ]dnl
[$global_saver_executable])

AC_ARG_WITH([default_saver_module],
            AS_HELP_STRING([--with-default-saver-module],
                           [The default per-screen saver module to use when none
                            was specified. Can be overridden via the
                            XSECURELOCK_SAVER environment variable.]),
                            [saver_executable=$withval],
                            [saver_executable=saver_blank])

AC_MSG_NOTICE([Configured default saver module: $saver_executable])

# Expand install directories for compiled-in paths.
saved_prefix=$prefix
saved_exec_prefix=$exec_prefix
if test "x$prefix" = xNONE; then
  prefix=$ac_default_prefix
fi
if test "x$exec_prefix" = xNONE; then
  exec_prefix=$prefix
fi
eval "compiled_docdir=\"$docdir\""
eval "compiled_docdir=\"$compiled_docdir\""
compiled_pkglibexecdir='${libexecdir}/${PACKAGE_TARNAME}'
eval "compiled_pkglibexecdir=\"$compiled_pkglibexecdir\""
eval "compiled_pkglibexecdir=\"$compiled_pkglibexecdir\""
prefix=$saved_prefix
exec_prefix=$saved_exec_prefix

AC_SUBST(pam_service_name)
AC_SUBST(auth_executable)
AC_SUBST(authproto_executable)
AC_SUBST(compiled_docdir)
AC_SUBST(compiled_pkglibexecdir)
AC_SUBST(global_saver_executable)
AC_SUBST(saver_executable)
AC_SUBST(path_to_htpasswd)
AC_SUBST(path_to_mplayer)
AC_SUBST(path_to_mpv)
AC_SUBST(path_to_pamtester)
AC_SUBST(path_to_xscreensaver)
AM_CONDITIONAL([PAM_CHECK_ACCOUNT_TYPE],
               [test x$enable_pam_check_account_type = xyes])
if test x$enable_pam_check_account_type = xyes; then
  AC_DEFINE([PAM_CHECK_ACCOUNT_TYPE], [1],
            [Enable checking of PAM accounting result.])
fi
AM_CONDITIONAL([HAVE_IDLE_TIMER],
               [test x$have_xss = xtrue -o x$have_xsync = xtrue])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([build-config.h])
AC_CONFIG_FILES([helpers/authproto_htpasswd],
                [chmod +x helpers/authproto_htpasswd])
AC_CONFIG_FILES([helpers/authproto_pamtester],
                [chmod +x helpers/authproto_pamtester])
AC_CONFIG_FILES([helpers/saver_mplayer],
                [chmod +x helpers/saver_mplayer])
AC_CONFIG_FILES([helpers/saver_mpv],
                [chmod +x helpers/saver_mpv])
AC_CONFIG_FILES([helpers/saver_xscreensaver],
                [chmod +x helpers/saver_xscreensaver])

# Generate documentation.
AC_CHECK_PROGS([DOXYGEN], [doxygen], [])
if test -z "$DOXYGEN"; then
  AC_MSG_WARN([Doxygen not found. Who needs documentation anyway.])
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])
AC_CONFIG_FILES([Doxyfile])

# End of documentation.

AC_OUTPUT
