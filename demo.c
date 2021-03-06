// update the demo.c file to use the existing folder structure and makefile
#include "websockets.h"  // includes the "http.h" header

#include <stdio.h>
#include <stdlib.h>
#include "bscrypt.h"

/*****************************
The Websocket echo implementation
*/

void ws_open(ws_s* ws) {
  fprintf(stderr, "Opened a new websocket connection (%p)\n", ws);
}

void ws_echo(ws_s* ws, char* data, size_t size, uint8_t is_text) {
  // echos the data to the current websocket
  websocket_write(ws, data, size, 1);
}

void ws_shutdown(ws_s* ws) {
  websocket_write(ws, "Shutting Down", 13, 1);
}

void ws_close(ws_s* ws) {
  fprintf(stderr, "Closed websocket connection (%p)\n", ws);
}

/*****************************
The Websocket Broadcast implementation
*/

/* websocket broadcast data */
struct ws_data {
  size_t size;
  char data[];
};
/* free the websocket broadcast data */
void free_wsdata(ws_s* ws, void* arg) {
  free(arg);
}
/* the broadcast "task" performed by `Websocket.each` */
void ws_get_broadcast(ws_s* ws, void* arg) {
  struct ws_data* data = arg;
  websocket_write(ws, data->data, data->size, 1);  // echo
}
/* The websocket broadcast server's `on_message` callback */

void ws_broadcast(ws_s* ws, char* data, size_t size, uint8_t is_text) {
  // Copy the message to a broadcast data-packet
  struct ws_data* msg = malloc(sizeof(*msg) + size);
  msg->size = size;
  memcpy(msg->data, data, size);
  // Asynchronously calls `ws_get_broadcast` for each of the websockets
  // (except this one)
  // and calls `free_wsdata` once all the broadcasts were perfomed.
  websocket_each(ws, ws_get_broadcast, msg, free_wsdata);
  // echos the data to the current websocket
  websocket_write(ws, data, size, 1);
}

/*****************************
The HTTP implementation
*/

void on_request(http_request_s* request) {
  // to log we will start a response.
  http_response_s response = http_response_init(request);
  // http_response_log_start(&response);
  // upgrade requests to broadcast will have the following properties:
  if (request->upgrade && !strcmp(request->path, "/broadcast")) {
    // Websocket upgrade will use our existing response (never leak responses).
    websocket_upgrade(.request = request, .on_message = ws_broadcast,
                      .on_open = ws_open, .on_close = ws_close,
                      .on_shutdown = ws_shutdown, .response = &response);

    return;
  }
  // other upgrade requests will have the following properties:
  if (request->upgrade) {
    websocket_upgrade(.request = request, .on_message = ws_echo,
                      .on_open = ws_open, .on_close = ws_close, .timeout = 4,
                      .on_shutdown = ws_shutdown, .response = &response);
    return;
  }
  // HTTP response
  http_response_write_body(&response, "Hello World!", 12);
  http_response_finish(&response);
}

/*****************************
The main function
*/
#define THREAD_COUNT 8
int main(int argc, char const* argv[]) {
  http_parser_test();
  const char* public_folder = "./public_www";
  http1_listen("3000", NULL, .on_request = on_request,
               .public_folder = public_folder, .log_static = 1);
  server_run(.threads = THREAD_COUNT);
  return 0;
}
