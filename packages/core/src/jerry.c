#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/http-server.h"
#include "finwo/http-parser.h"
#include "kgabis/parson.h"
#include "tidwall/evio.h"

#include "jerry.h"

struct llistener {
  void *next;
  struct evio_conn *conn;
};

struct llistener *listeners = NULL;

void jerry_route_post(struct http_parser_event *ev) {
  struct hs_udata *hsdata = ev->udata;
  struct evio_conn *conn  = hsdata->connection;

  /* struct conn_udata *conndata = ev->udata; */
  /* struct llist_conns *listener; */
  /* struct llist_conns *prev_listener = NULL; */
  /* char *response_buffer; */
  /* char *event_buffer; */
  /* char *event_tmp; */
  /* int listener_count = 0; */

  /* // Fetching the request */
  /* // Has been wrapped in http_parser_event to support more features in the future */
  /* struct http_parser_message *request  = ev->request; */
  /* struct http_parser_message *response = ev->response; */
  /* struct http_parser_header *header    = NULL; */

  /* // Parse and validate the body */
  /* JSON_Value *jEvent = json_parse_string(request->body); */
  /* if (json_value_get_type(jEvent) != JSONObject) { */
  /*   json_value_free(jEvent); */

  /*   // Send error in return */
  /*   response->status = 422; */
  /*   http_parser_header_set(response, "Content-Type", "application/json"); */
  /*   response->body     = strdup("{\"ok\":false,\"message\":\"Only JSON objects are allowed\"}"); */
  /*   response->bodysize = strlen(response->body); */
  /*   response_buffer    = http_parser_sprint_response(response); */
  /*   evio_conn_write(conndata->connection, response_buffer, strlen(response_buffer)); */
  /*   free(response_buffer); */
  /*   evio_conn_close(conndata->connection); */
  /*   return; */
  /* } */

  /* // Here = valid json object, we need to propagate the event to all listeners */

  /* // Pre-render json to distribute */
  /* event_tmp = json_serialize_to_string(jEvent); */
  /* asprintf(&event_buffer, "%x\r\n%s\n\r\n", strlen(event_tmp) + 1, event_tmp); */
  /* json_value_free(jEvent); */
  /* free(event_tmp); */

  /* // Dsitribute */
  /* listener = listeners; */
  /* while(listener) { */
  /*   listener_count++; */
  /*   evio_conn_write(listener->data, event_buffer, strlen(event_buffer)); */
  /*   prev_listener = listener; */
  /*   listener      = listener->next; */
  /* } */

  /* printf("Listener count: %d\n", listener_count); */

  /* // Clean up event */
  /* free(event_buffer); */

  /* // Build response */
  /* response->status = 200; */
  /* http_parser_header_set(response, "Content-Type", "application/json"); */
  /* response->body     = strdup("{\"ok\":true}"); */
  /* response->bodysize = strlen(response->body); */

  /* // Send response */
  /* response_buffer = http_parser_sprint_response(response); */
  /* evio_conn_write(conndata->connection, response_buffer, strlen(response_buffer)); */
  /* free(response_buffer); */

  /* // Aanndd.. we're done */
  /* evio_conn_close(conndata->connection); */
}

void jerry_route_get(struct hs_udata *hsdata) {
  struct evio_conn *conn  = hsdata->connection;

  // Fetching the request
  // Has been wrapped in http_parser_event to support more features in the future
  struct http_parser_message *request  = hsdata->reqres->request;
  struct http_parser_message *response = hsdata->reqres->response;
  struct http_parser_header *header    = NULL;

  // Build response
  response->status = 200;
  http_parser_header_set(response, "Transfer-Encoding", "chunked"             );
  http_parser_header_set(response, "Content-Type"     , "application/x-ndjson");

  // Assign an empty body, we're not doing anything yet
  response->body     = strdup("");
  response->bodysize = 0;

  // Send response
  char *response_buffer = http_parser_sprint_response(response);
  evio_conn_write(conn, response_buffer, strlen(response_buffer));
  free(response_buffer);

  // Add the connection to listener list
  struct llistener *listener = malloc(sizeof(struct llistener));
  listener->conn = conn;
  listener->next = listeners;
  listeners      = listener;

  // Intentionally NOT closing the connection
}

void jerry_onClose(struct hs_udata *hsdata, void *udata) {
  struct evio_conn *conn = hsdata->connection;

  struct llistener *listener      = listeners;
  struct llistener *prev_listener = NULL;

  while(listener) {
    if (listener->conn == conn) {
      if (prev_listener) {
        prev_listener->next = listener->next;
      } else {
        listeners = listener->next;
      }
      free(listener);
      break;
    }
    prev_listener = listener;
    listener      = listener->next;
  }
}

void jerry_register(char *path) {
  http_server_route("GET" , path, jerry_route_get);
  http_server_route("POST", path, jerry_route_post);
}
