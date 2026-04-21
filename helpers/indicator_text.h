#ifndef HELPERS_INDICATOR_TEXT_H
#define HELPERS_INDICATOR_TEXT_H

#include <stddef.h>

int AppendIndicatorText(char **output, size_t *output_size, int *have_output,
                        const char *name);

#endif
