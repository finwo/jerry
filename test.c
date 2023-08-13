#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/http-parser.h"
#include "finwo/http-server.h"

#include "jerry.h"

void onServing(const char *addr, uint16_t port, void *udata) {
  printf("\nServing at %s:%d\n", addr, port);
}

void route_get_hello(struct http_server_reqdata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  reqdata->reqres->response->body       = calloc(1, sizeof(struct buf));
  reqdata->reqres->response->body->data = strdup("Hello World!!");
  reqdata->reqres->response->body->len  = strlen(reqdata->reqres->response->body->data);
  reqdata->reqres->response->body->cap  = reqdata->reqres->response->body->len + 1;
  http_server_response_send(reqdata, true);
  return;
}

void route_404(struct http_server_reqdata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  reqdata->reqres->response->status   = 404;
  reqdata->reqres->response->body       = calloc(1, sizeof(struct buf));
  reqdata->reqres->response->body->data = strdup("Not found");
  reqdata->reqres->response->body->len  = strlen(reqdata->reqres->response->body->data);
  reqdata->reqres->response->body->cap  = reqdata->reqres->response->body->len + 1;
  http_server_response_send(reqdata, true);
  return;
}

void jerry_svc_onClose(struct http_server_reqdata *reqdata) {
  jerry_onClose(reqdata, NULL);
}

int main(int argc, char *argv[]) {
  char *addr    = "localhost";
  int port = 4000;

  int i;

  for(i=1; i<argc; i++) {

    if (
      (!strcmp("--address", argv[i])) ||
      (!strcmp("--addr", argv[i])) ||
      (!strcmp("-a", argv[i]))
    ) {
      i++;
      addr = argv[i];
      continue;
    }

    if (
      (!strcmp("--port", argv[i])) ||
      (!strcmp("-p", argv[i]))
    ) {
      i++;
      port = atoi(argv[i]);
      continue;
    }

    if (!strcmp("--join", argv[i])) {
      i++;
      jerry_join(argv[i]);
      continue;
    }

  }

  struct http_server_events evs = {
    /* .tick     = NULL, */
    .serving  = onServing,
    /* .error    = NULL, */
    .close    = jerry_svc_onClose,
    .notFound = route_404
  };

  jerry_register("/api/v1/jerry");
  http_server_route("GET", "/hello", route_get_hello);
  http_server_main(&(const struct http_server_opts){
    .evs    = &evs,
    .addr   = addr,
    .port   = (uint16_t) port,
  });

  return 0;
}
