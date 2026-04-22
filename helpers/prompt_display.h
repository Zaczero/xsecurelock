#ifndef XSECURELOCK_HELPERS_PROMPT_DISPLAY_H
#define XSECURELOCK_HELPERS_PROMPT_DISPLAY_H

#include <stddef.h>

#include "prompt_state.h"

#define DISCO_PASSWORD_DANCERS 5
#define PROMPT_DISPLAY_BUFFER_SIZE (PROMPT_PASSWORD_BUFFER_SIZE + 2)

enum PromptDisplayMode {
  PROMPT_DISPLAY_MODE_CURSOR,
  PROMPT_DISPLAY_MODE_ASTERISKS,
  PROMPT_DISPLAY_MODE_HIDDEN,
  PROMPT_DISPLAY_MODE_DISCO,
  PROMPT_DISPLAY_MODE_EMOJI,
  PROMPT_DISPLAY_MODE_EMOTICON,
  PROMPT_DISPLAY_MODE_KAOMOJI,
  PROMPT_DISPLAY_MODE_TIME,
  PROMPT_DISPLAY_MODE_TIME_HEX,

  PROMPT_DISPLAY_MODE_COUNT,
};

int GetPromptDisplayModeFromFlags(int paranoid_password_flag,
                                  const char *prompt_display_mode_name,
                                  enum PromptDisplayMode *mode);

size_t PromptDisplayMarkerCount(enum PromptDisplayMode mode);
size_t PromptDisplayMinChange(enum PromptDisplayMode mode);

int FormatDiscoPrompt(size_t displaymarker, char *displaybuf,
                      size_t displaybufsize, size_t *displaylen);
int RenderPromptDisplay(enum PromptDisplayMode mode,
                        const struct PromptState *state, int echo,
                        int blink_state, char cursor_char, char *displaybuf,
                        size_t displaybufsize, size_t *displaylen);

#endif  // XSECURELOCK_HELPERS_PROMPT_DISPLAY_H
