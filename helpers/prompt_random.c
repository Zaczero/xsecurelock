#include "config.h"

#include "prompt_random.h"

#include <assert.h>
#include <limits.h>

enum {
  kPromptRandomRetryLimit = 16,
};

static uint32_t NextPromptRandom(struct PromptRng *rng) {
  assert(rng != NULL);
  assert(rng->state != 0);

  uint32_t x = rng->state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng->state = x;
  return x;
}

static uint32_t PromptRandomBelow(struct PromptRng *rng, uint32_t upper_bound) {
  assert(upper_bound != 0);

  uint32_t minimum = (uint32_t)(-upper_bound) % upper_bound;
  for (int attempt = 0; attempt < kPromptRandomRetryLimit; ++attempt) {
    uint32_t value = NextPromptRandom(rng);
    if (value >= minimum) {
      return value % upper_bound;
    }
  }

  // Prompt RNG only drives UI jitter, so a bounded modulo fallback is fine.
  return NextPromptRandom(rng) % upper_bound;
}

static size_t AbsDiff(size_t a, size_t b) {
  return (a < b) ? (b - a) : (a - b);
}

static int ClampInt(int value, int min, int max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

void SeedPromptRng(struct PromptRng *rng, uint32_t seed) {
  assert(rng != NULL);

  rng->state = (seed != 0) ? seed : 0x6d2b79f5U;
}

int RandomRangeInclusive(struct PromptRng *rng, int min, int max) {
  assert(rng != NULL);
  assert(min <= max);

  int64_t span = (int64_t)max - (int64_t)min + 1;
  assert(span > 0);
  assert(span <= UINT32_MAX);

  return min + (int)PromptRandomBelow(rng, (uint32_t)span);
}

int StepBurnInOffset(struct PromptRng *rng, int current, int max_offset,
                     int max_offset_change) {
  assert(rng != NULL);

  if (max_offset <= 0) {
    return 0;
  }
  if (max_offset_change <= 0) {
    return ClampInt(current, -max_offset, max_offset);
  }

  int delta = RandomRangeInclusive(rng, -max_offset_change,
                                   max_offset_change);
  return ClampInt(current + delta, -max_offset, max_offset);
}

size_t NextDisplayMarker(struct PromptRng *rng, size_t current,
                         size_t marker_count, size_t min_change) {
  assert(rng != NULL);
  assert(marker_count > 1);

  size_t candidate_count = 0;
  for (size_t pos = 1; pos < marker_count; ++pos) {
    if (AbsDiff(pos, current) >= min_change) {
      ++candidate_count;
    }
  }
  assert(candidate_count > 0);

  size_t selected = PromptRandomBelow(rng, (uint32_t)candidate_count);
  for (size_t pos = 1; pos < marker_count; ++pos) {
    if (AbsDiff(pos, current) < min_change) {
      continue;
    }
    if (selected == 0) {
      return pos;
    }
    --selected;
  }

  assert(!"display marker choice must be reachable");
  return 0;
}
