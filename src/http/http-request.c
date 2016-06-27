#define _GNU_SOURCE
#include "http-request.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
// we declare because we need them... implementation comes later.
static struct HttpRequest* request_new(void);
static void request_clear(struct HttpRequest* self);
static void request_destroy(struct HttpRequest* self);
static void request_first(struct HttpRequest* self);
static int request_next(struct HttpRequest* self);
static int request_find(struct HttpRequest* self, char* const name);
static char* request_name(struct HttpRequest* self);
static char* request_value(struct HttpRequest* self);
static int request_is_request(struct HttpRequest* self);
static ssize_t decode_url(char* dest, const char* url_data, size_t length);

const struct HttpRequestClass HttpRequest = {
    // retures an new heap allocated request object
    .create = request_new,
    // releases the resources used by a request object and keeps it's memory.
    .clear = request_clear,
    // releases the resources used by a request object and frees it's memory.
    .destroy = request_destroy,
    // validated that this is a request object
    .is_request = request_is_request,

    // URL decoding
    .decode_url = decode_url,

    // Header handling

    /// restarts the header itteration
    .first = request_first,
    /// moves to the next header. returns 0 if the end of the list was
    /// reached.
    .next = request_next,
    /// finds a specific header matching the requested string.
    /// all headers are lower-case, so the string should be lower case.
    /// returns 0 if the header couldn't be found.
    .find = request_find,
    /// returns the name of the current header in the itteration cycle.
    .name = request_name,
    /// returns the value of the current header in the itteration cycle.
    .value = request_value,
};

////////////////
// The Request object pool implementation

#ifndef REQUEST_POOL_SIZE
#define REQUEST_POOL_SIZE 32
#endif

struct HttpRequestPool {
  struct HttpRequest request;
  struct HttpRequestPool* next;
};
// The global packet container pool
static struct {
  struct HttpRequestPool* _Atomic pool;
  struct HttpRequestPool* initialized;
} ContainerPool = {NULL};

static atomic_flag pool_initialized;

static void create_request_pool(void) {
  while (atomic_flag_test_and_set(&pool_initialized)) {
  }
  if (ContainerPool.initialized == NULL) {
    ContainerPool.initialized =
        calloc(REQUEST_POOL_SIZE, sizeof(struct HttpRequestPool));
    for (size_t i = 0; i < REQUEST_POOL_SIZE - 1; i++) {
      ContainerPool.initialized[i].next = &ContainerPool.initialized[i + 1];
    }
    ContainerPool.pool = ContainerPool.initialized;
  }
  atomic_flag_clear(&pool_initialized);
}

////////////////
// The Request object implementation

// The constructor
static struct HttpRequest* request_new(void) {
  if (ContainerPool.initialized == NULL)
    create_request_pool();
  struct HttpRequestPool* req = atomic_load(&ContainerPool.pool);
  while (req) {
    if (atomic_compare_exchange_weak(&ContainerPool.pool, &req, req->next))
      break;
  }
  if (!req) {
    req = calloc(sizeof(struct HttpRequest), 1);
  } else {
    memset(req, 0, sizeof(struct HttpRequest) - HTTP_HEAD_MAX_SIZE);
  }
  req->request.internal.is_request = request_is_request;
  return (struct HttpRequest*)req;
}

// the destructor
static void request_destroy(struct HttpRequest* self) {
  if (!self || !self->server)
    return;
  if (self->body_file)
    fclose(self->body_file);
  self->server = 0;
  if (ContainerPool.initialized == NULL ||
      ((struct HttpRequestPool*)self) < ContainerPool.initialized ||
      ((struct HttpRequestPool*)self) >
          (ContainerPool.initialized +
           (sizeof(struct HttpRequestPool) * REQUEST_POOL_SIZE))) {
    free(self);
  } else {
    ((struct HttpRequestPool*)self)->next = atomic_load(&ContainerPool.pool);
    for (;;) {
      if (atomic_compare_exchange_weak(&ContainerPool.pool,
                                       &((struct HttpRequestPool*)self)->next,
                                       ((struct HttpRequestPool*)self)))
        break;
    }
  }
}

// resetting the request
static void request_clear(struct HttpRequest* self) {
  if (!self || !self->server)
    return;
  if (self->body_file)
    fclose(self->body_file);
  *self = (struct HttpRequest){.internal.is_request = request_is_request};
}

// validating a request object
static int request_is_request(struct HttpRequest* self) {
  return (self && (self->internal.is_request == request_is_request));
}

// implement the following request handlers:

static void request_first(struct HttpRequest* self) {
  self->internal.pos = 0;
};
static int request_next(struct HttpRequest* self) {
  // repeat the following 2 times, as it's a name + value pair
  for (int i = 0; i < 2; i++) {
    // move over characters
    while (self->internal.pos < self->internal.max &&
           self->internal.header_hash[self->internal.pos])
      self->internal.pos++;
    // move over NULL
    while (self->internal.pos < self->internal.max &&
           !self->internal.header_hash[self->internal.pos])
      self->internal.pos++;
  }
  if (self->internal.pos == self->internal.max)
    return 0;
  return 1;
}
static int request_find(struct HttpRequest* self, char* const name) {
  self->internal.pos = 0;
  do {
    if (!strcasecmp(self->internal.header_hash + self->internal.pos, name))
      return 1;
  } while (request_next(self));
  return 0;
}
static char* request_name(struct HttpRequest* self) {
  if (!self->internal.header_hash[self->internal.pos])
    return NULL;
  return self->internal.header_hash + self->internal.pos;
};
static char* request_value(struct HttpRequest* self) {
  if (!self->internal.header_hash[self->internal.pos])
    return NULL;
  int pos = self->internal.pos;
  // move over characters
  while (pos < self->internal.max && self->internal.header_hash[pos])
    pos++;
  // move over NULL
  while (pos < self->internal.max && !self->internal.header_hash[pos])
    pos++;
  if (self->internal.pos == self->internal.max)
    return 0;
  return self->internal.header_hash + pos;
};

/* *****************************************************************************
*/

/* Credit to Jonathan Leffler for the idea */
#define hex_val(c)                                                        \
  (((c) >= '0' && (c) <= '9') ? ((c)-48) : (((c) >= 'a' && (c) <= 'f') || \
                                            ((c) >= 'A' && (c) <= 'F'))   \
                                               ? (((c) | 32) - 87)        \
                                               : ({                       \
                                                   return -1;             \
                                                   0;                     \
                                                 }))
static ssize_t decode_url(char* dest, const char* url_data, size_t length) {
  char* pos = dest;
  const char* end = url_data + length;
  while (url_data < end) {
    if (*url_data == '+') {
      // decode space
      *(pos++) = ' ';
      ++url_data;
    } else if (*url_data == '%') {
      // decode hex value
      // this is a percent encoded value.
      *(pos++) = (hex_val(url_data[1]) << 4) | hex_val(url_data[2]);
      url_data += 3;
    } else
      *(pos++) = *(url_data++);
  }
  *pos = 0;
  return pos - dest;
}
#undef hex_val
