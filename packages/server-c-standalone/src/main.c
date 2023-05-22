#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/http-parser.h"
#include "finwo/http-server.h"

void onServing(const char **addrs, int naddrs, void *udata) {
  for (int i = 0; i < naddrs; i++) {
    printf("Serving at %s\n", addrs[i]);
  }
}

void route_get_hello(struct hs_udata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  reqdata->reqres->response->body     = strdup("Hello World!!");
  reqdata->reqres->response->bodysize = strlen(reqdata->reqres->response->body);
  http_server_response_send(reqdata, true);
  return;
}

void route_404(struct hs_udata *reqdata) {
  http_parser_header_set(reqdata->reqres->response, "Content-Type", "text/plain");
  reqdata->reqres->response->status   = 404;
  reqdata->reqres->response->body     = strdup("Not found");
  reqdata->reqres->response->bodysize = strlen(reqdata->reqres->response->body);
  http_server_response_send(reqdata, true);
  return;
}

int main(int argc, char *argv[]) {
  char *addr_intermediate = "localhost:4000";
  int i;

  for(i=0; i<argc; i++) {

    if (!strcmp("--listen", argv[i])) {
      i++;
      addr_intermediate = argv[i];
      continue;
    }

  }

  char *addr;
  asprintf(&addr, "tcp://%s", addr_intermediate);
  const char *addrs[] = { addr };

  struct http_server_events evs = {
    .tick     = NULL,
    .serving  = onServing,
    .error    = NULL,
    .close    = NULL,
    .notFound = route_404
  };

  http_server_route("GET", "/hello", route_get_hello);
  http_server_main(addrs, sizeof(addrs) / sizeof(void*), &evs, NULL);

  return 42;
}
