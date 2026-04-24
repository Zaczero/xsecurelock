#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../wait_pgrp.h"

enum { MAX_KILL_CALLS = 2 };

struct KillCall {
  pid_t pid;
  int signo;
};

struct KillState {
  int results[MAX_KILL_CALLS];
  int errors[MAX_KILL_CALLS];
  struct KillCall calls[MAX_KILL_CALLS];
  size_t call_count;
};

static struct KillState g_kill;

pid_t waitpid(pid_t pid, int *status, int options) {
  (void)pid;
  (void)status;
  (void)options;
  errno = EINVAL;
  return -1;
}

int kill(pid_t pid, int signo) {
  assert(g_kill.call_count < MAX_KILL_CALLS);

  size_t call = g_kill.call_count++;
  g_kill.calls[call] = (struct KillCall){
      .pid = pid,
      .signo = signo,
  };
  if (g_kill.results[call] < 0) {
    errno = g_kill.errors[call];
  }
  return g_kill.results[call];
}

static void ResetKillResults(int first_result, int first_errno,
                             int second_result, int second_errno) {
  g_kill = (struct KillState){
      .results = {first_result, second_result},
      .errors = {first_errno, second_errno},
  };
}

static void TestWaitProcUnknownErrorKeepsChildRunning(void) {
  pid_t pid = 123;
  int exit_status = 456;

  assert(WaitProc("test", &pid, 1, 0, &exit_status) == 0);
  assert(pid == 123);
  assert(exit_status == 456);
}

static void TestKillPgrpSignalsProcessGroup(void) {
  ResetKillResults(0, 0, 0, 0);

  assert(KillPgrp(123, SIGTERM) == 0);
  assert(g_kill.call_count == 1);
  assert(g_kill.calls[0].pid == -123);
  assert(g_kill.calls[0].signo == SIGTERM);
}

static void TestKillPgrpFallsBackToLeaderOnMissingGroup(void) {
  ResetKillResults(-1, ESRCH, 0, 0);

  assert(KillPgrp(123, SIGTERM) == 0);
  assert(g_kill.call_count == 2);
  assert(g_kill.calls[0].pid == -123);
  assert(g_kill.calls[0].signo == SIGTERM);
  assert(g_kill.calls[1].pid == 123);
  assert(g_kill.calls[1].signo == SIGTERM);
}

static void TestKillPgrpDoesNotFallbackOnOtherErrors(void) {
  ResetKillResults(-1, EPERM, 0, 0);

  assert(KillPgrp(123, SIGTERM) == -1);
  assert(errno == EPERM);
  assert(g_kill.call_count == 1);
  assert(g_kill.calls[0].pid == -123);
  assert(g_kill.calls[0].signo == SIGTERM);
}

int main(void) {
  TestWaitProcUnknownErrorKeepsChildRunning();
  TestKillPgrpSignalsProcessGroup();
  TestKillPgrpFallsBackToLeaderOnMissingGroup();
  TestKillPgrpDoesNotFallbackOnOtherErrors();
  return 0;
}
