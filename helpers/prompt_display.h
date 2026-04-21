#ifndef XSECURELOCK_HELPERS_PROMPT_DISPLAY_H
#define XSECURELOCK_HELPERS_PROMPT_DISPLAY_H

#include <stddef.h>

#define DISCO_PASSWORD_DANCERS 5

int FormatDiscoPrompt(size_t displaymarker, char *displaybuf,
                      size_t displaybufsize, size_t *displaylen);

#endif  // XSECURELOCK_HELPERS_PROMPT_DISPLAY_H
