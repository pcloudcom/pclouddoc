#ifndef _BIN_API_H
#define _BIN_API_H

#include <stdint.h>
#include <string.h>
#include <openssl/ssl.h>

#define API_HOST     "api.pcloud.com"
#define API_PORT     "8398"
#define API_PORT_SSL "8399"

#define PARAM_STR   0
#define PARAM_NUM   1
#define PARAM_BOOL  2

#define PARAM_ARRAY 3
#define PARAM_HASH  4
#define PARAM_DATA  5

#define PARAM_END 255

#define PTR_OK ((binresult *)1)

typedef struct {
  uint16_t paramtype;
  uint16_t paramnamelen;
  uint32_t opts;
  const char *paramname;
  union {
    uint64_t num;
    const char *str;
  } un;
} binparam;

struct _binresult;

typedef struct _hashpair {
  const char *key;
  struct _binresult *value;
} hashpair;

typedef struct _binresult{
  uint32_t type;
  uint32_t length;
  union {
    uint64_t num;
    const char *str;
    struct _binresult **array;
    struct _hashpair *hash;
  };
} binresult;

typedef struct {
  int sock;
  SSL *ssl;
} apisock;

#define P_STR(name, val) {PARAM_STR, strlen(name), strlen(val), (name), .un.str=(val)}
#define P_LSTR(name, val, len) {PARAM_STR, strlen(name), (len), (name), .un.str=(val)}
#define P_NUM(name, val) {PARAM_NUM, strlen(name), 0, (name), .un.num=(val)}
#define P_BOOL(name, val) {PARAM_BOOL, strlen(name), 0, (name), .un.num=(val)?1:0}

#define send_command(sock, cmd, ...) \
  ({\
    binparam params[]={__VA_ARGS__}; \
    do_send_command(sock, cmd, strlen(cmd), params, sizeof(params)/sizeof(binparam), -1, 1); \
  })
  
#define send_command_nb(sock, cmd, ...) \
  ({\
    binparam params[]={__VA_ARGS__}; \
    do_send_command(sock, cmd, strlen(cmd), params, sizeof(params)/sizeof(binparam), -1, 0); \
  })
  
#define send_data_command(sock, cmd, dlen, ...) \
  ({\
    binparam params[]={__VA_ARGS__}; \
    do_send_command(sock, cmd, strlen(cmd), params, sizeof(params)/sizeof(binparam), dlen, 0); \
  })

int writeall(apisock *sock, const void *ptr, size_t len);
ssize_t readall(apisock *sock, void *ptr, size_t len);
binresult *get_result(apisock *sock);
binresult *do_send_command(apisock *sock, const char *command, size_t cmdlen, binparam *params, size_t paramcnt, int64_t datalen, int readres);

apisock *api_connect();
apisock *api_connect_ssl();
void api_close(apisock *sock);

#endif
