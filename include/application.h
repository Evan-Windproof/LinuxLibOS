/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINUXLIBOS_APPLICATION_H
#define LINUXLIBOS_APPLICATION_H

#include <stddef.h>

enum { APP_CLEANUP_FAILED = 74 };

int app_run(int (*entry)(void));
_Noreturn void app_abort(unsigned status);

int screen_write(const char *text);
int screen_clear(void);
int screen_wait_enter(void);

extern unsigned app_returns;
extern unsigned app_leak_count;
extern int app_last_status;

#endif
