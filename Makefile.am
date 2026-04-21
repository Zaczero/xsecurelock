# Copyright 2014 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

CLEANFILES =

AM_CPPFLAGS = -I$(top_builddir) $(FEATURE_TEST_CPPFLAGS)
AM_CFLAGS = $(WARNING_CFLAGS)

bin_PROGRAMS = \
	xsecurelock
xsecurelock_SOURCES = \
	auth_child.c auth_child.h \
	env_settings.c env_settings.h \
	logging.c logging.h \
	mlock_page.h \
	main.c \
	saver_child.c saver_child.h \
	unmap_all.c unmap_all.h \
	util.c util.h \
	version.c version.h \
	wait_pgrp.c wait_pgrp.h \
	wm_properties.c wm_properties.h \
	xscreensaver_api.c xscreensaver_api.h \
	incompatible_compositor.xbm
nodist_xsecurelock_SOURCES = \
	env_helpstr.inc

helpersdir = $(pkglibexecdir)
helpers_SCRIPTS = \
	helpers/saver_blank
if HAVE_HTPASSWD
helpers_SCRIPTS += \
	helpers/authproto_htpasswd
endif
if HAVE_MPLAYER
helpers_SCRIPTS += \
	helpers/saver_mplayer
endif
if HAVE_MPV
helpers_SCRIPTS += \
	helpers/saver_mpv
endif
if HAVE_PAMTESTER
helpers_SCRIPTS += \
	helpers/authproto_pamtester
endif
if HAVE_XSCREENSAVER
helpers_SCRIPTS += \
	helpers/saver_xscreensaver
endif

helpers_PROGRAMS = \
	pgrp_placeholder
pgrp_placeholder_SOURCES = \
	helpers/pgrp_placeholder.c

helpers_PROGRAMS += \
	saver_multiplex
saver_multiplex_SOURCES = \
	env_settings.c env_settings.h \
	helpers/monitors.c helpers/monitors.h \
	helpers/saver_multiplex.c \
	logging.c logging.h \
	saver_child.c saver_child.h \
	wait_pgrp.c wait_pgrp.h \
	wm_properties.c wm_properties.h \
	xscreensaver_api.c xscreensaver_api.h

helpers_PROGRAMS += \
	dimmer
dimmer_SOURCES = \
	env_settings.c env_settings.h \
	helpers/dimmer.c \
	logging.c logging.h \
	wm_properties.c wm_properties.h

if HAVE_IDLE_TIMER
helpers_PROGRAMS += \
	until_nonidle
until_nonidle_SOURCES = \
	env_settings.c env_settings.h \
	helpers/until_nonidle.c \
	logging.c logging.h \
	util.c util.h \
	wait_pgrp.c wait_pgrp.h
endif

helpers_PROGRAMS += \
	auth_x11
auth_x11_SOURCES = \
	env_info.c env_info.h \
	env_settings.c env_settings.h \
	helpers/authproto.c helpers/authproto.h \
	helpers/auth_x11.c \
	helpers/indicator_text.c helpers/indicator_text.h \
	helpers/monitors.c helpers/monitors.h \
	helpers/prompt_display.c helpers/prompt_display.h \
	helpers/prompt_random.c helpers/prompt_random.h \
	helpers/prompt_state.c helpers/prompt_state.h \
	helpers/xkb.c helpers/xkb.h \
	logging.c logging.h \
	mlock_page.h \
	rect.c rect.h \
	util.c util.h \
	wait_pgrp.c wait_pgrp.h \
	wm_properties.c wm_properties.h \
	xscreensaver_api.c xscreensaver_api.h
auth_x11_CPPFLAGS = $(AM_CPPFLAGS) $(FONTCONFIG_CFLAGS) $(XFT_CFLAGS)
auth_x11_LDADD = $(FONTCONFIG_LIBS) $(XFT_LIBS)

if HAVE_PAM
helpers_PROGRAMS += \
	authproto_pam
authproto_pam_SOURCES = \
	env_info.c env_info.h \
	env_settings.c env_settings.h \
	helpers/authproto.c helpers/authproto.h \
	helpers/authproto_pam.c \
	logging.c logging.h \
	mlock_page.h \
	util.c util.h
endif

doc_DATA = \
	CONTRIBUTING \
	LICENSE \
	README.md

if HAVE_PANDOC
man1_MANS = xsecurelock.1
xsecurelock.1.md: $(srcdir)/doc/xsecurelock.1.md $(srcdir)/README.md
	{ \
		grep -B 9999 'ENV VARIABLES HERE' "$(srcdir)/doc/xsecurelock.1.md"; \
		grep -A 9999 'ENV VARIABLES START' "$(srcdir)/README.md" |\
			grep -B 9999 'ENV VARIABLES END'; \
		grep -A 9999 'ENV VARIABLES HERE' "$(srcdir)/doc/xsecurelock.1.md"; \
	} > xsecurelock.1.md
xsecurelock.1: xsecurelock.1.md
	$(path_to_pandoc) -s -f markdown -t man -o $@ $<
CLEANFILES += xsecurelock.1.md xsecurelock.1
endif

# Some tools that we sure don't wan to install
noinst_PROGRAMS = authproto_bounds_test cat_authproto nvidia_break_compositor get_compositor remap_all rect_test prompt_display_test prompt_state_test xkb_test explicit_bzero_test explicit_bzero_fallback_test retry_io_test retry_io_fallback_test indicator_text_test mlock_page_test prompt_random_test env_settings_test
authproto_bounds_test_SOURCES = \
	logging.c logging.h \
	helpers/authproto.c helpers/authproto.h \
	test/authproto_bounds_test.c \
	util.c util.h
cat_authproto_SOURCES = \
	logging.c logging.h \
	helpers/authproto.c helpers/authproto.h \
	test/cat_authproto.c \
	util.c util.h
nvidia_break_compositor_SOURCES = \
	test/nvidia_break_compositor.c
get_compositor_SOURCES = \
	test/get_compositor.c
remap_all_SOURCES= \
	test/remap_all.c \
	unmap_all.c unmap_all.h
rect_test_SOURCES = \
	rect.c rect.h \
	test/rect_test.c
prompt_display_test_SOURCES = \
	helpers/prompt_display.c helpers/prompt_display.h \
	helpers/prompt_state.h \
	test/prompt_display_test.c
prompt_state_test_SOURCES = \
	helpers/prompt_random.c helpers/prompt_random.h \
	helpers/prompt_state.c helpers/prompt_state.h \
	test/prompt_state_test.c \
	util.c util.h
explicit_bzero_test_SOURCES = \
	test/explicit_bzero_test.c \
	util.c util.h
explicit_bzero_fallback_test_SOURCES = \
	test/explicit_bzero_test.c \
	util.c util.h
explicit_bzero_fallback_test_CPPFLAGS = $(AM_CPPFLAGS) -DFORCE_EXPLICIT_BZERO_FALLBACK=1
retry_io_test_SOURCES = \
	test/retry_io_test.c \
	util.c util.h
retry_io_fallback_test_SOURCES = \
	test/retry_io_test.c \
	util.c util.h
retry_io_fallback_test_CPPFLAGS = $(AM_CPPFLAGS) -DFORCE_PIPE2_FALLBACK=1
indicator_text_test_SOURCES = \
	helpers/indicator_text.c helpers/indicator_text.h \
	test/indicator_text_test.c
xkb_test_SOURCES = \
	helpers/indicator_text.c helpers/indicator_text.h \
	helpers/xkb.c helpers/xkb.h \
	logging.c logging.h \
	test/xkb_test.c
mlock_page_test_SOURCES = \
	test/mlock_page_test.c \
	mlock_page.h
mlock_page_test_CPPFLAGS = $(AM_CPPFLAGS) -DFORCE_MLOCK_PAGE_UNAVAILABLE=1
prompt_random_test_SOURCES = \
	helpers/prompt_random.c helpers/prompt_random.h \
	test/prompt_random_test.c
env_settings_test_SOURCES = \
	env_settings.c env_settings.h \
	logging.c logging.h \
	test/env_settings_test.c

check-local:
	@$(SHELL) "$(abs_srcdir)/test/check-local.sh" "$(abs_srcdir)"

FORCE:
version.c: FORCE
	if [ -n "$(GIT_VERSION)" ]; then \
		echo "const char *const git_version = \"$(GIT_VERSION)\";" \
			> version.c; \
	elif git describe --always --dirty >/dev/null 2>&1; then \
		echo "const char *const git_version = \"` \
			git describe --always --dirty \
		`\";" > version.c; \
	else \
		: version.c must exist in non-git builds.; \
		cat version.c; \
	fi
.PRECIOUS: version.c  # Old one is better than none.

env_helpstr.inc: README.md # Autogenerate for --help.
	grep -A 9999 'ENV VARIABLES START' "$<" |\
		grep -B 9999 'ENV VARIABLES END' |\
		grep '^[ *]' |\
		sed -e 's,\\\|",\\&,g; s,$$,\\n",; s,^,",;' > "$@"
main.$(OBJEXT): env_helpstr.inc

EXTRA_DIST = \
	CONTRIBUTING \
	LICENSE \
	README.md \
	autogen.sh \
	build-config.h.in \
	doc/xsecurelock.1.md \
	ensure-documented-settings.sh \
	helpers/saver_blank \
	incompatible_compositor.xbm.sh \
	run-iwyu.sh \
	run-linters.sh \
	test/check-local.sh \
	test/*.c \
	test/*.py \
	test/*.sh \
	test/*.xdo \
	version.c

examplesdir = $(docdir)/examples
dist_examples_DATA = \
	doc/examples/saver_livestreams

if HAVE_DOXYGEN
doxyfile.stamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > doxyfile.stamp

CLEANFILES += doxyfile.stamp

all-local: doxyfile.stamp

clean-local:
	$(RM) -r $(top_srcdir)/doxy
endif
