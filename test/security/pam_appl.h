#ifndef XSECURELOCK_TEST_SECURITY_PAM_APPL_H
#define XSECURELOCK_TEST_SECURITY_PAM_APPL_H

typedef struct pam_handle pam_handle_t;

struct pam_message {
  int msg_style;
  const char *msg;
};

struct pam_response {
  char *resp;
  int resp_retcode;
};

struct pam_conv {
  int (*conv)(int num_msg, const struct pam_message **msg,
              struct pam_response **resp, void *appdata_ptr);
  void *appdata_ptr;
};

#define PAM_SUCCESS 0
#define PAM_ABORT 1
#define PAM_MAXTRIES 2
#define PAM_NEW_AUTHTOK_REQD 3
#define PAM_PERM_DENIED 4
#define PAM_CONV_ERR 5

#define PAM_PROMPT_ECHO_OFF 1
#define PAM_PROMPT_ECHO_ON 2
#define PAM_ERROR_MSG 3
#define PAM_TEXT_INFO 4

#define PAM_RHOST 1
#define PAM_RUSER 2
#define PAM_TTY 3

#define PAM_DISALLOW_NULL_AUTHTOK 0x1
#define PAM_CHANGE_EXPIRED_AUTHTOK 0x2
#define PAM_REFRESH_CRED 0x4

int pam_start(const char *service_name, const char *user,
              const struct pam_conv *pam_conversation, pam_handle_t **pamh);
int pam_set_item(pam_handle_t *pamh, int item_type, const void *item);
int pam_authenticate(pam_handle_t *pamh, int flags);
int pam_acct_mgmt(pam_handle_t *pamh, int flags);
int pam_chauthtok(pam_handle_t *pamh, int flags);
int pam_setcred(pam_handle_t *pamh, int flags);
int pam_end(pam_handle_t *pamh, int pam_status);
const char *pam_strerror(pam_handle_t *pamh, int errnum);

#endif
