#include <assert.h>
#include <string.h>

#include "../helpers/prompt_display.h"
#include "../helpers/prompt_random.h"
#include "../helpers/prompt_state.h"

static size_t AbsDiff(size_t a, size_t b) {
  return (a < b) ? (b - a) : (a - b);
}

static void ExpectInitStartsEmpty(void) {
  struct PromptState state;

  PromptStateInit(&state);
  assert(state.password_length == 0);
  assert(state.display_marker == 0);
}

static void ExpectAppendAndClear(void) {
  struct PromptState state;

  PromptStateInit(&state);
  assert(PromptStateAppendByte(&state, 'a'));
  assert(PromptStateAppendByte(&state, 'b'));
  assert(PromptStateAppendByte(&state, 'c'));
  assert(state.password_length == 3);
  assert(memcmp(state.password, "abc", 3) == 0);

  PromptStateClear(&state);
  assert(state.password_length == 0);
  assert(state.password[0] == '\0');
  assert(state.password[1] == '\0');
  assert(state.password[2] == '\0');
}

static void ExpectDeleteLastGlyph(void) {
  struct PromptState state;

  PromptStateInit(&state);
  memcpy(state.password, "a\xC3\xA9", 3);
  state.password_length = 3;

  PromptStateDeleteLastGlyph(&state);
  assert(state.password_length == 1);
  assert(state.password[0] == 'a');
  assert(state.password[1] == '\0');
  assert(state.password[2] == '\0');

  PromptStateDeleteLastGlyph(&state);
  assert(state.password_length == 0);
  assert(state.password[0] == '\0');
}

static void ExpectDeleteMalformedByteStillProgresses(void) {
  struct PromptState state;

  PromptStateInit(&state);
  state.password[0] = '\xFF';
  state.password_length = 1;

  PromptStateDeleteLastGlyph(&state);
  assert(state.password_length == 0);
  assert(state.password[0] == '\0');
}

static void ExpectDeleteTruncatedUtf8TailStillProgresses(void) {
  struct PromptState state;

  PromptStateInit(&state);
  memcpy(state.password, "a\xE2\x82", 3);
  state.password_length = 3;

  PromptStateDeleteLastGlyph(&state);
  assert(state.password_length == 1);
  assert(state.password[0] == 'a');
  assert(state.password[1] == '\0');
  assert(state.password[2] == '\0');

  PromptStateDeleteLastGlyph(&state);
  assert(state.password_length == 0);
  assert(state.password[0] == '\0');
}

static void ExpectDeleteStrayContinuationByteStillProgresses(void) {
  struct PromptState state;

  PromptStateInit(&state);
  state.password[0] = 'a';
  state.password[1] = '\x80';
  state.password_length = 2;

  PromptStateDeleteLastGlyph(&state);
  assert(state.password_length == 0);
  assert(state.password[0] == '\0');
  assert(state.password[1] == '\0');
}

static void ExpectDeleteContinuationOnlyBufferStillProgresses(void) {
  struct PromptState state;

  PromptStateInit(&state);
  memcpy(state.password, "\x80\x80\x80", 3);
  state.password_length = 3;

  PromptStateDeleteLastGlyph(&state);
  assert(state.password_length == 0);
  assert(state.password[0] == '\0');
  assert(state.password[1] == '\0');
  assert(state.password[2] == '\0');
}

static void ExpectBufferFullRejectsAppend(void) {
  struct PromptState state;

  PromptStateInit(&state);
  memset(state.password, 'x', sizeof(state.password));
  state.password_length = sizeof(state.password);

  assert(!PromptStateAppendByte(&state, 'y'));
  assert(state.password_length == sizeof(state.password));
}

static void ExpectDisplayMarkerUpdatesStayValid(void) {
  struct PromptState state;
  struct PromptRng rng;

  PromptStateInit(&state);
  SeedPromptRng(&rng, 1);

  PromptStateAppendByte(&state, 'a');
  PromptStateBumpDisplayMarker(&state, &rng, 32, 4);
  assert(state.display_marker >= 1);
  assert(state.display_marker < 32);

  size_t previous = state.display_marker;
  PromptStateAppendByte(&state, 'b');
  PromptStateBumpDisplayMarker(&state, &rng, 32, 4);
  assert(state.display_marker >= 1);
  assert(state.display_marker < 32);
  assert(AbsDiff(state.display_marker, previous) >= 4);

  PromptStateClear(&state);
  PromptStateBumpDisplayMarker(&state, &rng, 32, 4);
  assert(state.display_marker == 0);
}

static void ExpectNonMarkerModesKeepMarkerAtZero(void) {
  struct PromptState state;
  struct PromptRng rng;

  PromptStateInit(&state);
  SeedPromptRng(&rng, 7);
  assert(PromptStateAppendByte(&state, 'x'));

  PromptStateBumpDisplayMarker(
      &state, &rng, PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_TIME),
      PromptDisplayMinChange(PROMPT_DISPLAY_MODE_TIME));
  assert(state.display_marker == 0);

  PromptStateBumpDisplayMarker(
      &state, &rng, PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_ASTERISKS),
      PromptDisplayMinChange(PROMPT_DISPLAY_MODE_ASTERISKS));
  assert(state.display_marker == 0);
}

int main(void) {
  ExpectInitStartsEmpty();
  ExpectAppendAndClear();
  ExpectDeleteLastGlyph();
  ExpectDeleteMalformedByteStillProgresses();
  ExpectDeleteTruncatedUtf8TailStillProgresses();
  ExpectDeleteStrayContinuationByteStillProgresses();
  ExpectDeleteContinuationOnlyBufferStillProgresses();
  ExpectBufferFullRejectsAppend();
  ExpectDisplayMarkerUpdatesStayValid();
  ExpectNonMarkerModesKeepMarkerAtZero();
  return 0;
}
