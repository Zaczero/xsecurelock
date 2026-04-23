#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../configured_command.h"
#include "../time_util.h"

static void WriteFileOrAbort(const char *path, const char *expected) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    abort();
  }

  char buf[128];
  size_t got = fread(buf, 1, sizeof(buf) - 1, file);
  fclose(file);
  buf[got] = '\0';

  if (strcmp(buf, expected) != 0) {
    abort();
  }
}

static void ExpectKeyCommandEnvFormats(void) {
  char buf[64] = "";
  if (!FormatKeyCommandEnvName(buf, sizeof(buf), "XF86AudioPlay")) {
    abort();
  }
  if (strcmp(buf, "XSECURELOCK_KEY_XF86AudioPlay_COMMAND") != 0) {
    abort();
  }
}

static void ExpectKeyCommandEnvRejectsTruncation(void) {
  char buf[16] = "";
  if (FormatKeyCommandEnvName(buf, sizeof(buf), "XF86AudioPlay")) {
    abort();
  }
}

static void ExpectForegroundShellCommandSucceeds(const char *tmpbase) {
  char path[PATH_MAX] = "";
  char command[PATH_MAX + 64] = "";

  if (snprintf(path, sizeof(path), "%s.foreground", tmpbase) <= 0) {
    abort();
  }
  unlink(path);
  if (snprintf(command, sizeof(command), "printf 'foreground' > '%s'", path) <=
      0) {
    abort();
  }

  if (!RunShellCommandValue("foreground command", command, 0)) {
    abort();
  }
  WriteFileOrAbort(path, "foreground");
}

static void ExpectForegroundShellCommandReportsFailure(void) {
  if (RunShellCommandValue("failing command", "exit 7", 0)) {
    abort();
  }
}

static void ExpectUnsetEnvDoesNothing(void) {
  unsetenv("XSECURELOCK_TEST_COMMAND");
  if (!RunShellCommandFromEnv("XSECURELOCK_TEST_COMMAND", 0)) {
    abort();
  }
}

static void ExpectEnvShellCommandRuns(const char *tmpbase) {
  char path[PATH_MAX] = "";
  char command[PATH_MAX + 96] = "";

  if (snprintf(path, sizeof(path), "%s.env", tmpbase) <= 0) {
    abort();
  }
  unlink(path);
  if (snprintf(command, sizeof(command), "printf '%s' '%s' > '%s'", "%s",
               "env works", path) <= 0) {
    abort();
  }
  setenv("XSECURELOCK_TEST_COMMAND", command, 1);

  if (!RunShellCommandFromEnv("XSECURELOCK_TEST_COMMAND", 0)) {
    abort();
  }
  WriteFileOrAbort(path, "env works");
  unsetenv("XSECURELOCK_TEST_COMMAND");
}

static void ExpectBackgroundShellCommandDetaches(const char *tmpbase) {
  char path[PATH_MAX] = "";
  char command[PATH_MAX + 128] = "";

  if (snprintf(path, sizeof(path), "%s.background", tmpbase) <= 0) {
    abort();
  }
  unlink(path);
  if (snprintf(command, sizeof(command), "sleep 1; printf 'background' > '%s'",
               path) <= 0) {
    abort();
  }

  if (!RunShellCommandValue("background command", command, 1)) {
    abort();
  }

  for (int attempt = 0; attempt < 20; ++attempt) {
    if (access(path, F_OK) == 0) {
      WriteFileOrAbort(path, "background");
      return;
    }
    if (SleepMs(100) != 0) {
      abort();
    }
  }
  abort();
}

int main(void) {
  char tmpbase[] = "/tmp/xsecurelock-configured-command.XXXXXX";
  int tmpfd = mkstemp(tmpbase);
  if (tmpfd < 0) {
    abort();
  }
  close(tmpfd);
  unlink(tmpbase);

  ExpectKeyCommandEnvFormats();
  ExpectKeyCommandEnvRejectsTruncation();
  ExpectForegroundShellCommandSucceeds(tmpbase);
  ExpectForegroundShellCommandReportsFailure();
  ExpectUnsetEnvDoesNothing();
  ExpectEnvShellCommandRuns(tmpbase);
  ExpectBackgroundShellCommandDetaches(tmpbase);
  return 0;
}
