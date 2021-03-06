/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <Application.h>

#include <pthread.h>

extern "C" {
#include <pobl/bl_conf_io.h>
#include <pobl/bl_debug.h>
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_unistd.h>
#include "main_loop.h"
}

#if defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

/* --- static functions --- */

static void *pty_watcher(void *app) {
  pthread_detach(pthread_self());

  main_loop_start();

  ((BApplication*)app)->PostMessage(B_QUIT_REQUESTED);

  return NULL;
}

/* --- global functions --- */

int main(int argc, char* argv[]) {
	BApplication app("application/x-vnd.mlterm");
  pthread_t thrd;

	bl_set_sys_conf_dir(CONFIG_PATH);
  bl_set_msg_log_file_name("mlterm/msg.log");

  main_loop_init(argc, argv);

  pthread_create(&thrd, NULL, pty_watcher, (void*)&app);
	app.Run();

  return 0;
}
