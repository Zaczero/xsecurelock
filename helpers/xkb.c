#include "config.h"

#include "xkb.h"

#include <stddef.h>
#include <string.h>

#include <X11/X.h>

#ifdef HAVE_XKB_EXT
#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>
#include <X11/extensions/XKBstr.h>
#endif

#include "../logging.h"
#include "indicator_text.h"

static void ClearXkbIndicators(struct XkbIndicators *result) {
  result->text[0] = '\0';
  result->warning = 0;
  result->have_multiple_layouts = 0;
}

static int AppendIndicatorName(char **output, size_t *output_size,
                               int *have_output, const char *name) {
  if (AppendIndicatorText(output, output_size, have_output, name)) {
    return 1;
  }
  if (name != NULL) {
    Log("Not enough space to store indicator text '%s'", name);
  }
  return 0;
}

static bool IsCapsLockIndicatorName(const char *name) {
  return name != NULL && strcmp(name, "Caps Lock") == 0;
}

static const struct {
  unsigned int mask;
  const char *name;
} kModifiers[] = {
    {ShiftMask, "Shift"}, {LockMask, "Lock"}, {ControlMask, "Control"},
    {Mod1Mask, "Mod1"},   {Mod2Mask, "Mod2"}, {Mod3Mask, "Mod3"},
    {Mod4Mask, "Mod4"},   {Mod5Mask, "Mod5"},
};

int FormatXkbIndicatorText(const struct XkbIndicatorFormatInput *input,
                           struct XkbIndicators *result) {
  if (input == NULL || result == NULL) {
    return 0;
  }

  ClearXkbIndicators(result);
  result->warning = !!(input->implicit_mods & LockMask);
  result->have_multiple_layouts = !!input->have_multiple_layouts;

  char *output = result->text;
  size_t output_size = sizeof(result->text);
  int have_output = 0;

  static const char prefix[] = "Keyboard: ";
  if (sizeof(prefix) > output_size) {
    Log("Not enough space to store intro '%s'", prefix);
    return 1;
  }
  memcpy(output, prefix, sizeof(prefix) - 1);
  output += sizeof(prefix) - 1;
  output_size -= sizeof(prefix) - 1;
  *output = '\0';

  if (input->show_keyboard_layout &&
      !AppendIndicatorName(&output, &output_size, &have_output,
                           input->layout_name)) {
    goto done;
  }

  if (input->show_locks_and_latches) {
    for (size_t i = 0; i < sizeof(kModifiers) / sizeof(*kModifiers); ++i) {
      if (!(input->implicit_mods & kModifiers[i].mask)) {
        continue;
      }
      if (!AppendIndicatorName(&output, &output_size, &have_output,
                               kModifiers[i].name)) {
        break;
      }
    }
  } else {
    if (input->caps_lock_active &&
        !AppendIndicatorName(&output, &output_size, &have_output,
                             "Caps Lock")) {
      goto done;
    }
    for (size_t i = 0; i < input->indicator_count; ++i) {
      if (IsCapsLockIndicatorName(input->indicator_names[i])) {
        continue;
      }
      if (!AppendIndicatorName(&output, &output_size, &have_output,
                               input->indicator_names[i])) {
        break;
      }
    }
  }

done:
  if (!have_output) {
    result->text[0] = '\0';
  }
  return 1;
}

#ifdef HAVE_XKB_EXT
static void FreeKeyboardDescription(XkbDescPtr *xkb) {
  if (*xkb != NULL) {
    XkbFreeKeyboard(*xkb, 0, True);
    *xkb = NULL;
  }
}

static XkbDescPtr GetKeyboardDescription(Display *display,
                                         unsigned int names_mask) {
  XkbDescPtr xkb = XkbGetMap(display, 0, XkbUseCoreKbd);
  if (xkb == NULL) {
    Log("XkbGetMap failed");
    return NULL;
  }
  if (XkbGetControls(display, XkbGroupsWrapMask, xkb) != Success) {
    Log("XkbGetControls failed");
    FreeKeyboardDescription(&xkb);
    return NULL;
  }
  if (xkb->ctrls == NULL) {
    Log("XkbGetControls returned no controls");
    FreeKeyboardDescription(&xkb);
    return NULL;
  }
  if (names_mask != 0) {
    if (XkbGetNames(display, names_mask, xkb) != Success) {
      Log("XkbGetNames failed");
      FreeKeyboardDescription(&xkb);
      return NULL;
    }
    if (xkb->names == NULL) {
      Log("XkbGetNames returned no names");
      FreeKeyboardDescription(&xkb);
      return NULL;
    }
  }
  return xkb;
}

static int GetKeyboardState(Display *display, XkbStateRec *state) {
  if (XkbGetState(display, XkbUseCoreKbd, state) != Success) {
    Log("XkbGetState failed");
    return 0;
  }
  return 1;
}

static char *GetAtomNameOrNull(Display *display, Atom atom) {
  if (atom == None) {
    return NULL;
  }
  char *name = XGetAtomName(display, atom);
  if (name == NULL) {
    Log("XGetAtomName failed for atom %#lx", (unsigned long)atom);
  }
  return name;
}

static void FreeAtomNames(char **names, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (names[i] != NULL) {
      XFree(names[i]);
      names[i] = NULL;
    }
  }
}
#endif

int HaveXkbExtension(Display *display) {
#ifdef HAVE_XKB_EXT
  int xkb_opcode = 0;
  int xkb_event_base = 0;
  int xkb_error_base = 0;
  int xkb_major_version = XkbMajorVersion;
  int xkb_minor_version = XkbMinorVersion;

  return XkbQueryExtension(display, &xkb_opcode, &xkb_event_base,
                           &xkb_error_base, &xkb_major_version,
                           &xkb_minor_version);
#else
  (void)display;
  return 0;
#endif
}

int GetXkbIndicators(Display *display, bool have_xkb_ext,
                     bool show_keyboard_layout, bool show_locks_and_latches,
                     struct XkbIndicators *result) {
  if (result == NULL) {
    return 0;
  }

  ClearXkbIndicators(result);

#ifdef HAVE_XKB_EXT
  if (!have_xkb_ext) {
    return 0;
  }

  XkbDescPtr xkb = GetKeyboardDescription(
      display, XkbIndicatorNamesMask | XkbGroupNamesMask | XkbSymbolsNameMask);
  if (xkb == NULL) {
    return 0;
  }

  XkbStateRec state;
  if (!GetKeyboardState(display, &state)) {
    FreeKeyboardDescription(&xkb);
    return 0;
  }

  unsigned int indicator_state = 0;
  if (!show_locks_and_latches &&
      XkbGetIndicatorState(display, XkbUseCoreKbd, &indicator_state) !=
          Success) {
    Log("XkbGetIndicatorState failed");
    FreeKeyboardDescription(&xkb);
    return 0;
  }

  char *layout_name = NULL;
  char *indicator_names[XkbNumIndicators] = {0};
  const char *indicator_name_views[XkbNumIndicators] = {0};
  size_t indicator_count = 0;

  if (show_keyboard_layout) {
    Atom layout_atom = None;
    if ((unsigned int)state.group < XkbNumKbdGroups) {
      layout_atom = xkb->names->groups[state.group];
    }
    if (layout_atom == None) {
      layout_atom = xkb->names->symbols;
    }
    layout_name = GetAtomNameOrNull(display, layout_atom);
  }

  if (!show_locks_and_latches) {
    for (int i = 0; i < XkbNumIndicators; ++i) {
      if (!(indicator_state & (1U << i))) {
        continue;
      }
      Atom indicator_atom = xkb->names->indicators[i];
      if (indicator_atom == None) {
        continue;
      }
      indicator_names[indicator_count] =
          GetAtomNameOrNull(display, indicator_atom);
      if (indicator_names[indicator_count] == NULL) {
        continue;
      }
      indicator_name_views[indicator_count] = indicator_names[indicator_count];
      ++indicator_count;
    }
  }

  struct XkbIndicatorFormatInput input = {
      .layout_name = layout_name,
      .implicit_mods = state.latched_mods | state.locked_mods,
      .caps_lock_active = (state.locked_mods & LockMask) != 0,
      .indicator_names = indicator_name_views,
      .indicator_count = indicator_count,
      .show_keyboard_layout = show_keyboard_layout,
      .show_locks_and_latches = show_locks_and_latches,
      .have_multiple_layouts = xkb->ctrls->num_groups > 1,
  };
  (void)FormatXkbIndicatorText(&input, result);

  if (layout_name != NULL) {
    XFree(layout_name);
  }
  FreeAtomNames(indicator_names, indicator_count);
  FreeKeyboardDescription(&xkb);
  return 1;
#else
  (void)display;
  (void)have_xkb_ext;
  (void)show_keyboard_layout;
  (void)show_locks_and_latches;
  return 0;
#endif
}

void SwitchToNextXkbLayout(Display *display, bool have_xkb_ext) {
#ifdef HAVE_XKB_EXT
  if (!have_xkb_ext) {
    return;
  }

  XkbDescPtr xkb = GetKeyboardDescription(display, 0);
  if (xkb == NULL) {
    return;
  }
  if (xkb->ctrls->num_groups < 1) {
    Log("XkbGetControls returned less than 1 group");
    FreeKeyboardDescription(&xkb);
    return;
  }

  XkbStateRec state;
  if (!GetKeyboardState(display, &state)) {
    FreeKeyboardDescription(&xkb);
    return;
  }

  XkbLockGroup(display, XkbUseCoreKbd,
               (state.group + 1) % xkb->ctrls->num_groups);
  FreeKeyboardDescription(&xkb);
#else
  (void)display;
  (void)have_xkb_ext;
#endif
}
