#ifndef __FINWO_JERRY_H__
#define __FINWO_JERRY_H__

#include "finwo/http-server.h"

// Client, supports http only (for now)
void jerry_join(const char *url);

// Server, http mode
void jerry_register(const char *path);
void jerry_onClose(struct hs_udata *hsdata, void *udata);

// // Server, tcp mode
// void jerry_listen(int port);

#endif // __FINWO_JERRY_H__
