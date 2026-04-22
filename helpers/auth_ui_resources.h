#ifndef XSECURELOCK_HELPERS_AUTH_UI_RESOURCES_H
#define XSECURELOCK_HELPERS_AUTH_UI_RESOURCES_H

#include "auth_ui.h"

int AuthUiResourcesInit(struct AuthUiResources *resources,
                        const struct AuthUiConfig *config);
void AuthUiResourcesCleanup(struct AuthUiResources *resources);

#endif  // XSECURELOCK_HELPERS_AUTH_UI_RESOURCES_H
