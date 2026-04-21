#include <assert.h>
#include <string.h>

#include "../helpers/prompt_display.h"

static void ExpectDiscoPrompt(size_t displaymarker, const char *expected) {
  char displaybuf[256];
  size_t displaylen = 0;

  assert(FormatDiscoPrompt(displaymarker, displaybuf, sizeof(displaybuf),
                           &displaylen) == 0);
  assert(strcmp(displaybuf, expected) == 0);
  assert(displaylen == strlen(expected));
  assert(displaylen == strlen(displaybuf));
}

static void ExpectDiscoPromptFailure(size_t displaymarker, size_t bufsize) {
  char displaybuf[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', '\0'};
  size_t displaylen = 123;

  assert(bufsize <= sizeof(displaybuf));
  assert(FormatDiscoPrompt(displaymarker, displaybuf, bufsize, &displaylen) !=
         0);
  assert(displaylen == 0);
  assert(displaybuf[0] == '\0');
}

int main(void) {
  ExpectDiscoPrompt(
      0,
      "┏(･o･)┛ ♪ ┏(･o･)┛ ♪ ┏(･o･)┛ ♪ ┏(･o･)┛ ♪ ┏(･o･)┛");
  ExpectDiscoPrompt(
      31,
      "┗(･o･)┓ ♪ ┗(･o･)┓ ♪ ┗(･o･)┓ ♪ ┗(･o･)┓ ♪ ┗(･o･)┓");
  ExpectDiscoPrompt(
      21,
      "┗(･o･)┓ ♪ ┏(･o･)┛ ♪ ┗(･o･)┓ ♪ ┏(･o･)┛ ♪ ┗(･o･)┓");

  ExpectDiscoPromptFailure(0, 8);
  ExpectDiscoPromptFailure(0, 1);

  return 0;
}
