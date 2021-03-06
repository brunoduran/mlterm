/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_unistd.h"

#include <time.h>
#include <unistd.h> /* select */

#ifdef USE_WIN32API
#include <windows.h>
#endif

/* --- global functions --- */

#ifndef HAVE_USLEEP

int __bl_usleep(u_int microseconds) {
#ifndef USE_WIN32API
  struct timeval tval;

  tval.tv_usec = microseconds % 1000000;
  tval.tv_sec = microseconds / 1000000;

  if (select(0, NULL, NULL, NULL, &tval) == 0) {
    return 0;
  } else {
    return -1;
  }
#else
  Sleep(microseconds / 1000); /* sleep mili-seconds */
  return 0;
#endif /* USE_WIN32API */
}

#endif /* HAVE_USLEEP */

#ifndef HAVE_SETENV

#include <string.h>
#include <stdio.h>
#include <stdlib.h> /* putenv */

int __bl_setenv(const char *name, const char *value, int overwrite) {
  char *env;

  if (!overwrite && getenv(name)) {
    return 0;
  }

  /* XXX Memory leaks. */
  if (!(env = malloc(strlen(name) + 1 + strlen(value) + 1))) {
    return -1;
  }

  sprintf(env, "%s=%s", name, value);

  return putenv(env);
}

#endif /* HAVE_SETENV */

#ifndef HAVE_GETUID

uid_t __bl_getuid(void) { return 0; }

#endif /* HAVE_GETUID */

#ifndef HAVE_GETGID

gid_t __bl_getgid(void) { return 0; }

#endif /* HAVE_GETGID */
