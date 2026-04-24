# About XSecureLock

> [!NOTE]
> This repository is a maintained fork of the discontinued upstream
> [`google/xsecurelock`](https://github.com/google/xsecurelock). It keeps the
> same security-focused design while carrying practical bug fixes, targeted new
> features, and compatibility work for X11 desktop environments.

XSecureLock is an X11 screen lock utility whose primary goal is security.

Screen lock utilities are widespread. However, in the past they often had
security issues regarding authentication bypass (a crashing screen locker would
unlock the screen), information disclosure (notifications may appear on top of
the screen saver), or sometimes even worse.

In XSecureLock, security is achieved using a modular design to avoid the usual
pitfalls of screen locking utility design on X11. Details are available in the
[Security Design](#security-design) section.

# Requirements

XSecureLock targets POSIX/X11 systems on Linux and BSD and expects a
C99-capable compiler.

For a normal build from a git checkout on Debian/Ubuntu-style systems, install
the usual C build tools plus the X11 and PAM development packages:

*   `gcc`, `binutils`, `make`, and libc development headers such as
    `libc6-dev`
*   `autoconf` and `automake` (for `autoreconf`/`aclocal`)
*   `pkg-config`
*   `libx11-dev` and `libxmuu-dev` or `libxmu-dev`
*   `libpam0g-dev`, unless configuring with `--without-pam`
*   `libxcomposite-dev`, unless configuring with
    `--without-xcomposite`

The configure script also detects these optional build-time features when their
development packages are available:

*   `libxext-dev` for DPMS and XSync support
*   `libxss-dev` for X11 Screen Saver extension support
*   `libxfixes-dev` for compositor workarounds
*   `libxft-dev`, `libxrender-dev`, and `libfontconfig-dev` for nicer auth
    dialog fonts
*   `libx11-dev` for XKB keyboard LED/layout status in the auth dialog
*   `libxrandr-dev` for monitor layout detection
*   `libxxf86misc-dev` for legacy ungrab-key handling

Optional helper modules are installed only when their runtime tools are found
or explicitly configured:

*   `apache2-utils` for `htpasswd` (for `authproto_htpasswd`)
*   `mplayer` (for `saver_mplayer`)
*   `mpv` (for `saver_mpv`)
*   `pamtester` (for `authproto_pamtester`)
*   `xscreensaver` for XScreenSaver hacks (for `saver_xscreensaver`)

Packagers can force script helper installation by passing the final runtime
tool path to `configure`, such as `--with-mpv=/usr/bin/mpv` or
`--with-mplayer=/usr/bin/mplayer`. The target executable does not need to be
present in the build environment when an absolute path is specified.

Optional documentation and validation tools:

*   `pandoc` to generate the man page
*   `doxygen` to generate API documentation
*   `clang`, `startx`, `Xephyr`, `xdotool`, and `htpasswd` for `make check`
*   `x11-utils`, `x11-xserver-utils`, and `imagemagick` for the full manual
    XDO suite

# Installation

Replace SERVICE-NAME with an existing file in `/etc/pam.d`. If xscreensaver is
installed, `xscreensaver` is usually a good choice. On Debian and Ubuntu,
`common-auth` is only enough for password checking; use a dedicated service with
`auth`, `account` and `password` rules if you want login-like account checks or
expired-password changes. This service is only the default and can be overridden
with [`XSECURELOCK_PAM_SERVICE`](#options).

Configuring a broken or missing SERVICE-NAME will render unlocking the screen
impossible! If this should happen to you, switch to another terminal
(`Ctrl-Alt-F1`), log in there, and run: `killall xsecurelock` to force unlocking
of the screen.

```
git clone https://github.com/Zaczero/xsecurelock.git
cd xsecurelock
sh autogen.sh
./configure --with-pam-service-name=SERVICE-NAME
make
sudo make install
```

## PAM service privileges

On many Linux systems, PAM password checks work without making XSecureLock
setuid because `pam_unix` delegates the sensitive part to its own helper.
Account checks and expired-password changes may need extra privileges,
depending on the PAM stack.

`make install` does not set setuid or setgid bits. If you need full login-like
PAM behavior, install `authproto_pam` with the privileges required by your PAM
service, typically as a root-owned setuid helper on Linux. Pair this with a
dedicated PAM service and consider `--enable-pam-check-account-type` if ordinary
account failures should also block unlocking.

# Testing

The default local runtime-correctness path is:

```
make check
```

This builds a temporary out-of-tree clang/ASan/UBSan install, runs the native
helper and unit smoke tests (for example `rect_test`, `retry_io_test`,
`env_settings_test`, `signal_pipe_test`, and a `cat_authproto` packet
round-trip), and then runs the default XDO smoke suite against the installed
prefix. `make check` requires `clang`, `startx`, `Xephyr`, `xdotool`, and
`htpasswd` to be present in `PATH`.

For manual XDO runs, the existing harness stays available:

```
./test/run-tests.sh
./test/run-tests.sh test-authproto-static-info-timeout
```

With no arguments, `./test/run-tests.sh` runs the full XDO suite. Otherwise,
arguments are treated as explicit test names, with or without the `.xdo`
suffix.

Valgrind is supported only as an ad hoc native debugging tool for the low-noise
helpers:

```
valgrind ./rect_test
printf 'P 7\nhunter2\n' > /tmp/cat_authproto.fixture && \
  valgrind ./cat_authproto < /tmp/cat_authproto.fixture | \
  cmp -s /tmp/cat_authproto.fixture -
```

## Special notes for BSD systems

On BSD systems, `/usr/local` is owned by the ports system, so unless you are
creating a port, it is recommended to install to a separate location by
specifying something like `--prefix=/opt/xsecurelock` in the `./configure`
call. You can then run XSecureLock as `/opt/xsecurelock/bin/xsecurelock`.

### FreeBSD and NetBSD

In order to authenticate with PAM on FreeBSD and NetBSD, you must be root so
you can read the shadow password database. The `authproto_pam` binary can be
made to acquire these required privileges like this:

```
chmod +s /opt/xsecurelock/libexec/xsecurelock/authproto_pam
```

### OpenBSD

In order to authenticate with PAM on OpenBSD, you must be in the `auth` group
so you can run a setuid helper called `login_passwd` that can read the shadow
password database. The `authproto_pam` binary can be made to acquire these
required privileges like this:

```
chgrp auth /opt/xsecurelock/libexec/xsecurelock/authproto_pam
chmod g+s /opt/xsecurelock/libexec/xsecurelock/authproto_pam
```

Note that this adds substantially less attack surface than adding your own user
to the `auth` group, as the `login_passwd` binary can try out passwords of any
user, while `authproto_pam` is restricted to trying your own user.

# Setup

Pick one of the [authentication modules](#authentication-modules) and one of the
[screen saver modules](#screen-saver-modules).

Tell your desktop environment to run XSecureLock by using a command line such as
one of the following:

```
xsecurelock
env XSECURELOCK_SAVER=saver_xscreensaver xsecurelock
env XSECURELOCK_SAVER=saver_mpv xsecurelock
```

IMPORTANT: Make sure your desktop environment does not launch any other locker,
be it via autostart file or its own configuration, as multiple screen lockers
may interfere with each other. You have been warned!

## Authentication on resume from suspend/hibernate

To have the authentication process start up without a keypress when
the system exits suspend/hibernate, arrange for the system to send the
`SIGUSR2` signal to the XSecureLock process.

For example, you can copy the following script to the file
`/usr/lib/systemd/system-sleep/xsecurelock`:

```
#!/bin/bash
if [[ "$1" = "post" ]] ; then
  pkill -x -USR2 xsecurelock
fi
exit 0
```

Don't forget to mark the script executable.

## Showing the auth prompt immediately

By default, XSecureLock starts in the saver/blank state and opens the auth
prompt after user activity. If you want the auth prompt visible immediately
after the screen is locked, use the post-lock command hook to send `SIGUSR2`
after XSecureLock has finished acquiring its grabs:

```
xsecurelock -- /bin/sh -c 'kill -USR2 "$PPID"'
```

Avoid starting XSecureLock in the background and immediately signaling it from
the parent shell; that can race with startup before the signal handler is
installed.

## Running from systemd

XSecureLock must run inside the graphical X11 session it is locking. A system
service, or a user service started without the session environment, cannot
usually open the display and will fail with an error such as `Could not connect
to $DISPLAY`.

For automatic locking, prefer running XSecureLock through the session itself,
for example with `xss-lock` as shown below. If you do wrap it in a
`systemd --user` service, import the X11 environment after graphical login
before starting the service:

```
systemctl --user import-environment DISPLAY XAUTHORITY
dbus-update-activation-environment --systemd DISPLAY XAUTHORITY
```

The exact unit wiring is desktop-specific, but the important invariant is that
the process must have the same `DISPLAY` and X authority as the logged-in X11
session.

# Automatic Locking

To automatically lock the screen after some time of inactivity, use
[xss-lock](https://github.com/wavexx/xss-lock) as follows:

```
xset s 300 5
xss-lock -n /usr/lib/xsecurelock/dimmer -l -- xsecurelock
```

The option `-l` is critical because it transfers logind's delay lock to
`xsecurelock`. This delays suspend until `xsecurelock` has closed
`XSS_SLEEP_LOCK_FD`, so previous screen content is not briefly visible after
wakeup.

This is a readiness signal, not a suspend cancellation mechanism. If
`xsecurelock` exits before locking, for example because another client is
holding a pointer grab, the delay lock is closed and suspend can continue. For
suspend paths that must tolerate a held mouse button or a similar temporary
grab, use the opt-in force-grab mode:

```
xss-lock -n /usr/lib/xsecurelock/dimmer -l -- env XSECURELOCK_FORCE_GRAB=1 xsecurelock
```

See `XSECURELOCK_FORCE_GRAB` and "Forcing Grabs" below for the tradeoffs; it can
interfere with some window managers and should not be enabled blindly.

NOTE: When using `xss-lock`, it's recommended to not launch `xsecurelock`
directly for manual locking, but to manually lock using `xset s activate`. This
ensures that `xss-lock` knows about the locking state and won't try again, which
would spam the X11 error log.

WARNING: Never rely on automatic locking for security, for the following
reasons:

-   An attacker can, of course, use your computer after you leave it alone and
    before it locks or you return.

-   Automatic locking is unreliable by design - for example, it could simply be
    misconfigured, or a pointer grab (due to open context menu) could prevent
    the screen lock from ever activating. Media players also often suspend
    screen saver activation for usability reasons.

Automatic locking should merely be seen as a fallback for the case of the user
forgetting to lock explicitly, and not as a security feature. If you really want
to use this as a security feature, make sure to kill the session whenever
attempts to lock fail (in which case `xsecurelock` will return a non-zero exit
status).

## Alternatives

### xautolock

`xautolock` can be used instead of `xss-lock` as long as you do not care for
suspend events (like on laptops):

```
xautolock -time 10 -notify 5 -notifier '/usr/lib/xsecurelock/until_nonidle /usr/lib/xsecurelock/dimmer' -locker xsecurelock
```

### Possible other tools

Ideally, an environment integrating `xsecurelock` should provide the following
facilities:

1.  Wait for one of the following events:
    1.  When idle for a sufficient amount of time:
        1.  Run `dimmer`.
        2.  When no longer idle while dimmed, kill `dimmer` and go back to the
            start.
        3.  When `dimmer` exits, run `xsecurelock` and wait for it.
    2.  When locking was requested, run `xsecurelock` and wait for it.
    3.  When suspending, run `xsecurelock` while passing along
        `XSS_SLEEP_LOCK_FD` and wait for it.
2.  Repeat.

This is, of course, precisely what `xss-lock` does, and - apart from the suspend
handling - what `xautolock` does.

As an alternative, we also support this way of integrating:

1.  Wait for one of the following events:
    1.  When idle for a sufficient amount of time:
        1.  Run `until_nonidle dimmer || exec xsecurelock` and wait for it.
        2.  Reset your idle timer (optional when your idle timer is either the
            X11 Screen Saver extension's idle timer or the X Synchronization
            extension's `"IDLETIME"` timer, as this command can never exit
            without those being reset).
    2.  When locking was requested, run `xsecurelock` and wait for it.
    3.  When suspending, run `xsecurelock` while passing along
        `XSS_SLEEP_LOCK_FD` and wait for it.
2.  Repeat.

NOTE: When using `until_nonidle` with dimming tools other than the included
`dimmer`, please set `XSECURELOCK_DIM_TIME_MS` and `XSECURELOCK_WAIT_TIME_MS` to
match the time your dimming tool takes for dimming, and how long you want to
wait in dimmed state before locking.

# Options

Options to XSecureLock can be passed by environment variables:

<!-- ENV VARIABLES START -->

*   `XSECURELOCK_AUTH`: specifies the desired authentication module (the part
    that displays the authentication prompt).
*   `XSECURELOCK_AUTHPROTO`: specifies the desired authentication protocol
    module (the part that talks to the system).
*   `XSECURELOCK_AUTH_BACKGROUND_COLOR`: specifies the X11 color (see manpage of
    XParseColor) for the background of the auth dialog.
*   `XSECURELOCK_AUTH_CURSOR_BLINK`: if set, the cursor will blink in the auth
    dialog. Enabled by default, can be set to 0 to disable.
*   `XSECURELOCK_AUTH_SOUNDS`: specifies whether to play sounds during
    authentication to indicate status. Sounds are defined as follows:
    *   High-pitch ascending: prompt for user input.
    *   High-pitch constant: an info message was displayed.
    *   Low-pitch descending: an error message was displayed.
    *   Medium-pitch ascending: authentication successful.
*   `XSECURELOCK_AUTH_FOREGROUND_COLOR`: specifies the X11 color (see manpage of
    XParseColor) for the foreground text of the auth dialog.
*   `XSECURELOCK_AUTH_TIMEOUT`: specifies the time (in seconds) to wait for
    response to an auth prompt or static PAM message shown by `auth_x11`
    before giving up and reverting to the screen saver. This also applies to
    PAM modules that display informational messages while waiting for external
    input, such as U2F prompts. This does not itself blank or power off the
    display; use `XSECURELOCK_BLANK_TIMEOUT` and
    `XSECURELOCK_BLANK_DPMS_STATE` to control what happens after the auth prompt
    closes.
*   `XSECURELOCK_AUTH_TITLE`: custom title prefix for the login screen of
    `auth_x11`. When set, this replaces the generated `username@hostname`
    prefix while keeping the current authentication prompt/status title after
    it.
*   `XSECURELOCK_AUTH_WARNING_COLOR`: specifies the X11 color (see manpage of
    XParseColor) for the warning text of the auth dialog.
*   `XSECURELOCK_AUTH_PADDING`: padding between auth dialog content and its
    optional border. Values are clamped to 0 through 4096. Defaults to 16.
    Set to 0 for a tighter dialog.
*   `XSECURELOCK_AUTH_BORDER_SIZE`: border stroke width for the auth dialog.
    Values are clamped to 0 through 4096. Defaults to 0, which disables the
    border entirely.
*   `XSECURELOCK_AUTH_X_POSITION`: horizontal position of the auth dialog as a
    percentage of screen width, from 0 (left) to 100 (right). Defaults to 50
    (horizontally centered). For example, set to 0 together with
    `XSECURELOCK_AUTH_Y_POSITION=0` to place the dialog near the top-left.
*   `XSECURELOCK_AUTH_Y_POSITION`: vertical position of the auth dialog as a
    percentage of screen height, from 0 (top) to 100 (bottom). Defaults to 50
    (vertically centered). For example, set to 80 to position the dialog near
    the bottom of the screen.
*   `XSECURELOCK_BACKGROUND_COLOR`: specifies the X11 color (see manpage
    of XParseColor) for the background of the main and saver windows.
*   `XSECURELOCK_BLANK_TIMEOUT`: specifies the time (in seconds) before telling
    X11 to fully blank the screen; a negative value disables xsecurelock-managed
    blanking. The time is measured since the closing of the auth window or
    xsecurelock startup. Set this to `0` if you want xsecurelock to reblank
    immediately after the auth window closes, for example after
    `XSECURELOCK_AUTH_TIMEOUT`. For manual-lock workflows, a small positive
    value may be less surprising because key-release events from launching
    xsecurelock or closing the auth dialog can wake the screen again. Set this
    to `-1` if you want external DPMS or screen saver policy to control blanking
    timing instead of xsecurelock.
*   `XSECURELOCK_BLANK_DPMS_STATE`: specifies which DPMS state to put the screen
    in when xsecurelock performs blanking (one of standby, suspend, off and on,
    where "on" means to not invoke DPMS at all). This setting only matters when
    `XSECURELOCK_BLANK_TIMEOUT` is nonnegative.

    For example, to close the auth prompt after 5 seconds and immediately ask
    X11 to power off the display again:

        XSECURELOCK_AUTH_TIMEOUT=5 XSECURELOCK_BLANK_TIMEOUT=0 \
        XSECURELOCK_BLANK_DPMS_STATE=off xsecurelock
*   `XSECURELOCK_BURNIN_MITIGATION`: specifies the number of pixels the prompt
    of `auth_x11` may be moved at startup to mitigate possible burn-in
    effects due to the auth dialog being displayed all the time (e.g. when
    spurious mouse events wake up the screen all the time).
*   `XSECURELOCK_BURNIN_MITIGATION_DYNAMIC`: if set to a non-zero value,
    `auth_x11` will move the prompt while it is being displayed, but stay
    within the bounds of `XSECURELOCK_BURNIN_MITIGATION`. The value of this
    variable is the maximum allowed shift per screen refresh. This mitigates
    short-term burn-in effects but is probably annoying to most users, and thus
    disabled by default.
*   `XSECURELOCK_COMPOSITE_OBSCURER`: create a second, 1-pixel-inset window
    to obscure most window content in case a running compositor unmaps its own
    window.
    Helps with some instances of bad compositor behavior (such as compositor
    crashes/restarts, but also compton has been caught at drawing notification
    icons above the screen locker when not using the GLX backend), should
    prevent compositors from unredirecting because it is deliberately smaller
    than the screen on every side, and should otherwise be harmless, so it's
    enabled by default.
*   `XSECURELOCK_DATETIME_FORMAT`: the date format to show. Defaults to the
    locale settings. (see `man date` for possible formats)
*   `XSECURELOCK_DEBUG_ALLOW_LOCKING_IF_INEFFECTIVE`: Normally we don't allow
    locking sessions that are unlikely to be useful to lock, such as the X11
    part of a Wayland session (Wayland applications would remain usable while
    locked) or VNC sessions (which only lock the server-side session while users
    are likely to think they locked the client). These checks can be bypassed by
    setting this variable to 1. Not recommended other than for debugging
    XSecureLock itself via such connections.
*   `XSECURELOCK_DEBUG_WINDOW_INFO`: When complaining about another window
    misbehaving, print not just the window ID but also some info about it. Uses
    the `xwininfo` and `xprop` tools.
*   `XSECURELOCK_DIM_ALPHA`: Linear-space opacity to fade the screen to.
*   `XSECURELOCK_DIM_COLOR`: X11 color to fade the screen to.
*   `XSECURELOCK_DIM_FPS`: Target framerate to attain during the dimming effect
    of `dimmer`. Ideally matches the display refresh rate.
*   `XSECURELOCK_DIM_MAX_FILL_SIZE`: Maximum size (in width or height) to fill
    at once using an XFillRectangle call. Low values may cause performance loss
    or noticeable tearing during dimming; high values may cause crashes or hangs
    with some graphics drivers or a temporarily unresponsive X server.
*   `XSECURELOCK_DIM_OVERRIDE_COMPOSITOR_DETECTION`: When set to 1, always try
    to use transparency for dimming; when set to 0, always use a dither
    pattern. Default is to autodetect whether transparency will likely work.
*   `XSECURELOCK_DIM_TIME_MS`: Milliseconds to dim for when the xss-lock command
    line above is used with `dimmer`; also used by `until_nonidle` to know when
    to assume dimming and waiting has finished and exit.
*   `XSECURELOCK_DISCARD_FIRST_KEYPRESS`: If set to 0, the key pressed to stop
    the screen saver and spawn the auth child is sent to the auth child (and
    thus becomes part of the password entry). By default we always discard the
    key press that started the authentication flow, to prevent users from
    getting used to type their password on a blank screen (which could be just
    powered off and have a chat client behind or similar).
*   `XSECURELOCK_FONT`: X11 or FontConfig font name to use for `auth_x11`.
    You can get a list of supported font names by running `xlsfonts` and
    `fc-list`. Unicode prompt modes need a regular text font that contains the
    required glyphs; icon fonts and unsupported color emoji fonts may not render
    useful password feedback.
*   `XSECURELOCK_FORCE_GRAB`: When grabbing fails, try stealing the grab from
    other windows (a value of `2` steals from all descendants of the root
    window, while a value of `1` only steals from client windows). This works
    only sometimes and is incompatible with many window managers, so use with
    care. See the "Forcing Grabs" section below for details.
*   `XSECURELOCK_GLOBAL_SAVER`: specifies the desired global screen saver module
    (by default this is a multiplexer that runs `XSECURELOCK_SAVER` on each
    screen).
*   `XSECURELOCK_IDLE_TIMERS`: comma-separated list of idle time counters used
    by `until_nonidle`. Typical values are either empty (relies on the X Screen
    Saver extension instead), "IDLETIME" and "DEVICEIDLETIME <n>" where n is an
    XInput device index (run `xinput` to see them). If multiple time counters
    are specified, the idle time is the minimum of them all. All listed timers
    must have the same unit.
*   `XSECURELOCK_IMAGE_DURATION_SECONDS`: how long to show each still image
    played by `saver_mpv`. Defaults to 1.
*   `XSECURELOCK_KEY_%s_COMMAND`: shell command to execute when a special
    non-text key is pressed, such as media or brightness keys. `%s` is the
    case-sensitive X11 keysym name from `xev -event keyboard`, for example
    `XF86AudioPlay` or `XF86MonBrightnessUp`. Fn usually changes the emitted
    keysym rather than acting as a separate X11 modifier. Beware: be cautious
    about what you run with this, as it may give attackers control over your
    computer.
*   `XSECURELOCK_LIST_VIDEOS_COMMAND`: shell command to list all video files to
    potentially play by `saver_mpv` or `saver_mplayer`. Defaults to
    `find ~/Videos -type f`.
*   `XSECURELOCK_AUTO_RAISE`: periodically try to raise the lock windows.
    Disabled by default. This is a compositor compatibility workaround for
    notification stacks that do not generate useful visibility events.
*   `XSECURELOCK_NO_COMPOSITE`: disables covering the composite overlay window.
    This switches to a more traditional way of locking, but may allow desktop
    notifications to be visible on top of the screen lock. Not recommended.
*   `XSECURELOCK_NO_PAM_RHOST`: do not set `PAM_RHOST` to `localhost`, despite
    [recommendation](http://www.linux-pam.org/Linux-PAM-html/adg-security-user-identity.html)
    to do so by the Linux-PAM Application Developers' Guide. This may work
    around bugs in third-party PAM authentication modules. If this solves a
    problem for you, please report a bug against said PAM module.
*   `XSECURELOCK_ALLOW_NULL_PAM_AUTHTOK`: allow PAM authentication to accept
    null authentication tokens if the PAM stack permits them. Disabled by
    default so empty-password PAM policy does not unlock an existing session
    unless you opt in explicitly.
*   `XSECURELOCK_NO_XRANDR`: disables multi-monitor support using XRandR.
*   `XSECURELOCK_NO_XRANDR15`: disables multi-monitor support using XRandR 1.5
    and falls back to XRandR 1.2. Not recommended.
*   `XSECURELOCK_PAM_SERVICE`: PAM service name. You should have a file with
    that name in `/etc/pam.d`.
*   `XSECURELOCK_PASSWORD_PROMPT`: Choose password prompt mode:
    *   `asterisks`: shows asterisks, like classic password prompts. This is
        the least secure option because password length is visible.

            ***_
            *******_

    *   `cursor`: shows a cursor that jumps around on each key press. This is
        the default.

            ________|_______________________
            ___________________|____________

    *   `disco`: shows dancers, which dance around on each key press. Requires a
        font that can handle Unicode line drawing characters, and FontConfig.

            ┏(･o･)┛ ♪ ┗(･o･)┓ ♪ ┏(･o･)┛ ♪ ┗(･o･)┓ ♪ ┏(･o･)┛
            ┗(･o･)┓ ♪ ┏(･o･)┛ ♪ ┏(･o･)┛ ♪ ┏(･o･)┛ ♪ ┏(･o･)┛

    *   `emoji`: shows an emoji, changing which one on each key press. Requires
        a font that can handle emoji, and FontConfig.

            👍
            🎶
            💕

    *   `emoticon`: shows an ASCII emoticon, changing which one on each key
        press.

            :-O
            d-X
            X-\

    *   `hidden`: completely hides the password, and there's no feedback for
        keypresses. This would almost be the most secure mode, but because it
        gives no feedback at all, you may not notice accidentally typing to
        another computer and sending your password to a chatroom.

        ```
        ```

    *   `kaomoji`: shows a kaomoji (Japanese emoticon), changing which one on
        each key press. Requires a Japanese font, and FontConfig.

            (͡°͜ʖ͡°)
            (＾ｕ＾)
            ¯\_(ツ)_/¯

    *   `time`: shows the current time since the epoch on each keystroke. This
        may be the most secure mode, as it gives feedback to keystroke based
        exclusively on public information, and does not carry over any state
        between keystrokes whatsoever - not even some form of randomness.

            1559655410.922329

    *   `time_hex`: same as `time`, but in microseconds and hexadecimal.
        "Because we can".

            0x58a7f92bd7359

*   `XSECURELOCK_SAVER`: specifies the desired screen saver module.
*   `XSECURELOCK_SAVER_RESET_ON_AUTH_CLOSE`: specifies whether to reset the
    saver module when the auth dialog closes. Resetting is done by sending
    `SIGUSR1` to the saver, which may either just terminate, or handle this
    specifically to do a cheaper reset.
*   `XSECURELOCK_SAVER_NOTIFY_ON_AUTH_OPEN`: specifies whether to notify the
    saver module when the auth dialog opens. Notification is done by sending
    `SIGUSR2` to the saver. Disabled by default.
*   `XSECURELOCK_SHOW_DATETIME`: whether to show local date and time on the
    login. Disabled by default.
*   `XSECURELOCK_SHOW_HOSTNAME`: whether to show the hostname on the login
    screen of `auth_x11`. Possible values are 0 for not showing the
    hostname, 1 for showing the short form, and 2 for showing the long form.
*   `XSECURELOCK_SHOW_KEYBOARD_LAYOUT`: whether to show the name of the current
    keyboard layout. Enabled by default.
*   `XSECURELOCK_LAYOUT_SWITCH_KEYSYM`: X11 keysym to use with Ctrl for
    switching keyboard layouts. Defaults to `Tab`; set this to `space` for
    Ctrl-Space. Use the case-sensitive keysym name from `xev -event keyboard`.
*   `XSECURELOCK_SHOW_USERNAME`: whether to show the username on the login
    screen of `auth_x11`.
*   `XSECURELOCK_SINGLE_AUTH_WINDOW`: whether to show only a single auth window
    from `auth_x11`, as opposed to one per screen.
*   `XSECURELOCK_SWITCH_USER_COMMAND`: shell command to execute when `Win-O` or
    `Ctrl-Alt-O` are pressed (think "_other_ user"). Typical values could be
    `lxdm -c USER_SWITCH`, `dm-tool switch-to-greeter`, `gdmflexiserver` or
    `kdmctl reserve`, depending on your desktop environment.
*   `XSECURELOCK_VIDEOS_FLAGS`: flags to append when invoking mpv/mplayer with
    `saver_mpv` or `saver_mplayer`. Defaults to empty.
*   `XSECURELOCK_WAIT_TIME_MS`: Milliseconds to wait after dimming (and before
    locking) when the xss-lock command line above is used. Should be at least as
    large as the period set using `xset s`. Also used by `until_nonidle` to know
    when to assume dimming and waiting has finished and exit.
*   `XSECURELOCK_SAVER_DELAY_MS`: Milliseconds to wait after starting child
    processes and before mapping windows, to let children become ready to
    display and reduce the black flash.
*   `XSECURELOCK_SAVER_STOP_ON_BLANK`: specifies if the saver is stopped
    when the screen is blanked (DPMS or XSS) to save power. This only controls the
    saver child; it does not change who decides when blanking happens. Set this
    to 0 if `xss-lock` is only triggering the lock and you want the configured
    saver to remain visible until xsecurelock's own blank/DPMS timeout.
*   `XSECURELOCK_XSCREENSAVER_PATH`: location where XScreenSaver hacks are
    installed for use by `saver_xscreensaver`.

<!-- ENV VARIABLES END -->

Additionally, command line arguments following `--` are executed via `execvp`
once locking is successful. This is intended for callers that need a reliable
"locked" notification; unlike the sleep-lock file descriptor used by
`xss-lock -l`, it is not run when locking fails during startup.

Set environment variables before `xsecurelock`, for example:

```sh
XSECURELOCK_PASSWORD_PROMPT=disco xsecurelock
```

# Authentication Modules

The following authentication modules are included:

*   `auth_x11`: Authenticates via an authproto module using keyboard input (X11
    based; recommended).

## Writing Your Own Module

The authentication module is a separate executable, whose name must start with
`auth_` and be installed together with the included `auth_` modules (default
location: `/usr/local/libexec/xsecurelock`).

*   Input: it may receive keystroke input from standard input in a
    locale-dependent multibyte encoding (usually UTF-8). Use the `mb*` C
    functions to act on these.
*   Output: it may draw on or create windows below `$XSCREENSAVER_WINDOW`.
*   Exit status: if authentication was successful, it must return with status
    zero. If it returns with any other status (including e.g. a segfault),
    XSecureLock assumes failed authentication.
*   It is recommended that it spawn the configured authentication protocol
    module and let it do the actual authentication; that way the authentication
    module can focus on the user interface alone.

In other words, an authentication module owns the user interface and returns
zero only after the user has successfully authenticated. The default `auth_x11`
module starts the configured `XSECURELOCK_AUTHPROTO`, translates keyboard input
and authproto packets into prompts/messages, and exits with the authproto
result.

Custom auth modules should treat a crash or nonzero exit as failed
authentication, because the main locker will keep the session locked. If a
custom UI needs system authentication, prefer speaking the authproto protocol
to a small `authproto_` helper instead of linking privileged or PAM-specific
logic directly into the graphical UI.

# Authentication Protocol Modules

The following authentication protocol ("authproto") modules are included:

*   `authproto_htpasswd`: Authenticates via a htpasswd style file stored in
    `~/.xsecurelock.pw`. To generate this file, run: `( umask 077; htpasswd -cB
    ~/.xsecurelock.pw "$USER" )`. Use this only if you for some reason can't use
    PAM!
*   `authproto_pam`: Authenticates via PAM. Use this.
*   `authproto_pamtester`: Authenticates via PAM using pamtester. Shouldn't
    be required unless you can't compile `authproto_pam`. Only supports simple
    password based conversations.

## Writing Your Own Module

The authentication protocol module is a separate executable, whose name must
start with `authproto_` and be installed together with the included
`authproto_` modules (default location:
`/usr/local/libexec/xsecurelock`).

*   Input: in response to some output messages, it may receive authproto
    messages. See helpers/authproto.h for details.
*   Output: it should output authproto messages; see helpers/authproto.h for
    details.
*   Exit status: if authentication was successful, it must return with status
    zero. If it returns with any other status (including e.g. a segfault),
    XSecureLock assumes failed authentication.

The packet format is:

```
<type> <byte-length>
<payload>
```

where `<payload>` is exactly `<byte-length>` bytes followed by a newline. For
example, an authproto can ask for a password by writing:

```
P 9
Password:
```

It then reads one response packet from standard input:

*   `p`: password-like response.
*   `u`: username-like response.
*   `x`: explicit cancellation, such as auth timeout or user cancellation.

Info and error packets use lowercase types and do not require a direct
response:

*   `i`: informational message, such as "Please touch the device".
*   `e`: error message.

Only uppercase request packets expect a response. Info/error packets and
response packets are one-way. Use exit status zero only for successful
authentication; all other statuses keep the screen locked.

# Screen Saver Modules

The following screen saver modules are included:

*   `saver_blank`: Simply blanks the screen.
*   `saver_mplayer` and `saver_mpv`: Plays a video using mplayer or mpv,
    respectively. The video to play is selected at random among all files in
    `~/Videos`.
*   `saver_multiplex`: Watches the display configuration and runs another screen
    saver module once on each screen; used internally.
*   `saver_xscreensaver`: Runs an XScreenSaver hack from an existing
    XScreenSaver setup. It executes the selected `programs` entry from your
    trusted `~/.xscreensaver` configuration as a command for compatibility with
    XScreenSaver. NOTE: some screen savers included by this may display
    arbitrary pictures from your home directory; if you care about this, either
    run `xscreensaver-demo` and disable screen savers that may do this, or stay
    away from this one!

## Writing Your Own Module

The screen saver module is a separate executable, whose name must start with
`saver_` and be installed together with the included `saver_` modules (default
location: `/usr/local/libexec/xsecurelock`).

*   Input: receives the 0-based index of the screen saver (remember: one saver
    is started per display by the multiplexer) via `$XSCREENSAVER_SAVER_INDEX`.
*   Output: it may draw on or create windows below `$XSCREENSAVER_WINDOW`.
*   Exit condition: the saver child will receive SIGTERM when the user wishes to
    unlock the screen. It should exit promptly.
*   Reset condition: the saver child will receive SIGUSR1 when the auth dialog
    is closed and `XSECURELOCK_SAVER_RESET_ON_AUTH_CLOSE`.
*   Auth-open notification: the saver child will receive SIGUSR2 when the auth
    dialog opens and `XSECURELOCK_SAVER_NOTIFY_ON_AUTH_OPEN` is enabled.

The saver is a visual helper only. The main xsecurelock process owns the actual
lock windows and grabs, so a saver crash does not unlock the screen. A saver may
therefore be a small wrapper around another program, as long as that program can
draw into or below `$XSCREENSAVER_WINDOW` and exits promptly on `SIGTERM`.

Minimal custom saver:

```sh
#!/bin/sh
exec sleep 86400
```

Run a program that can embed into an existing X11 window, such as `xterm`:

```sh
#!/bin/sh
exec xterm -into "$XSCREENSAVER_WINDOW" -e htop
```

Show one image per monitor with `mpv`:

```sh
#!/bin/sh
image=${XSECURELOCK_IMAGE_PATH:-"$HOME/Pictures/lock.png"}
exec mpv --no-input-terminal --really-quiet --no-audio \
  --no-stop-screensaver --wid="$XSCREENSAVER_WINDOW" --loop-file=inf "$image"
```

For multi-monitor setups, `saver_multiplex` starts one saver per monitor and
sets `$XSCREENSAVER_SAVER_INDEX` to `0`, `1`, and so on. A custom saver can use
that index to choose monitor-specific content:

```sh
#!/bin/sh
case "$XSCREENSAVER_SAVER_INDEX" in
  0) image=$HOME/Pictures/left-lock.png ;;
  1) image=$HOME/Pictures/right-lock.png ;;
  *) image=$HOME/Pictures/lock.png ;;
esac
exec mpv --no-input-terminal --really-quiet --no-audio \
  --no-stop-screensaver --wid="$XSCREENSAVER_WINDOW" --loop-file=inf "$image"
```

Saver commands run with the user's privileges and environment. Treat custom
saver scripts and `~/.xscreensaver` program entries as trusted configuration,
and avoid displaying sensitive files unless that is intentional.

# Security Design

To reduce the risk of screen lock bypasses, XSecureLock uses the following
design measures:

*   Authentication dialog, authentication checking and screen saving are done
    using separate processes. Therefore a crash of these processes will not
    unlock the screen, which means that these processes are allowed to do
    "possibly dangerous" things.
*   This also means that on operating systems where authentication checking
    requires special privileges (such as FreeBSD), only that module can be set
    to run at elevated privileges, unlike most other screen lockers which in
    this scenario also run graphical user interface code as root.
*   The main process is kept minimal and only uses C, POSIX and X11 APIs. This
    limits the possible influence from bugs in external libraries, and allows
    for easy auditing.
*   The main process regularly refreshes the screen grabs in case they get lost
    for whatever reason.
*   When XComposite is available, the main process uses the composite overlay
    window to reduce lock-screen transparency issues. It also reacts to
    visibility and configure events to keep its windows on top, and can use
    `XSECURELOCK_AUTO_RAISE=1` as a compatibility fallback on compositor stacks
    where those events are insufficient.
*   The main process resizes its window to the size of the root window, should
    the root window size change, to avoid leaking information by attaching a
    secondary display.
*   The main process uses only a single buffer - to hold a single keystroke.
    Therefore it is impossible to exploit a buffer overrun in the main process
    by e.g. an overlong password entry.
*   The only exit condition of the program is the Authentication Module
    returning with exit status zero, in which case xsecurelock itself will
    return with status zero; therefore especially security-conscious users might
    want to run it as `sh -c "xsecurelock ... || kill -9 -1"` :)

# Known Security Issues

*   Locking the screen will fail while other applications already have a
    keyboard or pointer grab open (for example while running a fullscreen game,
    or after opening a context menu). This will be noticeable as the screen will
    not turn black and should thus usually not be an issue - however when
    relying on automatic locking via `xss-lock`, this could leave a workstation
    open for days. The `... || kill -9 -1` workaround above would mitigate this
    issue too by simply killing the entire session if locking it fails.
*   As XSecureLock relies on an event notification after a screen configuration
    change, window content may be visible for a short time after attaching a
    monitor. No usual interaction with applications should be possible though.
    On desktop systems where monitors are usually not hotplugged, consider
    turning off automatic screen reconfiguration.
*   XSecureLock relies on a keyboard and pointer grab in order to prevent other
    applications from receiving keyboard events (and thus an unauthorized user
    from controlling the machine). However, there are various other ways for
    applications - in particular games - to receive input:
    *   Polling current keyboard status (`XQueryKeymap`).
    *   Polling current mouse position (`XQueryPointer`).
    *   Receiving input out-of-band (`/dev/input`), including other input
        devices than keyboard and mouse, such as gamepads or joysticks.

Most of these issues are inherent with X11 and can only really be fixed by
migrating to an alternative such as Wayland; some of the issues (in particular
the gamepad input issue) will probably persist even with Wayland.

## Forcing Grabs

As a workaround to the issue of another window already holding a grab, we offer
an `XSECURELOCK_FORCE_GRAB` option.

This adds a last-resort attempt to force grabbing by iterating through all
subwindows of the root window, unmapping them (which closes down their grabs),
then taking the grab and mapping them again.

This has the following known issues:

*   Grabs owned by the root window cannot be closed down this way. However,
    only screen lockers and fullscreen games should be doing that.
*   If the grab was owned by a full screen window (e.g. a game using
    `OverrideRedirect` to gain fullscreen mode), the window will become
    unresponsive, as your actions will be interpreted by another window instead
    - one you can't see. Alt-Tabbing around may often work around this.
*   If the grab was owned by a context menu, it may become impossible to close
    the menu other than by selecting an item in it.
*   It will also likely confuse window managers:
    *   Probably all window managers will rearrange the windows in response to
        this.
    *   Cinnamon (and probably other GNOME-derived WMs) may become unresponsive
        and need to be restarted.
        *   As a mitigation we try to hit only client windows - but then we
            lose the ability of closing down window manager owned grabs.
*   Negative side effects as described are still likely to happen in case the
    measure fails.

# Known Compatibility Issues

*   There is an open issue with the NVIDIA graphics driver in conjunction with
    some compositors. Workarounds include switching to the `nouveau` graphics
    driver, using a compositor that uses the Composite Overlay Window (e.g.
    `compton` with the flags `--backend glx --paint-on-overlay`) or passing
    `XSECURELOCK_NO_COMPOSITE=1` to XSecureLock (which however may make
    notifications appear on top of the screen lock).

*   XSecureLock is incompatible with the compositor built into `metacity` (a
    GNOME component) because it draws on the Compositor Overlay Window with
    `IncludeInferiors` set (i.e. it explicitly requests to draw on top of
    programs like XSecureLock). It likely does this because the same is
    necessary when drawing on top of the root window, which it had done in the
    past but no longer does. Workarounds include disabling its compositor with
    `gsettings set org.gnome.metacity compositing-manager false` or passing
    `XSECURELOCK_NO_COMPOSITE=1` to XSecureLock.

*   Picom doesn't remove windows in the required order, causing a window with
    the text "INCOMPATIBLE COMPOSITOR, PLEASE FIX!" to be displayed. To fix this
    you can disable the composite obscurer with
    `XSECURELOCK_COMPOSITE_OBSCURER=0` to stop the window from being drawn
    altogether.

*   In general, most compositor issues will become visible in form of a text
    "INCOMPATIBLE COMPOSITOR, PLEASE FIX!" being displayed. A known good
    compositor is `compton --backend glx --paint-on-overlay`. In worst case
    try `XSECURELOCK_AUTO_RAISE=1` first; if that still does not help, you can
    turn off our workaround for transparent windows by setting
    `XSECURELOCK_NO_COMPOSITE=1`.

# License

The code is released under the Apache 2.0 license. See the LICENSE file for more
details.
