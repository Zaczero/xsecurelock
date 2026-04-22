#ifndef XSECURELOCK_HELPERS_PROMPT_GLYPH_H
#define XSECURELOCK_HELPERS_PROMPT_GLYPH_H

#include <stddef.h>

size_t PromptGlyphCount(const char *input, size_t input_length);
size_t PromptPreviousGlyphStart(const char *input, size_t input_length);

#endif  // XSECURELOCK_HELPERS_PROMPT_GLYPH_H
