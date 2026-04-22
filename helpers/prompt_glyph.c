#include "config.h"

#include "prompt_glyph.h"

#include <assert.h>

static int IsContinuationByte(unsigned char ch) {
  return ch >= 0x80 && ch <= 0xBF;
}

// Prompt editing only needs stable UTF-8 sequence boundaries for masking and
// backspace. This is intentionally not a full grapheme cluster parser.
static size_t PromptNextGlyphLength(const char *input, size_t input_length) {
  unsigned char ch;

  assert(input != NULL);
  assert(input_length != 0);

  ch = (unsigned char)input[0];
  if (ch < 0x80) {
    return 1;
  }

  if (ch >= 0xC2 && ch <= 0xDF) {
    if (input_length >= 2 &&
        IsContinuationByte((unsigned char)input[1])) {
      return 2;
    }
    return 1;
  }

  if (ch == 0xE0) {
    if (input_length >= 3 && (unsigned char)input[1] >= 0xA0 &&
        (unsigned char)input[1] <= 0xBF &&
        IsContinuationByte((unsigned char)input[2])) {
      return 3;
    }
    return 1;
  }

  if ((ch >= 0xE1 && ch <= 0xEC) || (ch >= 0xEE && ch <= 0xEF)) {
    if (input_length >= 3 && IsContinuationByte((unsigned char)input[1]) &&
        IsContinuationByte((unsigned char)input[2])) {
      return 3;
    }
    return 1;
  }

  if (ch == 0xED) {
    if (input_length >= 3 && (unsigned char)input[1] >= 0x80 &&
        (unsigned char)input[1] <= 0x9F &&
        IsContinuationByte((unsigned char)input[2])) {
      return 3;
    }
    return 1;
  }

  if (ch == 0xF0) {
    if (input_length >= 4 && (unsigned char)input[1] >= 0x90 &&
        (unsigned char)input[1] <= 0xBF &&
        IsContinuationByte((unsigned char)input[2]) &&
        IsContinuationByte((unsigned char)input[3])) {
      return 4;
    }
    return 1;
  }

  if (ch >= 0xF1 && ch <= 0xF3) {
    if (input_length >= 4 && IsContinuationByte((unsigned char)input[1]) &&
        IsContinuationByte((unsigned char)input[2]) &&
        IsContinuationByte((unsigned char)input[3])) {
      return 4;
    }
    return 1;
  }

  if (ch == 0xF4) {
    if (input_length >= 4 && (unsigned char)input[1] >= 0x80 &&
        (unsigned char)input[1] <= 0x8F &&
        IsContinuationByte((unsigned char)input[2]) &&
        IsContinuationByte((unsigned char)input[3])) {
      return 4;
    }
    return 1;
  }

  return 1;
}

size_t PromptGlyphCount(const char *input, size_t input_length) {
  size_t count = 0;
  size_t offset = 0;

  assert(input != NULL || input_length == 0);

  while (offset < input_length) {
    offset += PromptNextGlyphLength(input + offset, input_length - offset);
    ++count;
  }

  return count;
}

size_t PromptPreviousGlyphStart(const char *input, size_t input_length) {
  size_t previous = 0;
  size_t offset = 0;

  assert(input != NULL || input_length == 0);

  while (offset < input_length) {
    previous = offset;
    offset += PromptNextGlyphLength(input + offset, input_length - offset);
  }

  return previous;
}
