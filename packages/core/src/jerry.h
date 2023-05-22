#ifndef __FINWO_JERRY_H__
#define __FINWO_JERRY_H__

#include "finwo/http-server.h"

void jerry_register(char *path);
void jerry_onClose(struct hs_udata *hsdata, void *udata);

#endif // __FINWO_JERRY_H__
