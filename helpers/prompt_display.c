#include "config.h"

#include "prompt_display.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const kDiscoCombiner = " ♪ ";
static const char *const kDiscoDancers[] = {
    "┏(･o･)┛",
    "┗(･o･)┓",
};
static const char *const kPromptDisplayModeNames[] = {
    /* PROMPT_DISPLAY_MODE_CURSOR= */ "cursor",
    /* PROMPT_DISPLAY_MODE_ASTERISKS= */ "asterisks",
    /* PROMPT_DISPLAY_MODE_HIDDEN= */ "hidden",
    /* PROMPT_DISPLAY_MODE_DISCO= */ "disco",
    /* PROMPT_DISPLAY_MODE_EMOJI= */ "emoji",
    /* PROMPT_DISPLAY_MODE_EMOTICON= */ "emoticon",
    /* PROMPT_DISPLAY_MODE_KAOMOJI= */ "kaomoji",
    /* PROMPT_DISPLAY_MODE_TIME= */ "time",
    /* PROMPT_DISPLAY_MODE_TIME_HEX= */ "time_hex",
};
static const char *const kEmoji[] = {
    "_____", "😂", "❤", "♻", "😍", "♥", "😭", "😊", "😒", "💕", "😘",
    "😩",     "☺", "👌", "😔", "😁", "😏", "😉", "👍", "⬅", "😅", "🙏",
    "😌",     "😢", "👀", "💔", "😎", "🎶", "💙", "💜", "🙌", "😳",
};
static const char *const kEmoticons[] = {
    ":-)",  ":-p", ":-O", ":-\\", "(-:",  "d-:", "O-:", "/-:",
    "8-)",  "8-p", "8-O", "8-\\", "(-8",  "d-8", "O-8", "/-8",
    "X-)",  "X-p", "X-O", "X-\\", "(-X",  "d-X", "O-X", "/-X",
    ":'-)", ":-S", ":-D", ":-#",  "(-':", "S-:", "D-:", "#-:",
};
static const char *const kKaomoji[] = {
    "(͡°͜ʖ͡°)",     "(>_<)",       "O_ם",      "(^_-)",        "o_0",
    "o.O",       "0_o",         "O.o",      "(°o°)",        "^m^",
    "^_^",       "((d[-_-]b))", "┏(･o･)┛",  "┗(･o･)┓",      "（ﾟДﾟ)",
    "(°◇°)",     "\\o/",        "\\o|",     "|o/",          "|o|",
    "(●＾o＾●)", "(＾ｖ＾)",    "(＾ｕ＾)", "(＾◇＾)",      "¯\\_(ツ)_/¯",
    "(^0_0^)",   "(☞ﾟ∀ﾟ)☞",     "(-■_■)",   "(┛ಠ_ಠ)┛彡┻━┻", "┬─┬ノ(º_ºノ)",
    "(˘³˘)♥",    "❤(◍•ᴗ•◍)",
};

static int WriteDisplayString(char *displaybuf, size_t displaybufsize,
                              const char *input, size_t input_length,
                              size_t *displaylen) {
  size_t copy_length;

  if (displaybuf == NULL || displaylen == NULL || displaybufsize == 0) {
    return -1;
  }

  copy_length = input_length;
  if (copy_length >= displaybufsize) {
    copy_length = displaybufsize - 1;
  }

  if (copy_length != 0) {
    memcpy(displaybuf, input, copy_length);
  }
  displaybuf[copy_length] = '\0';
  *displaylen = copy_length;
  return 0;
}

static int RenderCursorPromptDisplay(const struct PromptState *state,
                                     int blink_state, char cursor_char,
                                     char *displaybuf, size_t displaybufsize,
                                     size_t *displaylen) {
  if (displaybuf == NULL || displaylen == NULL || displaybufsize == 0) {
    return -1;
  }

  *displaylen = 1u << DISCO_PASSWORD_DANCERS;
  if (*displaylen + 1 > displaybufsize ||
      state->display_marker >= *displaylen) {
    goto fail;
  }
  memset(displaybuf, '_', *displaylen);
  displaybuf[state->display_marker] = blink_state ? '-' : cursor_char;
  displaybuf[*displaylen] = '\0';
  return 0;

fail:
  displaybuf[0] = '\0';
  *displaylen = 0;
  return -1;
}

static size_t GetMaskedPasswordLength(const struct PromptState *state) {
  size_t display_length = 0;
  size_t offset = 0;

  mblen(NULL, 0);
  while (offset < state->password_length) {
    int glyph_length;

    ++display_length;
    glyph_length =
        mblen(state->password + offset, state->password_length - offset);
    if (glyph_length <= 0) {
      break;
    }
    offset += (size_t)glyph_length;
  }

  return display_length;
}

static int RenderAsterisksPromptDisplay(const struct PromptState *state,
                                        int blink_state, char cursor_char,
                                        char *displaybuf,
                                        size_t displaybufsize,
                                        size_t *displaylen) {
  *displaylen = GetMaskedPasswordLength(state);
  if (displaybuf == NULL || displaylen == NULL || displaybufsize == 0 ||
      *displaylen + 2 > displaybufsize) {
    goto fail;
  }

  memset(displaybuf, '*', *displaylen);
  displaybuf[*displaylen] = blink_state ? ' ' : cursor_char;
  displaybuf[*displaylen + 1] = '\0';
  ++*displaylen;
  return 0;

fail:
  if (displaybuf != NULL && displaybufsize != 0) {
    displaybuf[0] = '\0';
  }
  if (displaylen != NULL) {
    *displaylen = 0;
  }
  return -1;
}

static int RenderEchoPromptDisplay(const struct PromptState *state,
                                   int blink_state, char cursor_char,
                                   char *displaybuf, size_t displaybufsize,
                                   size_t *displaylen) {
  if (displaybuf == NULL || displaylen == NULL || displaybufsize < 2 ||
      state->password_length + 2 > displaybufsize) {
    goto fail;
  }

  if (state->password_length != 0) {
    memcpy(displaybuf, state->password, state->password_length);
  }
  displaybuf[state->password_length] = blink_state ? ' ' : cursor_char;
  displaybuf[state->password_length + 1] = '\0';
  *displaylen = state->password_length + 1;
  return 0;

fail:
  if (displaybuf != NULL && displaybufsize != 0) {
    displaybuf[0] = '\0';
  }
  if (displaylen != NULL) {
    *displaylen = 0;
  }
  return -1;
}

static int RenderArrayPromptDisplay(const char *const *choices,
                                    const struct PromptState *state,
                                    char *displaybuf, size_t displaybufsize,
                                    size_t *displaylen) {
  return WriteDisplayString(displaybuf, displaybufsize,
                            choices[state->display_marker],
                            strlen(choices[state->display_marker]), displaylen);
}

static int RenderTimePromptDisplay(enum PromptDisplayMode mode,
                                   const struct PromptState *state,
                                   char *displaybuf, size_t displaybufsize,
                                   size_t *displaylen) {
  int written;

  if (displaybuf == NULL || displaylen == NULL || displaybufsize == 0) {
    return -1;
  }
  if (state->password_length == 0) {
    return WriteDisplayString(displaybuf, displaybufsize, "----", 4,
                              displaylen);
  }

  if (mode == PROMPT_DISPLAY_MODE_TIME) {
    written = snprintf(displaybuf, displaybufsize, "%" PRId64 ".%06" PRId64,
                       (int64_t)state->last_keystroke.tv_sec,
                       (int64_t)state->last_keystroke.tv_usec);
  } else {
    written = snprintf(displaybuf, displaybufsize, "%#" PRIx64,
                       (int64_t)state->last_keystroke.tv_sec * 1000000 +
                           (int64_t)state->last_keystroke.tv_usec);
  }

  if (written < 0) {
    displaybuf[0] = '\0';
    *displaylen = 0;
    return -1;
  }
  displaybuf[displaybufsize - 1] = '\0';
  *displaylen = strlen(displaybuf);
  return 0;
}

int GetPromptDisplayModeFromFlags(int paranoid_password_flag,
                                  const char *prompt_display_mode_name,
                                  enum PromptDisplayMode *mode) {
  if (mode == NULL) {
    return 0;
  }
  if (prompt_display_mode_name == NULL || *prompt_display_mode_name == '\0') {
    *mode = paranoid_password_flag ? PROMPT_DISPLAY_MODE_CURSOR
                                   : PROMPT_DISPLAY_MODE_ASTERISKS;
    return 1;
  }

  for (enum PromptDisplayMode candidate = 0;
       candidate < PROMPT_DISPLAY_MODE_COUNT; ++candidate) {
    if (strcmp(prompt_display_mode_name, kPromptDisplayModeNames[candidate]) ==
        0) {
      *mode = candidate;
      return 1;
    }
  }

  *mode = PROMPT_DISPLAY_MODE_CURSOR;
  return 0;
}

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

int RenderPromptDisplay(enum PromptDisplayMode mode,
                        const struct PromptState *state, int echo,
                        int blink_state, char cursor_char, char *displaybuf,
                        size_t displaybufsize, size_t *displaylen) {
  if (state == NULL) {
    if (displaybuf != NULL && displaybufsize != 0) {
      displaybuf[0] = '\0';
    }
    if (displaylen != NULL) {
      *displaylen = 0;
    }
    return -1;
  }

  if (echo) {
    return RenderEchoPromptDisplay(state, blink_state, cursor_char, displaybuf,
                                   displaybufsize, displaylen);
  }

  switch (mode) {
    case PROMPT_DISPLAY_MODE_ASTERISKS:
      return RenderAsterisksPromptDisplay(state, blink_state, cursor_char,
                                          displaybuf, displaybufsize,
                                          displaylen);
    case PROMPT_DISPLAY_MODE_HIDDEN:
      return WriteDisplayString(displaybuf, displaybufsize, "", 0, displaylen);
    case PROMPT_DISPLAY_MODE_DISCO:
      return FormatDiscoPrompt(state->display_marker, displaybuf,
                               displaybufsize, displaylen);
    case PROMPT_DISPLAY_MODE_EMOJI:
      return RenderArrayPromptDisplay(kEmoji, state, displaybuf,
                                      displaybufsize, displaylen);
    case PROMPT_DISPLAY_MODE_EMOTICON:
      return RenderArrayPromptDisplay(kEmoticons, state, displaybuf,
                                      displaybufsize, displaylen);
    case PROMPT_DISPLAY_MODE_KAOMOJI:
      return RenderArrayPromptDisplay(kKaomoji, state, displaybuf,
                                      displaybufsize, displaylen);
    case PROMPT_DISPLAY_MODE_TIME:
    case PROMPT_DISPLAY_MODE_TIME_HEX:
      return RenderTimePromptDisplay(mode, state, displaybuf, displaybufsize,
                                     displaylen);
    case PROMPT_DISPLAY_MODE_CURSOR:
    default:
      return RenderCursorPromptDisplay(state, blink_state, cursor_char,
                                       displaybuf, displaybufsize, displaylen);
  }
}
