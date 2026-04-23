// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "auth_title.h"

#include <string.h>

#include "../buf_util.h"

static void TruncatingAppendBytes(char **output, size_t *output_size,
                                  const char *input, size_t input_size) {
  if (output == NULL || output_size == NULL || *output == NULL ||
      *output_size == 0) {
    return;
  }
  if (input_size >= *output_size) {
    input_size = *output_size - 1;
  }
  (void)AppendBytes(output, output_size, input, input_size);
}

void AuthBuildTitle(char *output, size_t output_size, const char *auth_title,
                    bool show_username, int show_hostname, const char *username,
                    const char *hostname, const char *input) {
  if (output_size == 0) {
    return;
  }
  output[0] = '\0';
  bool have_prefix = false;

  if (auth_title != NULL && *auth_title != '\0') {
    TruncatingAppendBytes(&output, &output_size, auth_title,
                          strlen(auth_title));
    have_prefix = true;
  } else {
    if (show_username) {
      TruncatingAppendBytes(&output, &output_size, username, strlen(username));
      have_prefix = true;
    }

    if (show_username && show_hostname) {
      TruncatingAppendBytes(&output, &output_size, "@", 1);
    }

    if (show_hostname) {
      size_t hostname_len =
          show_hostname > 1 ? strlen(hostname) : strcspn(hostname, ".");
      TruncatingAppendBytes(&output, &output_size, hostname, hostname_len);
      have_prefix = true;
    }
  }

  if (*input == '\0') {
    return;
  }

  if (have_prefix) {
    TruncatingAppendBytes(&output, &output_size, " - ", 3);
  }
  TruncatingAppendBytes(&output, &output_size, input, strlen(input));
}
