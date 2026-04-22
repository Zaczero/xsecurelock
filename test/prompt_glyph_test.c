#include <assert.h>
#include <string.h>

#include "../helpers/prompt_glyph.h"

static void ExpectGlyphCount(const char *input, size_t input_length,
                             size_t expected) {
  assert(PromptGlyphCount(input, input_length) == expected);
}

static void ExpectPreviousGlyphStart(const char *input, size_t input_length,
                                     size_t expected) {
  assert(PromptPreviousGlyphStart(input, input_length) == expected);
}

static void ExpectAsciiGlyphs(void) {
  const char input[] = "abc";

  ExpectGlyphCount(input, strlen(input), 3);
  ExpectPreviousGlyphStart(input, strlen(input), 2);
}

static void ExpectValidUtf8Glyphs(void) {
  const char input[] = "a\xE2\x82\xAC\xF0\x9F\x98\x80";

  ExpectGlyphCount(input, sizeof(input) - 1, 3);
  ExpectPreviousGlyphStart(input, sizeof(input) - 1, 4);
}

static void ExpectMalformedLeadByteCountsAsGlyph(void) {
  const char input[] = "a\xFF";

  ExpectGlyphCount(input, sizeof(input) - 1, 2);
  ExpectPreviousGlyphStart(input, sizeof(input) - 1, 1);
}

static void ExpectStrayContinuationSuffixCollapsesIntoPreviousGlyph(void) {
  const char input[] = "a\x80";

  ExpectGlyphCount(input, sizeof(input) - 1, 1);
  ExpectPreviousGlyphStart(input, sizeof(input) - 1, 0);
}

static void ExpectTruncatedMultibyteSuffixUsesLeadBoundary(void) {
  const char input[] = "a\xE2\x82";

  ExpectGlyphCount(input, sizeof(input) - 1, 2);
  ExpectPreviousGlyphStart(input, sizeof(input) - 1, 1);
}

static void ExpectContinuationOnlyInputCountsAsZeroGlyphs(void) {
  const char input[] = "\x80\x80\x80";

  ExpectGlyphCount(input, sizeof(input) - 1, 0);
  ExpectPreviousGlyphStart(input, sizeof(input) - 1, 0);
}

int main(void) {
  ExpectAsciiGlyphs();
  ExpectValidUtf8Glyphs();
  ExpectMalformedLeadByteCountsAsGlyph();
  ExpectStrayContinuationSuffixCollapsesIntoPreviousGlyph();
  ExpectTruncatedMultibyteSuffixUsesLeadBoundary();
  ExpectContinuationOnlyInputCountsAsZeroGlyphs();
  return 0;
}
