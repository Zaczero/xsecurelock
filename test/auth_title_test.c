// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <string.h>

#include "../helpers/auth_title.h"

static void Build(char *output, size_t output_size, const char *auth_title,
                  bool show_username, int show_hostname, const char *input) {
  AuthBuildTitle(output, output_size, auth_title, show_username, show_hostname,
                 "alice", "workstation.example", input);
}

static void TestDefaultIdentityTitle(void) {
  char buf[64];

  Build(buf, sizeof(buf), "", true, 1, "Password:");
  assert(strcmp(buf, "alice@workstation - Password:") == 0);
}

static void TestCustomTitleOverridesIdentityOnly(void) {
  char buf[64];

  Build(buf, sizeof(buf), "Locked", true, 1, "Password:");
  assert(strcmp(buf, "Locked - Password:") == 0);
}

static void TestCustomTitleWithoutPromptTitle(void) {
  char buf[64];

  Build(buf, sizeof(buf), "Locked", true, 1, "");
  assert(strcmp(buf, "Locked") == 0);
}

static void TestNoIdentityAvoidsLeadingSeparator(void) {
  char buf[64];

  Build(buf, sizeof(buf), "", false, 0, "Password:");
  assert(strcmp(buf, "Password:") == 0);
}

static void TestLongTitleTruncatesAndTerminates(void) {
  char buf[8];

  Build(buf, sizeof(buf), "VeryLongTitle", true, 1, "Password:");
  assert(strcmp(buf, "VeryLon") == 0);
}

int main(void) {
  TestDefaultIdentityTitle();
  TestCustomTitleOverridesIdentityOnly();
  TestCustomTitleWithoutPromptTitle();
  TestNoIdentityAvoidsLeadingSeparator();
  TestLongTitleTruncatesAndTerminates();
  return 0;
}
