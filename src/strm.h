#ifndef STRM_H
#define STRM_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifndef FALSE
# define FALSE 0
#elif FALSE
# error FALSE must be false
#endif
#ifndef TRUE
# define TRUE 1
#elif !TRUE
# error TRUE must be true
#endif
#define STRM_OK 0
#define STRM_NG 1

extern int strm_option_verbose;

/* ----- Values */
#define STRM_MAKE_TAG(n) ((uint64_t)(0xFFF0|(n)) << 48)
enum strm_value_tag {
  STRM_TAG_NAN = STRM_MAKE_TAG(0x00),
  STRM_TAG_BOOL = STRM_MAKE_TAG(0x01),
  STRM_TAG_INT = STRM_MAKE_TAG(0x02),
  STRM_TAG_LIST = STRM_MAKE_TAG(0x03),
  STRM_TAG_ARRAY = STRM_MAKE_TAG(0x04),
  STRM_TAG_STRUCT = STRM_MAKE_TAG(0x05),
  STRM_TAG_STRING_I = STRM_MAKE_TAG(0x07),
  STRM_TAG_STRING_6 = STRM_MAKE_TAG(0x08),
  STRM_TAG_STRING_O = STRM_MAKE_TAG(0x09),
  STRM_TAG_STRING_F = STRM_MAKE_TAG(0x0A),
  STRM_TAG_CFUNC = STRM_MAKE_TAG(0x0B),
  STRM_TAG_PTR = STRM_MAKE_TAG(0x0D),
  STRM_TAG_FOREIGN = STRM_MAKE_TAG(0x0F),
};

#define STRM_TAG_MASK STRM_MAKE_TAG(0x0F)
#define STRM_VAL_MASK ~STRM_TAG_MASK
#define strm_value_tag(v) ((v) & STRM_TAG_MASK)
#define strm_value_val(v) ((v) & STRM_VAL_MASK)
#define strm_value_vptr(v) (void*)(intptr_t)strm_value_val(v)
#define strm_tag_val(v,tag) ((tag)|((strm_value)(v)&STRM_VAL_MASK))
#define strm_tag_vptr(v,tag) strm_tag_val((intptr_t)v,tag)

typedef uint64_t strm_value;

struct strm_state;
struct strm_stream;
typedef int (*strm_cfunc)(struct strm_stream*, int, strm_value*, strm_value*);

typedef int32_t strm_int;
strm_value strm_cfunc_value(strm_cfunc);
strm_value strm_bool_value(int);
strm_value strm_int_value(strm_int);
strm_value strm_float_value(double);
strm_value strm_nil_value(void);

strm_cfunc strm_value_cfunc(strm_value);
strm_int strm_value_int(strm_value);
int strm_value_bool(strm_value);
double strm_value_float(strm_value);

int strm_value_eq(strm_value, strm_value);
int strm_nil_p(strm_value);
int strm_bool_p(strm_value);
int strm_number_p(strm_value);
int strm_cfunc_p(strm_value);
int strm_lambda_p(strm_value);
int strm_array_p(strm_value);
int strm_string_p(strm_value);

/* internal functions */
int strm_int_p(strm_value);
int strm_float_p(strm_value);

enum strm_ptr_type {
  STRM_PTR_STREAM,
  STRM_PTR_LAMBDA,
  STRM_PTR_GENFUNC,
  STRM_PTR_IO,
  STRM_PTR_AUX,
};

#define STRM_PTR_HEADER \
  enum strm_ptr_type type

#define STRM_AUX_HEADER \
  STRM_PTR_HEADER;\
  struct strm_state* ns

int strm_ptr_tag_p(strm_value, enum strm_ptr_type);
void* strm_value_ptr(strm_value, enum strm_ptr_type);
strm_value strm_ptr_value(void*);

strm_value strm_foreign_value(void*);
void* strm_value_foreign(strm_value);
/* ----- Strings */
struct strm_string {
  const char *ptr;
  strm_int len;
};

typedef uint64_t strm_string;

strm_string strm_str_new(const char*, strm_int);
strm_string strm_str_static(const char*, strm_int);
#define strm_strlen_lit(s) (sizeof(s "") - 1)
#define strm_str_lit(s) strm_str_static(s, strm_strlen_lit(s))

#define strm_str_value(s) (strm_value)(s)
#define strm_value_str(v) (strm_string)(v)
const char* strm_strp_ptr(strm_string*);
#define strm_str_ptr(s) strm_strp_ptr(&s)
const char* strm_str_cstr(strm_string, char buf[]);
strm_int strm_str_len(strm_string);

strm_string strm_str_intern(const char *p, strm_int len);
strm_string strm_str_intern_str(strm_string s);
strm_string strm_str_intern_static(const char *p, strm_int len);
#define strm_str_intern_lit(s) strm_str_intern_static(s, strm_strlen_lit(s))
int strm_str_eq(strm_string a, strm_string b);
int strm_str_intern_p(strm_string v);

strm_string strm_to_str(strm_value v);
#define strm_str_null 0

/* ----- Arrays */
typedef uint64_t strm_array;

struct strm_array {
  strm_int len;
  strm_value *ptr;
  strm_array headers;
  struct strm_state* ns;
};

strm_array strm_ary_new(const strm_value*, strm_int);
#define strm_ary_value(a) (strm_value)(a)
#define strm_value_ary(v) (strm_array)(v)
struct strm_array* strm_ary_struct(strm_array);
#define strm_ary_ptr(a) strm_ary_struct(a)->ptr
#define strm_ary_len(a) strm_ary_struct(a)->len
#define strm_ary_headers(a) strm_ary_struct(a)->headers
#define strm_ary_ns(a) strm_ary_struct(a)->ns

int strm_ary_eq(strm_array a, strm_array b);
#define strm_ary_null 0

/* ----- Streams */
typedef enum {
  strm_producer,
  strm_filter,
  strm_consumer,
  strm_dying,
  strm_killed,
} strm_stream_mode;

typedef struct strm_stream strm_stream;
typedef int (*strm_callback)(strm_stream*, strm_value);

struct strm_stream {
  STRM_PTR_HEADER;
  unsigned int flags;
  strm_stream_mode mode;
  strm_callback start_func;
  strm_callback close_func;
  void *data;
  strm_stream* dst;
  strm_stream** rest;
  size_t rsize;
  size_t rcapa;
  struct node_error* exc;
  strm_int refcnt;
  struct strm_queue* queue;
  strm_int excl;
};

strm_stream* strm_stream_new(strm_stream_mode mode, strm_callback start, strm_callback close, void *data);
#define strm_stream_value(t) strm_ptr_value(t)
void strm_emit(strm_stream* strm, strm_value data, strm_callback cb);
void strm_io_emit(strm_stream* strm, strm_value data, int fd, strm_callback cb);
int strm_stream_connect(strm_stream* src, strm_stream* dst);
int strm_connect(strm_stream* strm, strm_value src, strm_value dst, strm_value* ret);
int strm_loop();
void strm_stream_close(strm_stream* strm);
#define strm_stream_p(v) strm_ptr_tag_p(v, STRM_PTR_STREAM)

extern int strm_event_loop_started;
#define strm_value_stream(t) (strm_stream*)strm_value_ptr(t, STRM_PTR_STREAM)

void strm_raise(strm_stream*, const char*);
int strm_funcall(strm_stream*, strm_value, int, strm_value*, strm_value*);
void strm_eprint(strm_stream*);
int strm_parse_args(strm_stream*, int, strm_value*, const char*,...);
#define strm_get_args(strm,argc,argv,...) do {\
  if (strm_parse_args(strm,argc,argv,__VA_ARGS__) == STRM_NG)\
    return STRM_NG;\
} while(0)

/* ----- queue */
struct strm_task {
  strm_callback func;
  strm_value data;
};

struct strm_task* strm_task_new(strm_callback func, strm_value data);
void strm_task_push(strm_stream* strm, strm_callback func, strm_value data);
void strm_task_add(strm_stream* strm, struct strm_task*);

/* ----- Variables */
struct node_error;
typedef struct strm_state {
  void *env;
  struct strm_state *prev;
  uint32_t flags;
} strm_state;

/* user defined namespace that can create instances */
#define STRM_NS_UDEF 1
#define STRM_NS_UDEF_SET(ns)  ((ns)->flags |= STRM_NS_UDEF)
#define STRM_NS_UDEF_GET(ns)  ((ns)->flags & STRM_NS_UDEF)
#define STRM_NS_ALLOC_ERR(ns) ((ns) == NULL)
#define STRM_NS_EXIST_ERR(ns) ((ns) == (void*)-1)

int strm_var_set(strm_state*, strm_string, strm_value);
int strm_var_def(strm_state*, const char*, strm_value);
int strm_var_get(strm_state*, strm_string, strm_value*);
int strm_var_match(strm_state*, strm_string, strm_value);
int strm_env_copy(strm_state*, strm_state*);

/* ----- Namespaces */
strm_state* strm_ns_new(strm_state*, const char*);
strm_state* strm_ns_create(strm_state*, strm_string);
strm_state* strm_ns_get(strm_string);
strm_state* strm_value_ns(strm_value);
strm_string strm_ns_name(strm_state*);

extern strm_state* strm_ns_array;
extern strm_state* strm_ns_string;
extern strm_state* strm_ns_number;

/* ----- I/O */
#define STRM_IO_READ  1
#define STRM_IO_WRITE 2
#define STRM_IO_FLUSH 4
#define STRM_IO_READING 8

typedef struct strm_io {
  STRM_PTR_HEADER;
  int fd;
  int mode;
  strm_stream* read_stream;
  strm_stream* write_stream;
} *strm_io;

strm_value strm_io_new(int fd, int mode);
strm_stream* strm_io_stream(strm_value io, int mode);
void strm_io_start_read(strm_stream* strm, int fd, strm_callback cb);
#define strm_value_io(v) (strm_io)strm_value_ptr(v, STRM_PTR_IO)
#define strm_io_p(v) strm_ptr_tag_p(v, STRM_PTR_IO)

/* ----- lambda */
struct strm_lambda {
  STRM_PTR_HEADER;
  struct node_lambda* body;
  struct strm_state* state;
};

#define strm_value_lambda(v) (struct strm_lambda*)strm_value_ptr(v, STRM_PTR_LAMBDA)
#define strm_lambda_p(v) strm_ptr_tag_p(v, STRM_PTR_LAMBDA)

/* ----- generic function */
struct strm_genfunc {
  STRM_PTR_HEADER;
  strm_state* state;
  strm_string id;
};

/* ----- time */
int strm_time_p(strm_value);
strm_value strm_time_new(long sec, long usec, int offset);
int strm_time_parse_time(const char* s, strm_int len, long* sec, long* usec, int* offset);

/* ----- signal */
typedef void (*strm_sighandler_t)(int, void*);
int strm_signal(int sig, strm_sighandler_t func, void* arg);
int strm_unsignal(int sig, strm_sighandler_t func);
#endif

