#include <assert.h>
#include <string.h>

#include <X11/X.h>

#include "../helpers/xkb.h"

static void ExpectLayoutOnlyFormatting(void) {
  struct XkbIndicatorFormatInput input = {
      .layout_name = "English (US)",
      .show_keyboard_layout = 1,
  };
  struct XkbIndicators indicators = {0};

  assert(FormatXkbIndicatorText(&input, &indicators));
  assert(strcmp(indicators.text, "Keyboard: English (US)") == 0);
  assert(indicators.warning == 0);
  assert(indicators.have_multiple_layouts == 0);
}

static void ExpectIndicatorListFormatting(void) {
  const char *indicator_names[] = {"Num Lock", "Compose"};
  struct XkbIndicatorFormatInput input = {
      .indicator_names = indicator_names,
      .indicator_count = 2,
      .have_multiple_layouts = 1,
  };
  struct XkbIndicators indicators = {0};

  assert(FormatXkbIndicatorText(&input, &indicators));
  assert(strcmp(indicators.text, "Keyboard: Num Lock, Compose") == 0);
  assert(indicators.warning == 0);
  assert(indicators.have_multiple_layouts == 1);
}

static void ExpectLockAndLatchFormatting(void) {
  struct XkbIndicatorFormatInput input = {
      .implicit_mods = ShiftMask | LockMask | Mod4Mask,
      .show_locks_and_latches = 1,
  };
  struct XkbIndicators indicators = {0};

  assert(FormatXkbIndicatorText(&input, &indicators));
  assert(strcmp(indicators.text, "Keyboard: Shift, Lock, Mod4") == 0);
  assert(indicators.warning == 1);
  assert(indicators.have_multiple_layouts == 0);
}

static void ExpectCombinedFormatting(void) {
  const char *indicator_names[] = {"Num Lock"};
  struct XkbIndicatorFormatInput input = {
      .layout_name = "us",
      .indicator_names = indicator_names,
      .indicator_count = 1,
      .show_keyboard_layout = 1,
      .have_multiple_layouts = 1,
  };
  struct XkbIndicators indicators = {0};

  assert(FormatXkbIndicatorText(&input, &indicators));
  assert(strcmp(indicators.text, "Keyboard: us, Num Lock") == 0);
  assert(indicators.warning == 0);
  assert(indicators.have_multiple_layouts == 1);
}

static void ExpectEmptyFormattingProducesEmptyText(void) {
  struct XkbIndicatorFormatInput input = {0};
  struct XkbIndicators indicators = {0};

  assert(FormatXkbIndicatorText(&input, &indicators));
  assert(strcmp(indicators.text, "") == 0);
  assert(indicators.warning == 0);
  assert(indicators.have_multiple_layouts == 0);
}

static void ExpectOverflowKeepsPartialOutput(void) {
  const char *indicator_names[] = {
      "This indicator name is far too long to fit into the remaining buffer "
      "after the layout name has already been written",
  };
  struct XkbIndicatorFormatInput input = {
      .layout_name = "us",
      .indicator_names = indicator_names,
      .indicator_count = 1,
      .show_keyboard_layout = 1,
      .have_multiple_layouts = 1,
      .implicit_mods = LockMask,
  };
  struct XkbIndicators indicators = {0};

  assert(FormatXkbIndicatorText(&input, &indicators));
  assert(strcmp(indicators.text, "Keyboard: us") == 0);
  assert(indicators.warning == 1);
  assert(indicators.have_multiple_layouts == 1);
}

int main(void) {
  ExpectLayoutOnlyFormatting();
  ExpectIndicatorListFormatting();
  ExpectLockAndLatchFormatting();
  ExpectCombinedFormatting();
  ExpectEmptyFormattingProducesEmptyText();
  ExpectOverflowKeepsPartialOutput();
  return 0;
}
