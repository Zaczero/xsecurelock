#include <assert.h>
#include <stddef.h>

#include "../helpers/prompt_display.h"
#include "../helpers/prompt_random.h"

static size_t AbsDiff(size_t a, size_t b) {
  return (a < b) ? (b - a) : (a - b);
}

static void ExpectDisplayMarkersStayValid(void) {
  struct PromptRng rng;
  SeedPromptRng(&rng, 1);

  for (size_t current = 0; current < 32; ++current) {
    for (size_t i = 0; i < 128; ++i) {
      size_t next = NextDisplayMarker(&rng, current, 32, 4);
      assert(next >= 1);
      assert(next < 32);
      assert(AbsDiff(next, current) >= 4);
    }
  }
}

static void ExpectAnimatedModeMetadataProducesValidMarkers(void) {
  struct PromptRng rng;
  enum PromptDisplayMode modes[] = {
      PROMPT_DISPLAY_MODE_CURSOR,  PROMPT_DISPLAY_MODE_DISCO,
      PROMPT_DISPLAY_MODE_EMOJI,   PROMPT_DISPLAY_MODE_EMOTICON,
      PROMPT_DISPLAY_MODE_KAOMOJI,
  };

  SeedPromptRng(&rng, 11);
  for (size_t mode_index = 0; mode_index < sizeof(modes) / sizeof(modes[0]);
       ++mode_index) {
    size_t marker_count = PromptDisplayMarkerCount(modes[mode_index]);
    size_t min_change = PromptDisplayMinChange(modes[mode_index]);

    assert(marker_count == 32);
    assert(min_change == 4);
    for (size_t current = 0; current < marker_count; ++current) {
      size_t next = NextDisplayMarker(&rng, current, marker_count, min_change);
      assert(next >= 1);
      assert(next < marker_count);
      assert(AbsDiff(next, current) >= min_change);
    }
  }
}

static void ExpectBurnInOffsetsStayClamped(void) {
  struct PromptRng rng;
  int offset = 0;

  SeedPromptRng(&rng, 2);
  for (size_t i = 0; i < 1024; ++i) {
    offset = StepBurnInOffset(&rng, offset, 16, 3);
    assert(offset >= -16);
    assert(offset <= 16);
  }
}

static void ExpectRandomRangesStayBounded(void) {
  struct PromptRng rng;

  SeedPromptRng(&rng, 3);
  for (size_t i = 0; i < 1024; ++i) {
    int value = RandomRangeInclusive(&rng, -7, 9);
    assert(value >= -7);
    assert(value <= 9);
  }
}

static void ExpectZeroSeedStillWorks(void) {
  struct PromptRng rng;

  SeedPromptRng(&rng, 0);
  for (size_t i = 0; i < 32; ++i) {
    int value = RandomRangeInclusive(&rng, 0, 1);
    assert(value == 0 || value == 1);
  }
}

int main(void) {
  ExpectDisplayMarkersStayValid();
  ExpectAnimatedModeMetadataProducesValidMarkers();
  ExpectBurnInOffsetsStayClamped();
  ExpectRandomRangesStayBounded();
  ExpectZeroSeedStillWorks();
  return 0;
}
