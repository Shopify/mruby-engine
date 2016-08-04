#include "value.h"
#include <ruby.h>
#include <ruby/encoding.h>

static me_guest_value_t me_value_to_guest_r(
  struct me_mruby_engine *engine,
  VALUE value,
  int depth,
  struct me_value_err *err);

struct me_value_assoc_args {
  struct me_mruby_engine *engine;
  me_guest_value_t hash;
  int depth;
  struct me_value_err *err;
};

static int me_value_assoc(st_data_t kdata, st_data_t vdata, st_data_t data) {
  struct me_value_assoc_args *args = (struct me_value_assoc_args *)data;

  me_guest_value_t key = me_value_to_guest_r(
    args->engine,
    kdata,
    args->depth + 1,
    args->err);
  if (args->err->type != ME_VALUE_NO_ERR) {
    return ST_STOP;
  }

  me_guest_value_t value = me_value_to_guest_r(
    args->engine,
    vdata,
    args->depth + 1,
    args->err);
  if (args->err->type != ME_VALUE_NO_ERR) {
    return ST_STOP;
  }

  me_value_guest_hash_assoc(args->engine, args->hash, key, value, args->err);
  if (args->err->type != ME_VALUE_NO_ERR) {
    return ST_STOP;
  }

  return ST_CONTINUE;
}

static me_guest_value_t me_value_to_guest_r(
  struct me_mruby_engine *engine,
  VALUE value,
  int depth,
  struct me_value_err *err)
{
  if (depth > ME_HOST_DATA_DEPTH_MAX) {
    *err = (struct me_value_err){ .type = ME_VALUE_TOO_DEEP };
    return me_value_guest_nil_new();
  }

  enum ruby_value_type type = rb_type(value);
  switch (type) {
  case RUBY_T_NIL:
    return me_value_guest_nil_new();
  case RUBY_T_FALSE:
    return me_value_guest_false_new();
  case RUBY_T_TRUE:
    return me_value_guest_true_new();
  case RUBY_T_FIXNUM:
    return me_value_guest_fixnum_new(engine, FIX2LONG(value), err);
  case RUBY_T_STRING:
    return me_value_guest_string_new(
      engine,
      RSTRING_PTR(value),
      RSTRING_LEN(value),
      err);
  case RUBY_T_SYMBOL:
    {
      VALUE symbol_str = rb_sym2str(value);
      return me_value_guest_symbol_new(
        engine,
        RSTRING_PTR(symbol_str),
        RSTRING_LEN(symbol_str),
        err);
    }
  case RUBY_T_ARRAY:
    {
      me_guest_value_t array = me_value_guest_array_new(engine, err);
      if (err->type != ME_VALUE_NO_ERR) {
        return me_value_guest_nil_new();
      }

      for (long i = 0, f = RARRAY_LEN(value); i < f; ++i) {
        me_guest_value_t element = me_value_to_guest_r(
          engine, RARRAY_AREF(value, i), depth + 1, err);
        if (err->type != ME_VALUE_NO_ERR) {
          return me_value_guest_nil_new();
        }

        me_value_guest_array_push(engine, array, element, err);
        if (err->type != ME_VALUE_NO_ERR) {
          return me_value_guest_nil_new();
        }
      }

      return array;
    }
  case RUBY_T_HASH:
    {
      me_guest_value_t hash = me_value_guest_hash_new(engine, err);
      if (err->type != ME_VALUE_NO_ERR) {
        return ME_HOST_NIL;
      }

      struct me_value_assoc_args args = (struct me_value_assoc_args){
        .engine = engine,
        .hash = hash,
        .depth = depth,
        .err = err,
      };
      st_foreach(RHASH_TBL(value), me_value_assoc, (st_data_t)&args);
      if (err->type != ME_VALUE_NO_ERR) {
        return me_value_guest_nil_new();
      }

      return hash;
    }
  default:
    *err = (struct me_value_err){ .type = ME_VALUE_UNSUPPORTED };
    return me_value_guest_nil_new();
  }
}

me_guest_value_t me_value_to_guest(
  struct me_mruby_engine *engine,
  me_host_value_t value,
  struct me_value_err *err)
{
  return me_value_to_guest_r(engine, value, 0, err);
}

me_host_value_t me_value_host_fixnum_new(long value, struct me_value_err *err) {
  if (!FIXABLE(value)) {
    *err = (struct me_value_err){
      .type = ME_VALUE_OUT_OF_RANGE,
    };
    return Qnil;
  }

  return LONG2FIX(value);
}

me_host_value_t me_value_host_string_new(
  const char *bytes,
  size_t size,
  struct me_value_err *err)
{
  (void)err;
  return rb_enc_str_new(bytes, size, rb_utf8_encoding());
}

me_host_value_t me_value_host_symbol_new(
  const char *bytes,
  size_t size,
  struct me_value_err *err)
{
  (void)err;
  return ID2SYM(rb_intern3(bytes, size, rb_utf8_encoding()));
}

me_host_value_t me_value_host_array_new(struct me_value_err *err) {
  (void)err;
  return rb_ary_new();
}

void me_value_host_array_push(
  me_host_value_t array,
  me_host_value_t element,
  struct me_value_err *err)
{
  (void)err;
  rb_ary_push(array, element);
}

me_host_value_t me_value_host_hash_new(struct me_value_err *err) {
  (void)err;
  return rb_hash_new();
}

void me_value_host_hash_assoc(
  me_host_value_t hash,
  me_host_value_t key,
  me_host_value_t value,
  struct me_value_err *err)
{
  (void)err;
  rb_hash_aset(hash, key, value);
}
