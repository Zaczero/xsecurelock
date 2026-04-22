#include "env_info.h"

#include <errno.h>
#include <pwd.h>     // for getpwuid_r, passwd
#include <stdlib.h>  // for free, malloc, size_t
#include <string.h>  // for memcpy, strlen
#include <unistd.h>  // for gethostname, getuid, read, sysconf

#include "buf_util.h"
#include "logging.h"
#include "mlock_page.h"

int GetHostName(char* hostname_buf, size_t hostname_buflen) {
  if (gethostname(hostname_buf, hostname_buflen)) {
    LogErrno("gethostname");
    return 0;
  }
  hostname_buf[hostname_buflen - 1] = 0;
  return 1;
}

int GetUserName(char* username_buf, size_t username_buflen) {
  struct passwd* pwd = NULL;
  struct passwd pwd_storage;
  char* pwd_buf = NULL;
  int ok = 0;
  size_t pwd_bufsize = 1 << 20;
  long sysconf_bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (sysconf_bufsize > 0) {
    pwd_bufsize = (size_t)sysconf_bufsize;
  }
  pwd_buf = malloc(pwd_bufsize);
  if (!pwd_buf) {
    LogErrno("malloc(pwd_bufsize)");
    return 0;
  }
  if (MLOCK_PAGE(pwd_buf, pwd_bufsize) < 0) {
    // We continue anyway, as very likely getpwuid_r won't retrieve a password
    // hash on modern systems.
    LogErrno("mlock");
  }
  int status =
      getpwuid_r(getuid(), &pwd_storage, pwd_buf, pwd_bufsize, &pwd);
  if (status != 0) {
    errno = status;
    LogErrno("getpwuid_r");
    goto done;
  }
  if (pwd == NULL) {
    Log("getpwuid_r returned no passwd entry for uid %ld", (long)getuid());
    goto done;
  }
  size_t username_len = strlen(pwd->pw_name);
  if (username_len >= username_buflen) {
    Log("Username too long: got %d, want < %d", (int)username_len,
        (int)username_buflen);
    goto done;
  }
  memcpy(username_buf, pwd->pw_name, username_len + 1);
  ok = 1;

done:
  ClearFreeBuffer(&pwd_buf, pwd_bufsize);
  return ok;
}
