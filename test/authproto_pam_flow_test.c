#include <stdlib.h>

#define main AuthprotoPamMain
int AuthprotoPamMain(void);
#include "../helpers/authproto_pam.c"
#undef main

struct pam_handle {
  int unused;
};

static struct pam_handle test_pam;

static int auth_status;
static int acct_status;
static int chauthtok_status;
static int auth_calls;
static int acct_calls;
static int chauthtok_calls;
static int setcred_calls;

static void ResetFakePam(int new_auth_status, int new_acct_status,
                         int new_chauthtok_status) {
  auth_status = new_auth_status;
  acct_status = new_acct_status;
  chauthtok_status = new_chauthtok_status;
  auth_calls = 0;
  acct_calls = 0;
  chauthtok_calls = 0;
  setcred_calls = 0;
  unsetenv("XSECURELOCK_PAM_SERVICE");
  unsetenv("XSECURELOCK_NO_PAM_RHOST");
  unsetenv("XSECURELOCK_ALLOW_NULL_PAM_AUTHTOK");
}

static int RunAuthenticate(void) {
  struct pam_conv conv = {
      .conv = Converse,
      .appdata_ptr = NULL,
  };
  pam_handle_t *pam = NULL;
  return Authenticate(&conv, &pam);
}

static void ExpectSuccessWithoutTokenChange(void) {
  ResetFakePam(PAM_SUCCESS, PAM_SUCCESS, PAM_PERM_DENIED);

  if (RunAuthenticate() != PAM_SUCCESS) {
    abort();
  }
  if (auth_calls != 1 || acct_calls != 1 || chauthtok_calls != 0 ||
      setcred_calls != 1) {
    abort();
  }
}

static void ExpectExpiredTokenChangeSuccessUnlocks(void) {
  ResetFakePam(PAM_SUCCESS, PAM_NEW_AUTHTOK_REQD, PAM_SUCCESS);

  if (RunAuthenticate() != PAM_SUCCESS) {
    abort();
  }
  if (auth_calls != 1 || acct_calls != 1 || chauthtok_calls != 1 ||
      setcred_calls != 1) {
    abort();
  }
}

static void ExpectExpiredTokenChangeFailureBlocksUnlock(void) {
  ResetFakePam(PAM_SUCCESS, PAM_NEW_AUTHTOK_REQD, PAM_PERM_DENIED);

  if (RunAuthenticate() != PAM_PERM_DENIED) {
    abort();
  }
  if (auth_calls != 1 || acct_calls != 1 || chauthtok_calls != 3 ||
      setcred_calls != 0) {
    abort();
  }
}

static void ExpectOrdinaryAccountFailureUsesBuildPolicy(void) {
  ResetFakePam(PAM_SUCCESS, PAM_PERM_DENIED, PAM_SUCCESS);

#ifdef PAM_CHECK_ACCOUNT_TYPE
  if (RunAuthenticate() != PAM_PERM_DENIED) {
    abort();
  }
  if (setcred_calls != 0) {
    abort();
  }
#else
  if (RunAuthenticate() != PAM_SUCCESS) {
    abort();
  }
  if (setcred_calls != 1) {
    abort();
  }
#endif
  if (auth_calls != 1 || acct_calls != 3 || chauthtok_calls != 0) {
    abort();
  }
}

int pam_start(const char *service_name, const char *user,
              const struct pam_conv *pam_conversation, pam_handle_t **pamh) {
  (void)service_name;
  (void)user;
  (void)pam_conversation;
  *pamh = &test_pam;
  return PAM_SUCCESS;
}

int pam_set_item(pam_handle_t *pamh, int item_type, const void *item) {
  (void)pamh;
  (void)item_type;
  (void)item;
  return PAM_SUCCESS;
}

int pam_authenticate(pam_handle_t *pamh, int flags) {
  (void)pamh;
  (void)flags;
  ++auth_calls;
  return auth_status;
}

int pam_acct_mgmt(pam_handle_t *pamh, int flags) {
  (void)pamh;
  (void)flags;
  ++acct_calls;
  return acct_status;
}

int pam_chauthtok(pam_handle_t *pamh, int flags) {
  (void)pamh;
  if (flags != PAM_CHANGE_EXPIRED_AUTHTOK) {
    abort();
  }
  ++chauthtok_calls;
  return chauthtok_status;
}

int pam_setcred(pam_handle_t *pamh, int flags) {
  (void)pamh;
  if (flags != PAM_REFRESH_CRED) {
    abort();
  }
  ++setcred_calls;
  return PAM_SUCCESS;
}

int pam_end(pam_handle_t *pamh, int pam_status) {
  (void)pamh;
  (void)pam_status;
  return PAM_SUCCESS;
}

const char *pam_strerror(pam_handle_t *pamh, int errnum) {
  (void)pamh;
  (void)errnum;
  return "fake PAM error";
}

int main(void) {
  ExpectSuccessWithoutTokenChange();
  ExpectExpiredTokenChangeSuccessUnlocks();
  ExpectExpiredTokenChangeFailureBlocksUnlock();
  ExpectOrdinaryAccountFailureUsesBuildPolicy();
  return 0;
}
