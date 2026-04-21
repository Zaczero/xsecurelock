#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../helpers/indicator_text.h"

static void TestAppendStaticAndDynamic(void) {
  char buf[32] = "";
  char *output = buf;
  size_t output_size = sizeof(buf);
  int have_output = 0;

  assert(AppendIndicatorText(&output, &output_size, &have_output, "Shift"));

  char *dynamic = malloc(5);
  assert(dynamic != NULL);
  memcpy(dynamic, "Caps", 5);
  assert(AppendIndicatorText(&output, &output_size, &have_output, dynamic));

  assert(strcmp(buf, "Shift, Caps") == 0);
  free(dynamic);
}

static void TestFailedAppendLeavesBufferUntouched(void) {
  char buf[8] = "";
  char *output = buf;
  size_t output_size = sizeof(buf);
  int have_output = 0;

  assert(!AppendIndicatorText(&output, &output_size, &have_output,
                              "TooLongForBuffer"));
  assert(strcmp(buf, "") == 0);
  assert(output == buf);
  assert(output_size == sizeof(buf));
  assert(have_output == 0);
}

static void TestFailedAppendAfterExistingOutputLeavesStateUntouched(void) {
  char buf[12] = "";
  char *output = buf;
  size_t output_size = sizeof(buf);
  int have_output = 0;

  assert(AppendIndicatorText(&output, &output_size, &have_output, "Shift"));

  char snapshot[sizeof(buf)];
  memcpy(snapshot, buf, sizeof(buf));
  char *saved_output = output;
  size_t saved_output_size = output_size;
  int saved_have_output = have_output;

  assert(!AppendIndicatorText(&output, &output_size, &have_output, "TooLong"));
  assert(memcmp(buf, snapshot, sizeof(buf)) == 0);
  assert(output == saved_output);
  assert(output_size == saved_output_size);
  assert(have_output == saved_have_output);
}

int main(void) {
  TestAppendStaticAndDynamic();
  TestFailedAppendLeavesBufferUntouched();
  TestFailedAppendAfterExistingOutputLeavesStateUntouched();
  return 0;
}
