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

void _jerry_respond_error(
  struct hs_udata *hsdata,
  int status,
  const char *message
) {
  response->status = status;
  http_parser_header_set(response, "Content-Type", "application/json");
  asprintf(&(response->body), "{\"ok\":false,\"message\":\"%s\"}", message);
  response->bodysize = strlen(response->body);
  char *buffer = http_parser_sprint_response(response);
  evio_conn_write(hsdata->connection, buffer, strlen(buffer));
  free(buffer);
  evio_conn_close(hsdata->connection);
}

void jerry_route_post(struct hs_udata *hsdata) {
  struct evio_conn *conn = hsdata->connection;

  struct llist_conns *listener;
  struct llist_conns *prev_listener = NULL;
  char *chunk_json;
  char *chunk;

  // Create easy references
  struct http_parser_message *request  = hsdata->reqres->request;
  struct http_parser_message *response = hsdata->reqres->response;

  // Parse and validate the body
  JSON_Value *jEvent = json_parse_string(request->body);
  if (json_value_get_type(jEvent) != JSONObject) {
    json_value_free(jEvent);
    _jerry_respond_error(hsdata, 422, "Only JSON objects are allowed");
    return;
  }

  // TODO: check if the required fields are set

  // Here = valid json object, we need to propagate the event to all listeners

  // Pre-render json into chunk to distribute
  chunk_json = json_serialize_to_string(jEvent);
  asprintf(&chunk, "%x\r\n%s\n\r\n", strlen(chunk_json) + 1, chunk_json);
  json_value_free(jEvent);
  json_free_serialized_string(chunk_json);

  // Dsitribute
  listener = listeners;
  while(listener) {
    evio_conn_write(listener->conn, event_buffer, strlen(event_buffer));
    prev_listener = listener;
    listener      = listener->next;
  }

  // Clean up event
  free(chunk);

  // Build response
  response->status = 200;
  http_parser_header_set(response, "Content-Type", "application/json");
  response->body     = strdup("{\"ok\":true}");
  response->bodysize = strlen(response->body);

  // Send response
  response_buffer = http_parser_sprint_response(response);
  evio_conn_write(hsdata->connection, response_buffer, strlen(response_buffer));
  free(response_buffer);

  // Aanndd.. we're done
  evio_conn_close(hsdata->connection);
}

void jerry_route_get(struct hs_udata *hsdata) {
  struct evio_conn *conn  = hsdata->connection;

  // Fetching the request
  // Has been wrapped in http_parser_event to support more features in the future
  struct http_parser_message *request  = hsdata->reqres->request;
  struct http_parser_message *response = hsdata->reqres->response;

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
