#ifndef __FINWO_JERRY_H__
#define __FINWO_JERRY_H__

#include "finwo/http-server.h"

#define JERRY_RETURNCODE               int
#define JERRY_RETURNCODE_OK            0
#define JERRY_RETURNCODE_ERROR_GENERIC -1

struct jerry_ev {
  char     *publicKey;
  char     *body;
  uint16_t sequence;
  char     *signature;
};

void jerry_ev_free(struct jerry_ev *ev);

// Client, supports http only (for now)
void jerry_join(const char *url);

// Server, http mode
void jerry_register(const char *path);
void jerry_onClose(struct http_server_reqdata *reqdata, void *udata);

// // Server, tcp mode
// void jerry_listen(int port);

#endif // __FINWO_JERRY_H__
