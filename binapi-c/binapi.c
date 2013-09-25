#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "binapi.h"

#define RPARAM_STR1  0
#define RPARAM_STR2  1
#define RPARAM_STR3  2
#define RPARAM_STR4  3
#define RPARAM_RSTR1 4
#define RPARAM_RSTR2 5
#define RPARAM_RSTR3 6
#define RPARAM_RSTR4 7
#define RPARAM_NUM1  8
#define RPARAM_NUM2  9
#define RPARAM_NUM3  10
#define RPARAM_NUM4  11
#define RPARAM_NUM5  12
#define RPARAM_NUM6  13
#define RPARAM_NUM7  14
#define RPARAM_NUM8  15
#define RPARAM_HASH  16
#define RPARAM_ARRAY 17
#define RPARAM_BFALSE 18
#define RPARAM_BTRUE 19
#define RPARAM_DATA 20
/* 100-149 inclusive - strings 0-49 bytes in length without additional len parameter */
#define RPARAM_SHORT_STR_BASE 100 
/* 150-199 inclusive - reused strings id 0-49 bytes in length without additional id parameter */
#define RPARAM_SHORT_RSTR_BASE 150
/* 200-219 inclusive - small numbers 0-19 */
#define RPARAM_SMALL_NUM_BASE 200
#define RPARAM_END 255

#define VSHORT_STR_LEN 50
#define VSHORT_RSTR_CNT 50
#define VSMALL_NUMBER_NUM 20

static SSL_CTX *globalctx=NULL;

static binresult BOOL_TRUE={PARAM_BOOL, 0, {1}};
static binresult BOOL_FALSE={PARAM_BOOL, 0, {0}};

static int writeallfd(int sock, const void *ptr, size_t len){
  ssize_t res;
  while (len){
    res=write(sock, ptr, len);
    if (res==-1){
      if (errno==EINTR || errno==EAGAIN)
        continue;
      return -1;
    }
    len-=res;
    ptr+=res;
  }
  return 0;
}

static int writeallssl(SSL *ssl, const void *ptr, size_t len){
  int res;
  while (len){
    res=SSL_write(ssl, ptr, len);
    if (res<=0)
      return -1;
    len-=res;
    ptr+=res;
  }
  return 0;
}

int writeall(apisock *sock, const void *ptr, size_t len){
  if (sock->ssl)
    return writeallssl(sock->ssl, ptr, len);
  else
    return writeallfd(sock->sock, ptr, len);
}

static ssize_t readallfd(int sock, void *ptr, size_t len){
  ssize_t ret, rd;
  rd=0;
  while (rd<len){
    ret=read(sock, ptr+rd, len-rd);
    if (ret==0)
      return -1;
    else if (ret==-1){
      if (errno==EINTR)
        continue;
      else
        return -1;
    }
    rd+=ret;
  }
  return rd;
}

static ssize_t readallssl(SSL *ssl, void *ptr, size_t len){
  ssize_t rd;
  int ret;
  rd=0;
  while (rd<len){
    ret=SSL_read(ssl, ptr+rd, len-rd);
    if (ret<=0)
      return -1;
    rd+=ret;
  }
  return rd;
}

ssize_t readall(apisock *sock, void *ptr, size_t len){
  if (sock->ssl)
    return readallssl(sock->ssl, ptr, len);
  else
    return readallfd(sock->sock, ptr, len);
}


#define _NEED_DATA(cnt) if (*datalen<(cnt)) return -1
#define ALIGN_BYTES sizeof(uint64_t)

ssize_t calc_ret_len(unsigned char **data, size_t *datalen, size_t *strcnt){
  size_t type, len;
  long cond;
  _NEED_DATA(1);
  type=**data;
  (*data)++;
  (*datalen)--;
  if ((cond=(type>=RPARAM_SHORT_STR_BASE && type<RPARAM_SHORT_STR_BASE+VSHORT_STR_LEN)) || (type>=RPARAM_STR1 && type<=RPARAM_STR4)){
    if (cond)
      len=type-RPARAM_SHORT_STR_BASE;
    else{
      size_t l=type-RPARAM_STR1+1;
      _NEED_DATA(l);
      len=0;
      memcpy(&len, *data, l);
      *data+=l;
      *datalen-=l;
    }
    _NEED_DATA(len);
    *data+=len;
    *datalen-=len;
    len=((len+ALIGN_BYTES)/ALIGN_BYTES)*ALIGN_BYTES;
    (*strcnt)++;
    return sizeof(binresult)+len;
  }
  else if ((cond=(type>=RPARAM_RSTR1 && type<=RPARAM_RSTR4)) || (type>=RPARAM_SHORT_RSTR_BASE && type<RPARAM_SHORT_RSTR_BASE+VSHORT_RSTR_CNT)){
    if (cond){
      size_t l=type-RPARAM_RSTR1+1;
      _NEED_DATA(l);
      len=0;
      memcpy(&len, *data, l);
      *data+=l;
      *datalen-=l;
    }
    else
      len=type-RPARAM_SHORT_RSTR_BASE;
    if (len<*strcnt)
      return 0;
    else
      return -1;
  }
  else if ((cond=(type>=RPARAM_NUM1 && type<=RPARAM_NUM8)) || (type>=RPARAM_SMALL_NUM_BASE && type<RPARAM_SMALL_NUM_BASE+VSMALL_NUMBER_NUM)){
    if (cond){
      len=type-RPARAM_NUM1+1;
      _NEED_DATA(len);
      *data+=len;
      *datalen-=len;
    }
    return sizeof(binresult);
  }
  else if (type==RPARAM_BFALSE || type==RPARAM_BTRUE)
    return 0;
  else if (type==RPARAM_ARRAY){
    ssize_t ret, r;
    int unsigned cnt;
    cnt=0;
    ret=sizeof(binresult);
    while (**data!=RPARAM_END){
      r=calc_ret_len(data, datalen, strcnt);
      if (r==-1)
        return -1;
      ret+=r;
      cnt++;
      _NEED_DATA(1);
    }
    (*data)++;
    (*datalen)--;
    ret+=sizeof(binresult *)*cnt;
    return ret;
  }
  else if (type==RPARAM_HASH){
    ssize_t ret, r;
    int unsigned cnt;
    cnt=0;
    ret=sizeof(binresult);
    while (**data!=RPARAM_END){
      r=calc_ret_len(data, datalen, strcnt);
      if (r==-1)
        return -1;
      ret+=r;
      r=calc_ret_len(data, datalen, strcnt);
      if (r==-1)
        return -1;
      ret+=r;
      cnt++;
      _NEED_DATA(1);
    }
    (*data)++;
    (*datalen)--;
    ret+=sizeof(hashpair)*cnt;
    return ret;
  }
  else if (type==RPARAM_DATA){
    _NEED_DATA(8);
    *data+=8;
    *datalen-=8;
    return sizeof(binresult);
  }
  else
    return -1;
}

static binresult *do_parse_result(unsigned char **indata, unsigned char **odata, binresult **strings, size_t *nextstrid){
  binresult *ret;
  long cond;
  uint32_t type, len;
  type=**indata;
  (*indata)++;
  if ((cond=(type>=RPARAM_SHORT_STR_BASE && type<RPARAM_SHORT_STR_BASE+VSHORT_STR_LEN)) || (type>=RPARAM_STR1 && type<=RPARAM_STR4)){
    if (cond)
      len=type-RPARAM_SHORT_STR_BASE;
    else{
      size_t l=type-RPARAM_STR1+1;
      len=0;
      memcpy(&len, *indata, l);
      *indata+=l;
    }
    ret=(binresult *)(*odata);
    *odata+=sizeof(binresult);
    ret->type=PARAM_STR;
    strings[*nextstrid]=ret;
    (*nextstrid)++;
    ret->length=len;
    memcpy(*odata, *indata, len);
    (*odata)[len]=0;
    ret->str=(char *)*odata;
    *odata+=((len+ALIGN_BYTES)/ALIGN_BYTES)*ALIGN_BYTES;
    *indata+=len;
    return ret;
  }
  else if ((cond=(type>=RPARAM_RSTR1 && type<=RPARAM_RSTR4)) || (type>=RPARAM_SHORT_RSTR_BASE && type<RPARAM_SHORT_RSTR_BASE+VSHORT_RSTR_CNT)){
    size_t id;
    if (cond){
      len=type-RPARAM_RSTR1+1;
      id=0;
      memcpy(&id, *indata, len);
      *indata+=len;
    }
    else
      id=type-RPARAM_SHORT_RSTR_BASE;
    return strings[id];
  }
  else if ((cond=(type>=RPARAM_NUM1 && type<=RPARAM_NUM8)) || (type>=RPARAM_SMALL_NUM_BASE && type<RPARAM_SMALL_NUM_BASE+VSMALL_NUMBER_NUM)){
    ret=(binresult *)(*odata);
    *odata+=sizeof(binresult);
    ret->type=PARAM_NUM;
    if (cond){
      len=type-RPARAM_NUM1+1;
      ret->num=0;
      memcpy(&ret->num, *indata, len);
      *indata+=len;
    }
    else
      ret->num=type-RPARAM_SMALL_NUM_BASE;
    return ret;
  }
  else if (type==RPARAM_BTRUE)
    return &BOOL_TRUE;
  else if (type==RPARAM_BFALSE)
    return &BOOL_FALSE;
  else if (type==RPARAM_ARRAY){
    binresult **arr;
    int unsigned cnt, alloc;
    ret=(binresult *)(*odata);
    *odata+=sizeof(binresult);
    ret->type=PARAM_ARRAY;
    arr=NULL;
    cnt=0;
    alloc=128;
    arr=(binresult **)malloc(sizeof(binresult *)*alloc);
    while (**indata!=RPARAM_END){
      if (cnt==alloc){
        alloc*=2;
        arr=(binresult **)realloc(arr, sizeof(binresult *)*alloc);
      }
      arr[cnt++]=do_parse_result(indata, odata, strings, nextstrid);
    }
    (*indata)++;
    ret->length=cnt;
    ret->array=(struct _binresult **)*odata;
    *odata+=sizeof(struct _binresult *)*cnt;
    memcpy(ret->array, arr, sizeof(struct _binresult *)*cnt);
    free(arr);
    return ret;
  }
  else if (type==RPARAM_HASH){
    struct _hashpair *arr;
    int unsigned cnt, alloc;
    binresult *key;
    ret=(binresult *)(*odata);
    *odata+=sizeof(binresult);
    ret->type=PARAM_HASH;
    arr=NULL;
    cnt=0;
    alloc=32;
    arr=(struct _hashpair *)malloc(sizeof(struct _hashpair)*alloc);
    while (**indata!=RPARAM_END){
      if (cnt==alloc){
        alloc*=2;
        arr=(struct _hashpair *)realloc(arr, sizeof(struct _hashpair)*alloc);
      }
      key=do_parse_result(indata, odata, strings, nextstrid);
      arr[cnt].value=do_parse_result(indata, odata, strings, nextstrid);
      if (key->type==PARAM_STR){
        arr[cnt].key=key->str;
        cnt++;
      }
    }
    (*indata)++;
    ret->length=cnt;
    ret->hash=(struct _hashpair *)*odata;
    *odata+=sizeof(struct _hashpair)*cnt;
    memcpy(ret->hash, arr, sizeof(struct _hashpair)*cnt);
    free(arr);
    return ret;
  }
  else if (type==RPARAM_DATA){
    ret=(binresult *)(*odata);
    *odata+=sizeof(binresult);
    ret->type=PARAM_DATA;
    memcpy(&ret->num, *indata, 8);
    *indata+=8;
    return ret;
  }
  return NULL;
}

static binresult *parse_result(unsigned char *data, size_t datalen){
  unsigned char *datac;
  binresult **strings;
  binresult *res;
  ssize_t retlen;
  size_t datalenc, strcnt;
  datac=data;
  datalenc=datalen;
  strcnt=0;
  retlen=calc_ret_len(&datac, &datalenc, &strcnt);
  if (retlen==-1)
    return NULL;
  datac=(unsigned char *)malloc(retlen);
  strings=(binresult **)malloc(strcnt*sizeof(binresult *));
  strcnt=0;
  res=do_parse_result(&data, &datac, strings, &strcnt);
  free(strings);
  return res;
}

binresult *get_result(apisock *sock){
  unsigned char *data;
  binresult *res;
  uint32_t ressize;
  if (readall(sock, &ressize, sizeof(uint32_t))!=sizeof(uint32_t))
    return NULL;
  data=(unsigned char *)malloc(ressize);
  if (readall(sock, data, ressize)!=ressize){
    free(data);
    return NULL;
  }
  res=parse_result(data, ressize);
  free(data);
  return res;
}

binresult *do_send_command(apisock *sock, const char *command, size_t cmdlen, binparam *params, size_t paramcnt, int64_t datalen, int readres){
  size_t i, plen;
  unsigned char *data;
  void *sdata;
  /* 2 byte len (not included), 1 byte cmdlen, 1 byte paramcnt, cmdlen bytes cmd*/
  plen=cmdlen+2;
  if (datalen!=-1)
    plen+=sizeof(uint64_t);
  for (i=0; i<paramcnt; i++)
    if (params[i].paramtype==PARAM_STR)
      plen+=params[i].paramnamelen+params[i].opts+5; /* 1byte type+paramnamelen, nbytes paramnamelen, 4byte strlen, nbytes str */
    else if (params[i].paramtype==PARAM_NUM)
      plen+=params[i].paramnamelen+1+sizeof(uint64_t);
    else if (params[i].paramtype==PARAM_BOOL)
      plen+=params[i].paramnamelen+2;
  if (plen>0xffff)
    return NULL;
  sdata=data=(unsigned char *)malloc(plen+2);
  memcpy(data, &plen, 2);
  data+=2;
  if (datalen!=-1){
    *data++=cmdlen|0x80;
    memcpy(data, &datalen, sizeof(uint64_t));
    data+=sizeof(uint64_t);
  }
  else
    *data++=cmdlen;
  memcpy(data, command, cmdlen);
  data+=cmdlen;
  *data++=paramcnt;
  for (i=0; i<paramcnt; i++){
    *data++=(params[i].paramtype<<6)+params[i].paramnamelen;
    memcpy(data, params[i].paramname, params[i].paramnamelen);
    data+=params[i].paramnamelen;
    if (params[i].paramtype==PARAM_STR){
      memcpy(data, &params[i].opts, 4);
      data+=4;
      memcpy(data, params[i].un.str, params[i].opts);
      data+=params[i].opts;
    }
    else if (params[i].paramtype==PARAM_NUM){
      memcpy(data, &params[i].un.num, sizeof(uint64_t));
      data+=sizeof(uint64_t);
    }
    else if (params[i].paramtype==PARAM_BOOL)
      *data++=params[i].un.num&1;
  }
  if (writeall(sock, sdata, plen+2)){
    free(sdata);
    return NULL;
  }
  free(sdata);
  if (readres)
    return get_result(sock);
  else
    return PTR_OK;
}

static int connect_res(struct addrinfo *res){
  int sock;
  while ((sock=socket(res->ai_family, res->ai_socktype, res->ai_protocol))==-1 || connect(sock, res->ai_addr, res->ai_addrlen)==-1){
    if (sock!=-1)
      close(sock);
    if (res->ai_next)
      res=res->ai_next;
    else
      return -1;
  }
  return sock;
}

static int connect_socket(const char *host, const char *port){
  struct addrinfo hints, *res=NULL;
  struct in6_addr serveraddr;
  int sock, rc;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags=AI_NUMERICSERV;
  hints.ai_family=AF_UNSPEC;
  hints.ai_socktype=SOCK_STREAM;
  if (inet_pton(AF_INET, host, &serveraddr)==1){
    hints.ai_family = AF_INET;
    hints.ai_flags |= AI_NUMERICHOST;
  }
  else if (inet_pton(AF_INET6, host, &serveraddr)==1){
    hints.ai_family = AF_INET6;
    hints.ai_flags |= AI_NUMERICHOST;
  }
  rc=getaddrinfo(host, port, &hints, &res);
  if (rc!=0)
    return -1;
  sock=connect_res(res);
  freeaddrinfo(res);
#ifdef TCP_KEEPCNT
  if (sock!=-1){
    int sock_opt=1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &sock_opt, sizeof(sock_opt));
    sock_opt=3;
    setsockopt(sock, SOL_TCP, TCP_KEEPCNT, &sock_opt, sizeof(sock_opt));
    sock_opt=60;
    setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, &sock_opt, sizeof(sock_opt));
    sock_opt=20;
    setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, &sock_opt, sizeof(sock_opt));
  }
#endif
  return sock;
}

apisock *api_connect(){
  apisock *ret;
  int sock;
  sock=connect_socket(API_HOST, API_PORT);
  if (sock==-1)
    return NULL;
  ret=(apisock *)malloc(sizeof(apisock));
  ret->sock=sock;
  ret->ssl=NULL;
  return ret;
}

apisock *api_connect_ssl(){
  apisock *ret;
  SSL *ssl;
  int sock;
  sock=connect_socket(API_HOST, API_PORT_SSL);
  if (sock==-1)
    return NULL;
  if (!globalctx){
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    OpenSSL_add_all_ciphers();
    SSL_load_error_strings();
    globalctx=SSL_CTX_new(SSLv23_method());
    if (!globalctx)
      return NULL;
  }
  ssl=SSL_new(globalctx);
  if (!ssl){
    close(sock);
    return NULL;
  }
  SSL_set_fd(ssl, sock);
  if (SSL_connect(ssl)!=1){
    SSL_free(ssl);
    close(sock);
    return NULL;
  }
  ret=(apisock *)malloc(sizeof(apisock));
  ret->sock=sock;
  ret->ssl=ssl;
  return ret;
}

void api_close(apisock *sock){
  if (sock->ssl){
    SSL_shutdown(sock->ssl);
    SSL_free(sock->ssl);
  }
  close(sock->sock);
  free(sock);
}
