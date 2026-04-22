#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "../helpers/prompt_display.h"

static struct PromptState MakePromptState(const char *password,
                                          size_t display_marker,
                                          time_t seconds,
                                          suseconds_t microseconds) {
  struct PromptState state;

  memset(&state, 0, sizeof(state));
  if (password != NULL) {
    state.password_length = strlen(password);
    memcpy(state.password, password, state.password_length);
  }
  state.display_marker = display_marker;
  state.last_keystroke.tv_sec = seconds;
  state.last_keystroke.tv_usec = microseconds;
  return state;
}

static void ExpectDiscoPrompt(size_t display_marker, const char *expected) {
  char displaybuf[256];
  size_t displaylen = 0;

  assert(FormatDiscoPrompt(display_marker, displaybuf, sizeof(displaybuf),
                           &displaylen) == 0);
  assert(strcmp(displaybuf, expected) == 0);
  assert(displaylen == strlen(expected));
}

static void ExpectDiscoPromptFailure(size_t display_marker, size_t bufsize) {
  char displaybuf[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', '\0'};
  size_t displaylen = 123;

  assert(bufsize <= sizeof(displaybuf));
  assert(FormatDiscoPrompt(display_marker, displaybuf, bufsize, &displaylen) !=
         0);
  assert(displaylen == 0);
  assert(displaybuf[0] == '\0');
}

static void ExpectDisplayModeParsing(void) {
  enum PromptDisplayMode mode = PROMPT_DISPLAY_MODE_COUNT;

  assert(GetPromptDisplayModeFromFlags(1, "", &mode) == 1);
  assert(mode == PROMPT_DISPLAY_MODE_CURSOR);

  assert(GetPromptDisplayModeFromFlags(0, "", &mode) == 1);
  assert(mode == PROMPT_DISPLAY_MODE_ASTERISKS);

  assert(GetPromptDisplayModeFromFlags(1, "time_hex", &mode) == 1);
  assert(mode == PROMPT_DISPLAY_MODE_TIME_HEX);

  assert(GetPromptDisplayModeFromFlags(1, "not-a-mode", &mode) == 0);
  assert(mode == PROMPT_DISPLAY_MODE_CURSOR);
}

static void ExpectMarkerMetadata(void) {
  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_CURSOR) == 32);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_CURSOR) == 4);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_DISCO) == 32);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_DISCO) == 4);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_EMOJI) == 32);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_EMOJI) == 4);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_EMOTICON) == 32);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_EMOTICON) == 4);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_KAOMOJI) == 32);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_KAOMOJI) == 4);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_ASTERISKS) == 0);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_ASTERISKS) == 0);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_HIDDEN) == 0);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_HIDDEN) == 0);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_TIME) == 0);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_TIME) == 0);

  assert(PromptDisplayMarkerCount(PROMPT_DISPLAY_MODE_TIME_HEX) == 0);
  assert(PromptDisplayMinChange(PROMPT_DISPLAY_MODE_TIME_HEX) == 0);
}

static void ExpectCursorAndEchoRendering(void) {
  char displaybuf[PROMPT_DISPLAY_BUFFER_SIZE];
  size_t displaylen = 0;
  struct PromptState state = MakePromptState("abc", 5, 0, 0);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_CURSOR, &state, 0, 0, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(displaylen == (size_t)(1u << DISCO_PASSWORD_DANCERS));
  assert(displaybuf[5] == '|');

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_CURSOR, &state, 0, 1, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(displaybuf[5] == '-');

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_CURSOR, &state, 1, 0, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "abc|") == 0);
  assert(displaylen == 4);
}

static void ExpectMaskedAndHiddenRendering(void) {
  char displaybuf[PROMPT_DISPLAY_BUFFER_SIZE];
  size_t displaylen = 0;
  struct PromptState state = MakePromptState("hunter2", 0, 0, 0);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_ASTERISKS, &state, 0, 0, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "*******|") == 0);
  assert(displaylen == 8);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_HIDDEN, &state, 0, 0, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "") == 0);
  assert(displaylen == 0);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_ASTERISKS, &state, 0, 0, '|',
                             displaybuf, sizeof(displaybuf), NULL) != 0);
  assert(displaybuf[0] == '\0');
}

static void ExpectEmojiAndTruncationRendering(void) {
  char displaybuf[PROMPT_DISPLAY_BUFFER_SIZE];
  char tinybuf[4];
  size_t displaylen = 0;
  struct PromptState emoji_state = MakePromptState(NULL, 0, 0, 0);
  struct PromptState emoticon_state = MakePromptState(NULL, 24, 0, 0);
  struct PromptState kaomoji_state = MakePromptState(NULL, 31, 0, 0);
  struct PromptState invalid_state = MakePromptState(NULL, 32, 0, 0);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_EMOJI, &emoji_state, 0, 0,
                             '|', displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "_____") == 0);
  assert(displaylen == 5);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_EMOTICON, &emoticon_state, 0,
                             0, '|', tinybuf, sizeof(tinybuf),
                             &displaylen) == 0);
  assert(strcmp(tinybuf, ":'-") == 0);
  assert(displaylen == 3);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_KAOMOJI, &kaomoji_state, 0, 0,
                             '|', displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "❤(◍•ᴗ•◍)") == 0);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_EMOJI, &invalid_state, 0, 0,
                             '|', displaybuf, sizeof(displaybuf),
                             &displaylen) != 0);
  assert(displaybuf[0] == '\0');
  assert(displaylen == 0);
}

static void ExpectTimeRendering(void) {
  char displaybuf[PROMPT_DISPLAY_BUFFER_SIZE];
  size_t displaylen = 0;
  struct PromptState empty_state = MakePromptState(NULL, 0, 0, 0);
  struct PromptState state = MakePromptState("x", 0, 123, 456789);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_TIME, &empty_state, 0, 0, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "----") == 0);
  assert(displaylen == 4);

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_TIME, &state, 0, 0, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "123.456789") == 0);
  assert(displaylen == strlen("123.456789"));

  assert(RenderPromptDisplay(PROMPT_DISPLAY_MODE_TIME_HEX, &state, 0, 0, '|',
                             displaybuf, sizeof(displaybuf),
                             &displaylen) == 0);
  assert(strcmp(displaybuf, "0x75bcd15") == 0);
  assert(displaylen == strlen("0x75bcd15"));
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

  ExpectDisplayModeParsing();
  ExpectMarkerMetadata();
  ExpectCursorAndEchoRendering();
  ExpectMaskedAndHiddenRendering();
  ExpectEmojiAndTruncationRendering();
  ExpectTimeRendering();
  return 0;
}
