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

AM_CPPFLAGS = -I$(top_builddir)

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
	wait_pgrp.c wait_pgrp.h
endif

helpers_PROGRAMS += \
	auth_x11
auth_x11_SOURCES = \
	env_info.c env_info.h \
	env_settings.c env_settings.h \
	helpers/authproto.c helpers/authproto.h \
	helpers/auth_x11.c \
	helpers/monitors.c helpers/monitors.h \
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
xsecurelock.1.md: doc/xsecurelock.1.md README.md
	{ \
		grep -B 9999 'ENV VARIABLES HERE' doc/xsecurelock.1.md; \
		grep -A 9999 'ENV VARIABLES START' README.md |\
			grep -B 9999 'ENV VARIABLES END'; \
		grep -A 9999 'ENV VARIABLES HERE' doc/xsecurelock.1.md; \
	} > xsecurelock.1.md
xsecurelock.1: xsecurelock.1.md
	$(path_to_pandoc) -s -f markdown -t man -o $@ $<
CLEANFILES += xsecurelock.1.md xsecurelock.1
endif

# Some tools that we sure don't wan to install
noinst_PROGRAMS = authproto_bounds_test cat_authproto nvidia_break_compositor get_compositor remap_all rect_test
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

check-local:
	@set -eu; \
	for tool in clang startx Xephyr xdotool htpasswd; do \
		if ! command -v "$$tool" >/dev/null 2>&1; then \
			echo "error: make check requires '$$tool' in PATH" >&2; \
			exit 1; \
		fi; \
	done; \
	srcdir="$(abs_srcdir)"; \
	checksrcdir=$$(mktemp -d -t xsecurelock-check-src.XXXXXX); \
	builddir=$$(mktemp -d -t xsecurelock-check-build.XXXXXX); \
	prefix=$$(mktemp -d -t xsecurelock-check-prefix.XXXXXX); \
	trap 'status=$$?; rm -rf "$$checksrcdir" "$$builddir" "$$prefix"; exit $$status' 0 1 2 3 15; \
	rm -rf "$$checksrcdir"; \
	$(MAKE) distdir distdir="$$checksrcdir"; \
	git_version=$$(cd "$$srcdir" && git describe --always --dirty 2>/dev/null || true); \
	echo "Configuring sanitizer smoke build in $$builddir"; \
	cd "$$builddir"; \
	CC=clang \
	CFLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' \
	LDFLAGS='-fsanitize=address,undefined' \
	"$$checksrcdir"/configure \
		--prefix="$$prefix" \
		--without-pam \
		--with-default-auth-module=auth_x11 \
		--with-default-authproto-module=authproto_htpasswd; \
	$(MAKE) GIT_VERSION="$$git_version"; \
	$(MAKE) GIT_VERSION="$$git_version" install; \
	export ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1'; \
	export UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1'; \
	echo "Running native smoke tests"; \
	"$$builddir"/rect_test; \
	"$$builddir"/authproto_bounds_test; \
	printf 'P 7\nhunter2\n' > "$$builddir"/cat_authproto.fixture; \
	"$$builddir"/cat_authproto < "$$builddir"/cat_authproto.fixture > "$$builddir"/cat_authproto.out; \
	cmp -s "$$builddir"/cat_authproto.fixture "$$builddir"/cat_authproto.out || { \
		echo "error: cat_authproto round-trip smoke failed" >&2; \
		exit 1; \
	}; \
	echo "Running XDO smoke tests"; \
	cd "$$srcdir"/test; \
	PATH="$$prefix/bin:$$PATH" ./run-tests.sh \
		test-correct-password \
		test-wrong-password \
		test-global-saver-relative-override \
		test-authproto-static-info-advance \
		test-authproto-static-info-timeout

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
xsecurelock-main.$(OBJEXT): env_helpstr.inc

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
