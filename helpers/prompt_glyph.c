#include "config.h"

#include "prompt_glyph.h"

#include <assert.h>

static int IsUtf8Continuation(unsigned char ch) {
  return (ch & 0xC0u) == 0x80u;
}

// Prompt editing only needs stable UTF-8 sequence boundaries for masking and
// backspace. This is intentionally not a full grapheme cluster parser or a
// full UTF-8 validator: any non-continuation byte starts a glyph.

size_t PromptGlyphCount(const char *input, size_t input_length) {
  size_t count = 0;
  size_t offset = 0;

  assert(input != NULL || input_length == 0);

  while (offset < input_length) {
    if (!IsUtf8Continuation((unsigned char)input[offset])) {
      ++count;
    }
    ++offset;
  }

  return count;
}

size_t PromptPreviousGlyphStart(const char *input, size_t input_length) {
  size_t offset = input_length;
  size_t continuation_count = 0;

  assert(input != NULL || input_length == 0);

  while (offset > 0 && continuation_count < 3 &&
         IsUtf8Continuation((unsigned char)input[offset - 1])) {
    --offset;
    ++continuation_count;
  }

  while (offset > 0 && IsUtf8Continuation((unsigned char)input[offset - 1])) {
    --offset;
  }

  return offset > 0 ? offset - 1 : 0;
}
