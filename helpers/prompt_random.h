#ifndef PROMPT_RANDOM_H
#define PROMPT_RANDOM_H

#include <stddef.h>
#include <stdint.h>

struct PromptRng {
  uint32_t state;
};

void SeedPromptRng(struct PromptRng *rng, uint32_t seed);
int RandomRangeInclusive(struct PromptRng *rng, int min, int max);
int StepBurnInOffset(struct PromptRng *rng, int current, int max_offset,
                     int max_offset_change);
size_t NextDisplayMarker(struct PromptRng *rng, size_t current,
                         size_t marker_count, size_t min_change);

#endif
