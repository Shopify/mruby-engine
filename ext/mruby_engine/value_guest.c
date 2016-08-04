#include "mruby_engine_private.h"
#include "value.h"
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/dump.h>
#include <mruby/error.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/variable.h>

static me_host_value_t me_value_to_host_r(
    struct me_mruby_engine *self,
    mrb_value value,
    int depth,
    struct me_value_err *err)
{
  if (depth > ME_HOST_DATA_DEPTH_MAX) {
    *err = (struct me_value_err){ .type = ME_VALUE_TOO_DEEP };
    return ME_HOST_NIL;
  }

  if (mrb_nil_p(value)) {
    return ME_HOST_NIL;
  }

  enum mrb_vtype type = mrb_type(value);
  switch (type) {
  case MRB_TT_FALSE:
    return ME_HOST_FALSE;
  case MRB_TT_TRUE:
    return ME_HOST_TRUE;
  case MRB_TT_FIXNUM:
    return me_value_host_fixnum_new(mrb_fixnum(value), err);
  case MRB_TT_STRING:
    return me_value_host_string_new(RSTRING_PTR(value), RSTRING_LEN(value), err);
  case MRB_TT_SYMBOL:
    {
      mrb_value s = mrb_check_convert_type(
        self->state, value, MRB_TT_STRING, "String", "to_s");
      return me_value_host_symbol_new(RSTRING_PTR(s), RSTRING_LEN(s), err);
    }
  case MRB_TT_ARRAY:
    {
      me_host_value_t array = me_value_host_array_new(err);
      if (err->type != ME_VALUE_NO_ERR) {
        return ME_HOST_NIL;
      }

      for (mrb_int i = 0, f = mrb_ary_len(self->state, value); i < f; ++i) {
        me_host_value_t element = me_value_to_host_r(
          self, mrb_ary_ref(self->state, value, i), depth + 1, err);
        if (err->type != ME_VALUE_NO_ERR) {
          return ME_HOST_NIL;
        }

        me_value_host_array_push(array, element, err);
        if (err->type != ME_VALUE_NO_ERR) {
          return ME_HOST_NIL;
        }
      }
      return array;
    }
  case MRB_TT_HASH:
    {
      me_host_value_t hash = me_value_host_hash_new(err);
      if (err->type != ME_VALUE_NO_ERR) {
        return ME_HOST_NIL;
      }

      struct kh_ht *kh = mrb_hash_tbl(self->state, value);
      for (int i = kh_begin(kh), f = kh_end(kh); i < f; ++i) {
        if (!kh_exist(kh, i)) {
          continue;
        }

        me_host_value_t key = me_value_to_host_r(
          self, kh_key(kh, i), depth + 1, err);
        if (err->type != ME_VALUE_NO_ERR) {
          return ME_HOST_NIL;
        }

        me_host_value_t element = me_value_to_host_r(
          self, kh_value(kh, i).v, depth + 1, err);
        if (err->type != ME_VALUE_NO_ERR) {
          return ME_HOST_NIL;
        }

        me_value_host_hash_assoc(hash, key, element, err);
        if (err->type != ME_VALUE_NO_ERR) {
          return ME_HOST_NIL;
        }
      }

      return hash;
    }
  default:
    {
      *err = (struct me_value_err){ .type = ME_VALUE_UNSUPPORTED };
      return ME_HOST_NIL;
    }
  }
}

me_host_value_t me_value_to_host(
  struct me_mruby_engine *self,
  me_guest_value_t value,
  struct me_value_err *err)
{
  return me_value_to_host_r(self, (mrb_value){ .w = value }, 0, err);
}

me_guest_value_t me_value_guest_nil_new(void) {
  return mrb_nil_value().w;
}

me_guest_value_t me_value_guest_false_new(void) {
  return mrb_false_value().w;
}
me_guest_value_t me_value_guest_true_new(void) {
  return mrb_true_value().w;
}

static void me_value_guest_check_exception(
  struct me_mruby_engine *engine,
  struct me_value_err *err)
{
  me_host_exception_t guest_err = me_mruby_engine_get_exception(engine);
  if (guest_err == ME_HOST_NIL) {
    return;
  }

  *err = (struct me_value_err){
    .type = ME_VALUE_GUEST_ERR,
    .guest_err = { .err = guest_err },
  };
}

me_guest_value_t me_value_guest_fixnum_new(
  struct me_mruby_engine *engine,
  long value,
  struct me_value_err *err)
{
  mrb_value v = mrb_fixnum_value(value);
  me_value_guest_check_exception(engine, err);
  return v.w;
}

me_guest_value_t me_value_guest_string_new(
  struct me_mruby_engine *engine,
  const char *bytes,
  size_t size,
  struct me_value_err *err)
{
  mrb_value v = mrb_str_new(engine->state, bytes, size);
  me_value_guest_check_exception(engine, err);
  return v.w;
}

me_guest_value_t me_value_guest_symbol_new(
  struct me_mruby_engine *engine,
  const char *bytes,
  size_t size,
  struct me_value_err *err)
{
  mrb_sym v = mrb_intern(engine->state, bytes, size);
  me_value_guest_check_exception(engine, err);
  return mrb_symbol_value(v).w;
}

me_guest_value_t me_value_guest_array_new(
  struct me_mruby_engine *engine,
  struct me_value_err *err)
{
  mrb_value v = mrb_ary_new(engine->state);
  me_value_guest_check_exception(engine, err);
  return v.w;
}

void me_value_guest_array_push(
  struct me_mruby_engine *engine,
  me_guest_value_t array,
  me_guest_value_t element,
  struct me_value_err *err)
{
  mrb_ary_push(engine->state, (mrb_value){ .w = array }, (mrb_value){ .w = element });
  me_value_guest_check_exception(engine, err);
}

me_guest_value_t me_value_guest_hash_new(
  struct me_mruby_engine *engine,
  struct me_value_err *err)
{
  mrb_value v = mrb_hash_new(engine->state);
  me_value_guest_check_exception(engine, err);
  return v.w;
}

void me_value_guest_hash_assoc(
  struct me_mruby_engine *engine,
  me_guest_value_t hash,
  me_guest_value_t key,
  me_guest_value_t value,
  struct me_value_err *err)
{
  mrb_hash_set(
    engine->state,
    (mrb_value){ .w = hash },
    (mrb_value){ .w = key },
    (mrb_value){ .w = value });
  me_value_guest_check_exception(engine, err);
}
