#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "../buf_util.h"

static void TestAppendCString(void) {
  char buf[16] = "Hi";
  char *out = buf + strlen(buf);
  size_t remaining = sizeof(buf) - strlen(buf);

  assert(AppendCString(&out, &remaining, ", there") == 0);
  assert(strcmp(buf, "Hi, there") == 0);
  assert(out == buf + strlen("Hi, there"));
  assert(remaining == sizeof(buf) - strlen("Hi, there"));
}

static void TestAppendBytes(void) {
  char buf[8] = "";
  char *out = buf;
  size_t remaining = sizeof(buf);

  assert(AppendBytes(&out, &remaining, "abc", 3) == 0);
  assert(strcmp(buf, "abc") == 0);
  assert(out == buf + 3);
  assert(remaining == sizeof(buf) - 3);
}

static void TestAppendFailureKeepsState(void) {
  char buf[5] = "abc";
  char *out = buf + 3;
  char *saved_out = out;
  size_t remaining = sizeof(buf) - 3;
  size_t saved_remaining = remaining;

  errno = 0;
  assert(AppendCString(&out, &remaining, "xy") < 0);
  assert(errno == ENOSPC);
  assert(strcmp(buf, "abc") == 0);
  assert(out == saved_out);
  assert(remaining == saved_remaining);
}

static void TestAppendInvalidInput(void) {
  char buf[8] = "";
  char *out = buf;
  size_t remaining = sizeof(buf);

  errno = 0;
  assert(AppendBytes(NULL, &remaining, "a", 1) < 0);
  assert(errno == EINVAL);

  errno = 0;
  assert(AppendCString(&out, &remaining, NULL) < 0);
  assert(errno == EINVAL);
}

static void TestClearFreeHelpers(void) {
  char *string = malloc(8);
  assert(string != NULL);
  memcpy(string, "secret", 7);
  ClearFreeString(&string);
  assert(string == NULL);
  ClearFreeString(&string);
  assert(string == NULL);

  char *buffer = malloc(4);
  assert(buffer != NULL);
  memcpy(buffer, "abcd", 4);
  ClearFreeBuffer(&buffer, 4);
  assert(buffer == NULL);
  ClearFreeBuffer(&buffer, 4);
  assert(buffer == NULL);
}

int main(void) {
  TestAppendCString();
  TestAppendBytes();
  TestAppendFailureKeepsState();
  TestAppendInvalidInput();
  TestClearFreeHelpers();
  return 0;
}
