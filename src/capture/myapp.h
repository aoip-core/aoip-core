#pragma once

#include <inttypes.h>

#include <pthread.h>
#include <signal.h>

#include <aoip.h>

volatile sig_atomic_t caught_signal;

int set_signal(struct sigaction *, int);
void sig_handler(int);

int myapp_ao_init(aoip_ctx_t *);
int myapp_ao_release(aoip_ctx_t *);
int myapp_ao_open(aoip_ctx_t *);
int myapp_ao_close(aoip_ctx_t *);
int myapp_ao_read(aoip_ctx_t *);
int myapp_ao_write(aoip_ctx_t *);

int myapp_nt_init(aoip_ctx_t *);
int myapp_nt_release(aoip_ctx_t *);
int myapp_nt_open(aoip_ctx_t *);
int myapp_nt_close(aoip_ctx_t *);
int myapp_nt_recv(aoip_ctx_t *);
int myapp_nt_send(aoip_ctx_t *);
