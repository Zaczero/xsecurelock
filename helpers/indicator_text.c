#include "config.h"

#include "indicator_text.h"

#include <string.h>

int AppendIndicatorText(char **output, size_t *output_size, int *have_output,
                        const char *name) {
  if (name == NULL) {
    return 0;
  }

  size_t separator_len = *have_output ? 2 : 0;
  size_t name_len = strlen(name);
  if (*output_size < separator_len + name_len + 1) {
    return 0;
  }

  if (*have_output) {
    memcpy(*output, ", ", 2);
    *output += 2;
    *output_size -= 2;
  }

  memcpy(*output, name, name_len);
  *output += name_len;
  *output_size -= name_len;
  **output = 0;
  *have_output = 1;
  return 1;
}
