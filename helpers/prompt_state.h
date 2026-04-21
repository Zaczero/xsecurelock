#ifndef XSECURELOCK_HELPERS_PROMPT_STATE_H
#define XSECURELOCK_HELPERS_PROMPT_STATE_H

#include <stddef.h>
#include <sys/time.h>

struct PromptRng;

#define PROMPT_PASSWORD_BUFFER_SIZE 256

struct PromptState {
  char password[PROMPT_PASSWORD_BUFFER_SIZE];
  size_t password_length;
  size_t display_marker;
  struct timeval last_keystroke;
};

void PromptStateInit(struct PromptState *state);
int PromptStateAppendByte(struct PromptState *state, char input_byte);
void PromptStateDeleteLastGlyph(struct PromptState *state);
void PromptStateClear(struct PromptState *state);
void PromptStateBumpDisplayMarker(struct PromptState *state,
                                  struct PromptRng *rng, size_t marker_count,
                                  size_t min_change);
void PromptStateWipe(struct PromptState *state);

#endif  // XSECURELOCK_HELPERS_PROMPT_STATE_H
