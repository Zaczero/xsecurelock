#include "config.h"

#include "prompt_display.h"

#include <string.h>

static const char *const kDiscoCombiner = " ♪ ";
static const char *const kDiscoDancers[] = {
    "┏(･o･)┛",
    "┗(･o･)┓",
};

static int AppendPromptFragment(char *displaybuf, size_t displaybufsize,
                                size_t *used, const char *fragment) {
  size_t fragment_length = strlen(fragment);

  if (*used >= displaybufsize ||
      fragment_length > displaybufsize - *used - 1) {
    return -1;
  }

  memcpy(displaybuf + *used, fragment, fragment_length);
  *used += fragment_length;
  return 0;
}

int FormatDiscoPrompt(size_t displaymarker, char *displaybuf,
                      size_t displaybufsize, size_t *displaylen) {
  size_t used = 0;

  if (displaybuf == NULL || displaylen == NULL || displaybufsize == 0) {
    return -1;
  }

  for (size_t i = 0, bit = 1; i < DISCO_PASSWORD_DANCERS; ++i, bit <<= 1) {
    if (i != 0 &&
        AppendPromptFragment(displaybuf, displaybufsize, &used,
                             kDiscoCombiner) != 0) {
      goto fail;
    }
    if (AppendPromptFragment(
            displaybuf, displaybufsize, &used,
            kDiscoDancers[(displaymarker & bit) ? 1 : 0]) != 0) {
      goto fail;
    }
  }

  displaybuf[used] = '\0';
  *displaylen = used;
  return 0;

fail:
  displaybuf[0] = '\0';
  *displaylen = 0;
  return -1;
}
