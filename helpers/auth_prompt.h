#ifndef XSECURELOCK_HELPERS_AUTH_PROMPT_H
#define XSECURELOCK_HELPERS_AUTH_PROMPT_H

#include <stdbool.h>

#include "auth_ui.h"

enum PromptSessionResult {
  PROMPT_SESSION_RESULT_SUBMITTED,
  PROMPT_SESSION_RESULT_CANCELLED,
  PROMPT_SESSION_RESULT_FAILED,
};

enum StaticMessageResult {
  STATIC_MESSAGE_RESULT_ADVANCE,
  STATIC_MESSAGE_RESULT_CANCELLED,
  STATIC_MESSAGE_RESULT_FAILED,
};

enum StaticMessageResult AuthWaitStaticMessage(struct AuthUiContext *ctx,
                                               const char *title,
                                               const char *message,
                                               bool warning,
                                               int extra_read_fd);
enum PromptSessionResult AuthRunPromptSession(struct AuthUiContext *ctx,
                                              const char *message, bool echo,
                                              int response_fd,
                                              char response_type);

#endif  // XSECURELOCK_HELPERS_AUTH_PROMPT_H
