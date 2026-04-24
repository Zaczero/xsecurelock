#include "config.h"

#include "auth_ui_config.h"

#include <string.h>

#include "../env_info.h"
#include "../env_settings.h"
#include "../keysym_config.h"
#include "../logging.h"
#include "build-config.h"

enum { AUTH_UI_MAX_INSET_SETTING = 4096 };

void AuthUiContextInit(struct AuthUiContext *ctx, int argc, char **argv) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->config.argc = argc;
  ctx->config.argv = argv;
  ctx->config.prompt_display_mode = PROMPT_DISPLAY_MODE_CURSOR;
  ctx->config.datetime_format = "%c";
  ctx->config.background_color = "black";
  ctx->config.foreground_color = "white";
  ctx->config.warning_color = "red";
  ctx->config.font_name = "";
  ctx->config.auth_title = "";
  ctx->config.auth_cursor_blink = true;
  ctx->config.auth_padding = 16;
  ctx->config.auth_x_position = 50;
  ctx->config.auth_y_position = 50;
  ctx->config.show_keyboard_layout = true;
  ctx->config.layout_switch_key_name = "Tab";
  ctx->windows.dirty = true;
}

int AuthUiConfigLoad(struct AuthUiConfig *config) {
  int paranoid_password_flag = GetBoolSetting(
      "XSECURELOCK_" /* REMOVE IN v2 */ "PARANOID_PASSWORD", 1);
  const char *password_prompt_flag =
      GetStringSetting("XSECURELOCK_PASSWORD_PROMPT", "");

  config->authproto_executable = GetExecutablePathSetting(
      "XSECURELOCK_AUTHPROTO", AUTHPROTO_EXECUTABLE, 0);
  config->burnin_mitigation_max_offset =
      GetNonnegativeIntSetting("XSECURELOCK_BURNIN_MITIGATION", 16);
  config->burnin_mitigation_max_offset_change =
      GetNonnegativeIntSetting("XSECURELOCK_BURNIN_MITIGATION_DYNAMIC", 0);
  config->prompt_timeout =
      GetNonnegativeIntSetting("XSECURELOCK_AUTH_TIMEOUT", 5 * 60);
  config->auth_title = GetStringSetting("XSECURELOCK_AUTH_TITLE", "");
  config->show_username = GetBoolSetting("XSECURELOCK_SHOW_USERNAME", 1);
  config->show_hostname =
      GetClampedIntSetting("XSECURELOCK_SHOW_HOSTNAME", 1, 0, 2);
  config->show_datetime = GetBoolSetting("XSECURELOCK_SHOW_DATETIME", 0);
  config->datetime_format =
      GetStringSetting("XSECURELOCK_DATETIME_FORMAT", "%c");
  config->background_color =
      GetStringSetting("XSECURELOCK_AUTH_BACKGROUND_COLOR", "black");
  config->foreground_color =
      GetStringSetting("XSECURELOCK_AUTH_FOREGROUND_COLOR", "white");
  config->warning_color =
      GetStringSetting("XSECURELOCK_AUTH_WARNING_COLOR", "red");
  config->font_name = GetStringSetting("XSECURELOCK_FONT", "");
  config->have_switch_user_command =
      *GetStringSetting("XSECURELOCK_SWITCH_USER_COMMAND", "") != '\0';
  config->auth_sounds = GetBoolSetting("XSECURELOCK_AUTH_SOUNDS", 0);
  config->single_auth_window =
      GetBoolSetting("XSECURELOCK_SINGLE_AUTH_WINDOW", 0);
  config->auth_cursor_blink =
      GetBoolSetting("XSECURELOCK_AUTH_CURSOR_BLINK", 1);
  config->auth_padding = GetClampedIntSetting("XSECURELOCK_AUTH_PADDING", 16, 0,
                                              AUTH_UI_MAX_INSET_SETTING);
  config->auth_border_size = GetClampedIntSetting(
      "XSECURELOCK_AUTH_BORDER_SIZE", 0, 0, AUTH_UI_MAX_INSET_SETTING);
  config->auth_x_position =
      GetClampedIntSetting("XSECURELOCK_AUTH_X_POSITION", 50, 0, 100);
  config->auth_y_position =
      GetClampedIntSetting("XSECURELOCK_AUTH_Y_POSITION", 50, 0, 100);
#ifdef HAVE_XKB_EXT
  config->show_keyboard_layout =
      GetBoolSetting("XSECURELOCK_SHOW_KEYBOARD_LAYOUT", 1);
  config->show_locks_and_latches =
      GetBoolSetting("XSECURELOCK_SHOW_LOCKS_AND_LATCHES", 0);
  config->layout_switch_key_name =
      GetKeySymSetting("XSECURELOCK_LAYOUT_SWITCH_KEYSYM", "Tab", NULL);
#endif

  if (!GetPromptDisplayModeFromFlags(paranoid_password_flag,
                                     password_prompt_flag,
                                     &config->prompt_display_mode)) {
    Log("Invalid XSECURELOCK_PASSWORD_PROMPT value; defaulting to cursor");
    config->prompt_display_mode = PROMPT_DISPLAY_MODE_CURSOR;
  }

  if (!GetHostName(config->hostname, sizeof(config->hostname))) {
    return 0;
  }
  if (!GetUserName(config->username, sizeof(config->username))) {
    return 0;
  }

  return 1;
}
