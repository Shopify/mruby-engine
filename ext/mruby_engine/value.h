#ifndef MRUBY_ENGINE_VALUE_H
#define MRUBY_ENGINE_VALUE_H

#include <stddef.h>
#include <stdint.h>

struct me_mruby_engine;

typedef intptr_t me_host_value_t;
typedef me_host_value_t me_host_exception_t;
typedef me_host_value_t me_host_backtrace_t;

typedef unsigned long me_guest_value_t;

// We need the extern malarky, instead of just defining ME_HOST_NIL to be Qnil
// right here, because Qnil is defined by ruby.h, which we cannot include here.
// The reason is this header is included by files including mruby.h and both
// ruby.h and mruby.h define structures with the same name (RBasic, RObject,
// etc).
extern const me_host_value_t ME_HOST_NIL;
extern const me_host_value_t ME_HOST_TRUE;
extern const me_host_value_t ME_HOST_FALSE;

enum me_value_err_type {
  ME_VALUE_NO_ERR,
  ME_VALUE_UNSUPPORTED,
  ME_VALUE_OUT_OF_RANGE,
  ME_VALUE_TOO_DEEP,
  ME_VALUE_GUEST_ERR,
};

struct me_value_err {
  enum me_value_err_type type;
  union {
    struct {
      me_host_exception_t err;
    } guest_err;
  };
};

me_host_value_t me_value_to_host(
  struct me_mruby_engine *engine,
  me_guest_value_t value,
  struct me_value_err *err);

me_guest_value_t me_value_to_guest(
  struct me_mruby_engine *engine,
  me_host_value_t value,
  struct me_value_err *err);

me_host_value_t me_value_host_fixnum_new(
  long value,
  struct me_value_err *err);
me_host_value_t me_value_host_string_new(
  const char *bytes,
  size_t size,
  struct me_value_err *err);
me_host_value_t me_value_host_symbol_new(
  const char *bytes,
  size_t size,
  struct me_value_err *err);
me_host_value_t me_value_host_array_new(
  struct me_value_err *err);
void me_value_host_array_push(
  me_host_value_t array,
  me_host_value_t element,
  struct me_value_err *err);
me_host_value_t me_value_host_hash_new(
  struct me_value_err *err);
void me_value_host_hash_assoc(
  me_host_value_t hash,
  me_host_value_t key,
  me_host_value_t value,
  struct me_value_err *err);

me_guest_value_t me_value_guest_nil_new(void);
me_guest_value_t me_value_guest_false_new(void);
me_guest_value_t me_value_guest_true_new(void);
me_guest_value_t me_value_guest_fixnum_new(
  struct me_mruby_engine *engine,
  long value,
  struct me_value_err *err);
me_guest_value_t me_value_guest_string_new(
  struct me_mruby_engine *engine,
  const char *bytes,
  size_t size,
  struct me_value_err *err);
me_guest_value_t me_value_guest_symbol_new(
  struct me_mruby_engine *engine,
  const char *bytes,
  size_t size,
  struct me_value_err *err);
me_guest_value_t me_value_guest_array_new(
  struct me_mruby_engine *engine,
  struct me_value_err *err);
void me_value_guest_array_push(
  struct me_mruby_engine *engine,
  me_guest_value_t array,
  me_guest_value_t element,
  struct me_value_err *err);
me_guest_value_t me_value_guest_hash_new(
  struct me_mruby_engine *engine,
  struct me_value_err *err);
void me_value_guest_hash_assoc(
  struct me_mruby_engine *engine,
  me_guest_value_t hash,
  me_guest_value_t key,
  me_guest_value_t value,
  struct me_value_err *err);

static const int ME_HOST_DATA_DEPTH_MAX = 32;

#endif
