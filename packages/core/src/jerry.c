#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finwo/http-server.h"
#include "finwo/http-parser.h"
#include "finwo/mindex.h"
#include "finwo/strnstr.h"
#include "kgabis/parson.h"
#include "orlp/ed25519.h"
#include "finwo/fnet.h"
#include "jacketizer/yuarel.h"
#include "tidwall/buf.h"

#include "jerry.h"

struct llistener {
  void *next;
  struct fnet_t *conn;
};

struct dedup_entry {
  char *pubkey;
  uint16_t seq;
};

struct http_resp_udata {
  struct fnet_t *conn;
  struct buf    *buffer;
};

struct llistener *listeners = NULL;
struct mindex_t * dedup_index = NULL;

int dedup_compare(
    const void *a,
    const void *b,
    void *udata
) {
  const struct dedup_entry *ta = a;
  const struct dedup_entry *tb = b;
  return strcmp(ta->pubkey, tb->pubkey);
}

void dedup_purge(
  void *subject,
  void *udata
) {
  struct dedup_entry *tsubject = subject;
  free(tsubject->pubkey);
  free(tsubject);
}

int isHex(const char *subject) {
  int i;
  size_t l = strlen(subject);
  for(i = 0 ; i < l ; i++) {
    if (
      (subject[i] >= 'a' && subject[i] <= 'f') ||
      (subject[i] >= 'A' && subject[i] <= 'F') ||
      (subject[i] >= '0' && subject[i] <= '9')
    ) {
      continue;
    }
    return 0;
  }
  return 1;
}

void _jerry_respond_error(
  struct http_server_reqdata *reqdata,
  int status,
  const char *message
) {
  struct http_parser_message *response = reqdata->reqres->response;
  response->status = status;
  http_parser_header_set(response, "Content-Type", "application/json");

  response->body = calloc(1, sizeof(struct buf));
  asprintf(&(response->body->data), "{\"ok\":false,\"message\":\"%s\"}", message);
  response->body->len = strlen(response->body->data);
  response->body->cap = response->body->len + 1;

  struct buf *buffer = http_parser_sprint_response(response);
  fnet_write(reqdata->connection, buffer);
  buf_clear(buffer);
  free(buffer);

  fnet_close(reqdata->connection);
}

void jerry_route_options(struct http_server_reqdata *reqdata) {

  // Create easy references
  struct http_parser_message *request  = reqdata->reqres->request;
  struct http_parser_message *response = reqdata->reqres->response;

  const char *origin = http_parser_header_get(request, "Origin");

  response->status = 200;
  http_parser_header_set(response, "Access-Control-Allow-Methods", "OPTIONS, GET, POST");
  http_parser_header_set(response, "Access-Control-Allow-Origin" , origin ? origin : "*");
  http_parser_header_set(response, "Access-Control-Allow-Headers", "Content-Type");
  http_parser_header_set(response, "Connection"                  , "close");

  // Send response
  struct buf *response_buffer = http_parser_sprint_response(response);
  fnet_write(reqdata->connection, response_buffer);
  buf_clear(response_buffer);
  free(response_buffer);

  // Aanndd.. we're done
  fnet_close(reqdata->connection);
}

void jerry_route_post(struct http_server_reqdata *reqdata) {
  struct llistener *listener;
  struct buf *response_buffer;
  char *chunk_json;
  char *chunk;
  int chunk_len;

  // Create easy references
  struct http_parser_message *request  = reqdata->reqres->request;
  struct http_parser_message *response = reqdata->reqres->response;

  // Parse and validate the body
  JSON_Value *jEvent = json_parse_string(request->body->data);
  if (json_value_get_type(jEvent) != JSONObject) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "Only JSON objects are allowed");
  }

  // Easy reference to the obj
  JSON_Object * oEvent = json_value_get_object(jEvent);

  // Check for required fields (and basic typing
  if (!json_object_has_value_of_type(oEvent, "pub", JSONString)) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "'pub' field must be a hexidecimal string containing the origin public key");
  }
  if (!json_object_has_value_of_type(oEvent, "sig", JSONString)) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "'sig' field must be a hexidecimal string representing a ed25519 signature");
  }
  if (!json_object_has_value_of_type(oEvent, "seq", JSONNumber)) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "'seq' field is required to contain a number representing the current sequence number");
  }
  if (!json_object_has_value(oEvent, "bdy")) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "Missing 'bdy' field");
  }

  // Validate pubkey/signature structure
  if (json_object_get_string_len(oEvent, "pub") != 64) { // 32 bytes, so 64 hex characters
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "'pub' field must be a hexidecimal string containing the origin public key");
  }
  if (json_object_get_string_len(oEvent, "sig") != 128) { // 64 bytes, so 128 hex characters
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "'sig' field must be a hexidecimal string representing a ed25519 signature");
  }

  // Easier reference that doesn't require more fn calls
  const char *strPub = json_object_get_string(oEvent, "pub");
  const char *strSig = json_object_get_string(oEvent, "sig");

  // Check if both strings are actualy hex
  if (!isHex(strPub)) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "'pub' field must be a hexidecimal string containing the origin public key");
  }
  if (!isHex(strSig)) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "'sig' field must be a hexidecimal string representing a ed25519 signature");
  }

  // Fetch the dedup entry for the given pubkey
  struct dedup_entry dd_pattern = { .pubkey = strdup(strPub) };
  struct dedup_entry *dd_entry = mindex_get(dedup_index, &dd_pattern);
  free(dd_pattern.pubkey);

  uint16_t seq_stored;
  uint16_t seq_gotten = (uint16_t)json_object_get_number(oEvent, "seq");
  int16_t seq_result;
  if (dd_entry) {
    seq_stored = dd_entry->seq;
    seq_result = (int16_t)(seq_gotten - seq_stored);
    if (seq_result <= 0) {
      json_value_free(jEvent);
      return _jerry_respond_error(reqdata, 422, "invalid seq");
    }
  }

  // Convert pub and sig to buffers
  char eventPub[32];
  char eventSig[64];
  int i;
  for(i = 0 ; i < 64 ; i++) {
    if (i < 32) {
      sscanf(strPub + (i*2), "%2hhx", &eventPub[i]);
    }
    sscanf(strSig + (i*2), "%2hhx", &eventSig[i]);
  }

  // Rebuild the signed message
  JSON_Value  *jEventValidate = json_value_deep_copy(jEvent);
  JSON_Object *oEventValidate = json_value_get_object(jEventValidate);
  json_object_remove(oEventValidate, "sig");
  char *strEventValidate      = json_serialize_to_string(jEventValidate);

  // Do the actual signature check
  int isValid = ed25519_verify((unsigned char *)eventSig, (unsigned char *)strEventValidate, strlen(strEventValidate), (unsigned char *) eventPub);

  // Free used memory before checking the result
  // We'll never use these values anymore
  json_free_serialized_string(strEventValidate);
  json_value_free(jEventValidate);

  if (!isValid) {
    json_value_free(jEvent);
    return _jerry_respond_error(reqdata, 422, "invalid signature");
  }

  // Here = valid json object, we need to propagate the event to all listeners

  // Create new dd_entry if missing
  if (!dd_entry) {
    dd_entry = malloc(sizeof(struct dedup_entry));
    dd_entry->pubkey = strdup(strPub);
    // seq is set later, no need here
    mindex_set(dedup_index, dd_entry);
  }

  // Update the mindex entry
  dd_entry->seq = seq_gotten;

  // Limit the length of the mindex
  // TODO: configurable length
  if (mindex_length(dedup_index) > 1024) {
    dd_entry = mindex_rand(dedup_index);
    mindex_delete(dedup_index, dd_entry);
  }

  // Caution: dd_entry is unsafe here

  // Pre-render json into chunk to distribute
  chunk_json = json_serialize_to_string(jEvent);
  asprintf(&chunk, "%x\r\n%s\n\r\n", strlen(chunk_json) + 1, chunk_json);
  json_value_free(jEvent);
  json_free_serialized_string(chunk_json);
  chunk_len = strlen(chunk);

  // Dsitribute
  listener = listeners;
  while(listener) {
    fnet_write(listener->conn, &(struct buf){
      .data = chunk,
      .len  = chunk_len,
      .cap  = chunk_len,
    });
    listener      = listener->next;
  }

  // Clean up event
  free(chunk);

  // Build response
  const char *origin = http_parser_header_get(request, "Origin");
  response->status = 200;
  http_parser_header_set(response, "Content-Type"                , "application/json"   );
  http_parser_header_set(response, "Access-Control-Allow-Origin" , origin ? origin : "*");

  response->body       = calloc(1, sizeof(struct buf));
  response->body->data = strdup("{\"ok\":true}");
  response->body->len  = strlen(response->body->data);
  response->body->cap  = response->body->len + 1;

  // Send response
  response_buffer = http_parser_sprint_response(response);
  fnet_write(reqdata->connection, response_buffer);
  buf_clear(response_buffer);
  free(response_buffer);

  // Aanndd.. we're done
  fnet_close(reqdata->connection);
}

void jerry_route_get(struct http_server_reqdata *reqdata) {
  struct fnet_t *conn = reqdata->connection;

  // Fetching the request
  // Has been wrapped in http_parser_event to support more features in the future
  struct http_parser_message *request  = reqdata->reqres->request;
  struct http_parser_message *response = reqdata->reqres->response;

  // Build response
  const char *origin = http_parser_header_get(request, "Origin");
  response->status = 200;
  http_parser_header_set(response, "Transfer-Encoding"           , "chunked"             );
  http_parser_header_set(response, "Content-Type"                , "application/x-ndjson");
  http_parser_header_set(response, "Access-Control-Allow-Origin" , origin ? origin : "*" );

  // Assign an empty body, we're not doing anything yet
  response->body = calloc(1, sizeof(struct buf));
  response->body->data = strdup("");
  response->body->len  = 0;
  response->body->cap  = 1;

  // Send response
  struct buf *response_buffer = http_parser_sprint_response(response);
  fnet_write(conn, response_buffer);
  buf_clear(response_buffer);
  free(response_buffer);

  // Add the connection to listener list
  struct llistener *listener = malloc(sizeof(struct llistener));
  listener->conn = conn;
  listener->next = listeners;
  listeners      = listener;

  // Intentionally NOT closing the connection
}

void jerry_onClose(struct http_server_reqdata *reqdata, void *udata) {
  struct fnet_t *conn = reqdata->connection;

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

void jerry_register(const char *path) {
  dedup_index = mindex_init(dedup_compare, dedup_purge, NULL);

  http_server_route("GET"    , path, jerry_route_get);
  http_server_route("POST"   , path, jerry_route_post);
  http_server_route("OPTIONS", path, jerry_route_options);
}

void _jerry_join_onConnect(struct fnet_ev *ev) {
  struct http_parser_pair *reqres = ev->udata;
  struct buf *req = http_parser_sprint_pair_request(reqres);
  int e = fnet_write(ev->connection, req);
  buf_clear(req);
  free(req);
}

void _jerry_join_onData(struct fnet_ev *ev) {
  struct http_parser_pair *reqres = ev->udata;
  http_parser_pair_response_data(reqres, ev->buffer);
  printf("onData: %*s\n", ev->buffer->len, ev->buffer->data);
}

void _jerry_join_onChunk(struct http_parser_event *ev) {
  char *index;
  struct http_resp_udata *dat = ev->udata;
  buf_append(dat->buffer, ev->chunk->data, ev->chunk->len);

  // Split by first newline
  if (!(index = strnstr(dat->buffer->data, "\n", dat->buffer->len))) {
    // No newline = no json document
    return;
  }
  *(index) = "\0";


  printf("onChunk: nice\n");
}

void _jerry_join_onResponse(struct fnet_ev *ev) {
  printf("onResponse: death expected.. Reconnect?\n");
}

void _jerry_join_onClose(struct fnet_ev *ev) {
  printf("onClose\n");
}

void jerry_join(const char *url) {
  int isSupported = 0;
  char *urlcopy = strdup(url);
  struct yuarel parsed;

  yuarel_parse(&parsed, urlcopy);

  // TODO: clean this up (http-client lib with streamed body support?)

  // http fallback port
  if ((!strcmp(parsed.scheme, "http")) && (!parsed.port)) {
    parsed.port = 80;
  }
  if ((!strcmp(parsed.scheme, "tcp")) && (!parsed.port)) {
    parsed.port = 80;
  }

  if (
    (!strcmp(parsed.scheme, "http")) ||
    (!strcmp(parsed.scheme, "tcp"))
  ) {
    isSupported = 1;
  }

  if (!isSupported) {
    fprintf(stderr, "Only http/tcp connections supported\n");
    exit(1);
  }
  if (!parsed.host) {
    fprintf(stderr, "Missing host in url\n");
    exit(1);
  }

  struct http_parser_pair *reqres = http_parser_pair_init(NULL);
  http_parser_header_set(reqres->request, "Host", parsed.host);
  reqres->request->version  = strdup("1.1");
  reqres->request->method   = strdup("GET");

  reqres->response->onChunk = _jerry_join_onChunk;
  reqres->response->udata   = calloc(1, sizeof(struct http_resp_udata));
  reqres->onResponse        = _jerry_join_onResponse;

  ((struct http_resp_udata *)(reqres->response->udata))->buffer = calloc(1, sizeof(struct buf));

  if (parsed.path ) reqres->request->path  = strdup(parsed.path);
  if (parsed.query) reqres->request->query = strdup(parsed.query);

  struct fnet_t *conn = fnet_connect(parsed.host, parsed.port, &((struct fnet_options_t){
    .proto     = FNET_PROTO_TCP,
    .flags     = 0,
    .onConnect = _jerry_join_onConnect,
    .onData    = _jerry_join_onData,
    .onTick    = NULL,
    .onClose   = _jerry_join_onClose,
    .udata     = reqres,
  }));

  printf("Scheme  : %s\n", parsed.scheme);
  printf("Host    : %s\n", parsed.host);
  printf("Port    : %d\n", parsed.port);
  printf("Path    : %s\n", parsed.path);
  printf("Query   : %s\n", parsed.query);
  printf("Fragment: %s\n", parsed.fragment);
  printf("Username: %s\n", parsed.username);
  printf("Password: %s\n", parsed.password);

  free(urlcopy);


  /* if (strstr(target, "http://") == target) { */
  /*   target += 7; */
  /*   mode    = "http"; */
  /* /1* } else if (strstr(target, "tcp://") == target) { *1/ */
  /* /1* target += 6; *1/ */
  /* /1*   mode    = "tcp"; *1/ */
  /* } else { */
  /*   fprintf(stderr, "Unsupported target url: %s\n", target); */
  /*   exit(1); */
  /* } */

  /* fprintf(stderr, "Join feature not implemented yet\n"); */
}
