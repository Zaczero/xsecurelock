#include "config.h"

#include "prompt_state.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../util.h"
#include "prompt_glyph.h"
#include "prompt_random.h"

void PromptStateInit(struct PromptState *state) {
  assert(state != NULL);

  memset(state, 0, sizeof(*state));
}

int PromptStateAppendByte(struct PromptState *state, char input_byte) {
  assert(state != NULL);

  if (state->password_length >= sizeof(state->password)) {
    return 0;
  }
  state->password[state->password_length] = input_byte;
  ++state->password_length;
  return 1;
}

void PromptStateDeleteLastGlyph(struct PromptState *state) {
  assert(state != NULL);

  size_t old_length = state->password_length;
  size_t previous_length =
      PromptPreviousGlyphStart(state->password, old_length);
  if (previous_length < old_length) {
    explicit_bzero(state->password + previous_length,
                   old_length - previous_length);
  }
  state->password_length = previous_length;
}

void PromptStateClear(struct PromptState *state) {
  assert(state != NULL);
  if (state->password_length != 0) {
    explicit_bzero(state->password, state->password_length);
  }
  state->password_length = 0;
}

void PromptStateBumpDisplayMarker(struct PromptState *state,
                                  struct PromptRng *rng, size_t marker_count,
                                  size_t min_change) {
  assert(state != NULL);
  assert(rng != NULL);

  gettimeofday(&state->last_keystroke, NULL);
  if (state->password_length == 0 || marker_count <= 1) {
    state->display_marker = 0;
    return;
  }

  state->display_marker = NextDisplayMarker(
      rng, state->display_marker, marker_count, min_change);
}

void PromptStateWipe(struct PromptState *state) {
  assert(state != NULL);
  explicit_bzero(state, sizeof(*state));
}
