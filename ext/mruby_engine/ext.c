#include "ext.h"
#include "host.h"
#include "memory_pool.h"
#include "mruby_engine.h"
#include "platform.h"
#include <ruby.h>
#include <inttypes.h>
#include <stdarg.h>
#include "native_tests.h"

ID me_ext_id_guest_backtrace_eq;
ID me_ext_id_generate;
ID me_ext_id_instructions;
ID me_ext_id_memory;
ID me_ext_id_mul;
VALUE me_ext_m_json;
VALUE me_ext_c_mruby_engine;
VALUE me_ext_c_iseq;
VALUE me_ext_e_engine_error;
VALUE me_ext_e_engine_runtime_error;
VALUE me_ext_e_engine_type_error;
VALUE me_ext_e_engine_syntax_error;
VALUE me_ext_e_engine_quota_error;
VALUE me_ext_e_engine_memory_quota_error;
VALUE me_ext_e_engine_instruction_quota_error;
VALUE me_ext_e_engine_time_quota_error;
VALUE me_ext_e_engine_stack_exhausted_error;
VALUE me_ext_e_engine_internal_error;
VALUE me_ext_e_engine_quota_already_reached;

static void ext_mruby_engine_check_initialized(struct me_mruby_engine *self, const char *caller) {
  if (self)
    return;

  rb_raise(rb_eArgError, "uninitialized value when calling '%s'", caller);
}


static void ext_mruby_engine_free(struct me_mruby_engine *engine) {
  if (!engine)
    return;

  struct me_memory_pool *allocator = me_mruby_engine_get_allocator(engine);
  me_mruby_engine_destroy(engine);
  me_memory_pool_destroy(allocator);
}

static void ext_iseq_free(struct me_iseq *iseq) {
  if (!iseq) {
    return;
  }

  me_iseq_destroy(iseq);
}

static VALUE ext_mruby_engine_alloc(VALUE class) {
  return Data_Wrap_Struct(class, NULL, ext_mruby_engine_free, NULL);
}

static VALUE ext_iseq_alloc(VALUE class) {
  return Data_Wrap_Struct(class, NULL, ext_iseq_free, NULL);
}

static void check_quota_error_raised(struct me_mruby_engine  *self) {

  if (me_mruby_engine_get_quota_exception_raised(self)) {
      me_host_raise(me_host_quota_already_reached_new("quota error already reached, operation aborted"));
  }
}

static void check_memory_pool_err(struct me_memory_pool_err *err) {
  switch (err->type) {
  case ME_MEMORY_POOL_NO_ERR:
    return;
  case ME_MEMORY_POOL_INVALID_CAPACITY:
    {
      me_host_exception_t exception = me_host_argument_error_new(
        "memory pool must be between %"PRIuPTR"KiB and %"PRIuPTR"KiB "
        "(requested %"PRIuPTR"B rounded to %"PRIuPTR"KiB)",
        err->data.invalid_capacity.min >> 10,
        err->data.invalid_capacity.max >> 10,
        err->data.invalid_capacity.capacity,
        err->data.invalid_capacity.rounded_capacity >> 10);
      me_host_raise(exception);
    }
  case ME_MEMORY_POOL_SYSTEM_ERR:
    {
      char message[1024];
      me_platform_strerror(err->data.system_err.err_no, message, sizeof(message));
      me_host_exception_t exception = me_host_internal_error_new(
        "failed to allocate memory pool of %"PRIuPTR"KiB (rounded from %"PRIuPTR"B): %s",
        err->data.system_err.rounded_capacity >> 10,
        err->data.system_err.capacity,
        message);
      me_host_raise(exception);
    }
  default:
    {
      me_host_exception_t exception = me_host_internal_error_new(
        "unknown error: %d",
        err->type);
      me_host_raise(exception);
    }
  }
}

static void check_iseq_dump_err(struct me_iseq_err *err) {
  switch (err->type) {
  case ME_ISEQ_NO_ERR:
    return;
  case ME_ISEQ_GENERAL_FAILURE:
    {
      me_host_exception_t exception = me_host_internal_error_new(
        "failed to save instruction sequence");
      me_host_raise(exception);
    }
  }
}

static VALUE ext_mruby_engine_initialize(int argc, VALUE *argv, VALUE rself) {
  ext_mruby_engine_free(DATA_PTR(rself));

  VALUE rcapacity;
  VALUE r_instruction_quota;
  VALUE r_time_quota_s;
  rb_scan_args(argc, argv, "3", &rcapacity, &r_instruction_quota, &r_time_quota_s);

  long capacity = NUM2LONG(rcapacity);
  if (capacity <= 0) {
    rb_raise(rb_eArgError, "memory quota cannot be negative");
  }

  long instruction_quota = NUM2LONG(r_instruction_quota);
  if (instruction_quota <= 0) {
    rb_raise(rb_eArgError, "instruction quota cannot be negative");
  }

  VALUE r_time_quota_ms = rb_funcall(r_time_quota_s, me_ext_id_mul, 1, LONG2FIX(1000));
  long time_quota_ms = NUM2LONG(r_time_quota_ms);
  if (time_quota_ms <= 0) {
    rb_raise(rb_eArgError, "time quota cannot be negative");
  }

  struct me_memory_pool_err err = { 0 };
  struct me_memory_pool *allocator = me_memory_pool_new(capacity, &err);
  check_memory_pool_err(&err);

  struct timespec time_quota = (struct timespec){
    .tv_sec = time_quota_ms / 1000,
    .tv_nsec = time_quota_ms % 1000 * 1000000,
  };

  struct me_mruby_engine *engine = me_mruby_engine_new(
    allocator,
    instruction_quota,
    time_quota);
  if (engine == NULL) {
    me_memory_pool_destroy(allocator);
    me_host_exception_t exception = me_host_internal_error_new("failed to initialize mruby");
    me_host_raise(exception);
  }

  DATA_PTR(rself) = engine;
  return Qnil;
}

static inline struct me_mruby_engine *ext_mruby_engine_unwrap(VALUE rengine) {
  struct me_mruby_engine *engine;
  Data_Get_Struct(rengine, struct me_mruby_engine, engine);
  return engine;
}

static inline struct me_iseq *ext_iseq_unwrap(VALUE riseq) {
  struct me_iseq *iseq;
  Data_Get_Struct(riseq, struct me_iseq, iseq);
  return iseq;
}

static VALUE ext_mruby_engine_eval(VALUE rself, VALUE rpath, VALUE rsource) {
  struct me_mruby_engine *self = ext_mruby_engine_unwrap(rself);
  ext_mruby_engine_check_initialized(self, "sandbox_eval");

  check_quota_error_raised(self);

  me_host_exception_t err = ME_HOST_NIL;
  struct me_proc *proc = me_mruby_engine_generate_code(
    self, StringValueCStr(rpath), StringValueCStr(rsource), &err);
  if (err != ME_HOST_NIL) {
    me_host_raise(err);
  }

  me_mruby_engine_eval(self, proc, &err);
  if (err != ME_HOST_NIL) {
    me_host_raise(err);
  }

  return rself;
}

static VALUE ext_mruby_engine_load(VALUE rself, VALUE riseq) {

  struct me_mruby_engine *self = ext_mruby_engine_unwrap(rself);
  struct me_iseq *iseq = ext_iseq_unwrap(riseq);

  check_quota_error_raised(self);

  me_host_exception_t err = ME_HOST_NIL;
  me_mruby_engine_iseq_load(self, iseq, &err);

  if (err != ME_HOST_NIL) {
    me_host_raise(err);
  }
  return rself;
}

static void ext_mruby_engine_check_value_err(struct me_value_err *err) {
  switch (err->type) {
  case ME_VALUE_NO_ERR:
    return;
  case ME_VALUE_UNSUPPORTED:
    rb_raise(
      me_ext_e_engine_type_error,
      "can only extract strings, fixnums, symbols, arrays or hashes");
  case ME_VALUE_OUT_OF_RANGE:
    rb_raise(
      me_ext_e_engine_type_error,
      "can't extract value out of bounds");
  case ME_VALUE_TOO_DEEP:
    rb_raise(
      me_ext_e_engine_type_error,
      "structure nested too deeply");
  case ME_VALUE_GUEST_ERR:
    rb_exc_raise(err->guest_err.err);
  default:
    rb_raise(me_ext_e_engine_internal_error, "unknown");
  }
}

static VALUE ext_mruby_engine_inject(VALUE rself, VALUE r_ivar_name, VALUE rvalue) {
  struct me_mruby_engine *self = ext_mruby_engine_unwrap(rself);
  ext_mruby_engine_check_initialized(self, "inject");
  check_quota_error_raised(self);

  struct me_value_err err = (struct me_value_err){ .type = ME_VALUE_NO_ERR };
  me_mruby_engine_inject(self, StringValueCStr(r_ivar_name), rvalue, &err);
  ext_mruby_engine_check_value_err(&err);

  return rself;
}

static VALUE ext_mruby_engine_extract(VALUE rself, VALUE r_ivar_name) {
  struct me_mruby_engine *self = ext_mruby_engine_unwrap(rself);
  ext_mruby_engine_check_initialized(self, "extract");
  check_quota_error_raised(self);

  struct me_value_err err = (struct me_value_err){ .type = ME_VALUE_NO_ERR };
  VALUE result = me_mruby_engine_extract(self, StringValueCStr(r_ivar_name), &err);
  ext_mruby_engine_check_value_err(&err);

  return result;
}

static VALUE ext_mruby_engine_stat(VALUE rself) {
  struct me_mruby_engine *self = ext_mruby_engine_unwrap(rself);
  ext_mruby_engine_check_initialized(self, "stat");

  VALUE stat = rb_hash_new();

  uint64_t instruction_count = me_mruby_engine_get_instruction_count(self);
  rb_hash_aset(stat, ID2SYM(me_ext_id_instructions), ULONG2NUM(instruction_count));

  uint64_t memory_count = me_mruby_engine_get_memory_count(self);
  rb_hash_aset(stat, ID2SYM(me_ext_id_memory), ULONG2NUM(memory_count));

  return stat;
}

static const size_t COMPILE_MEMORY_CAPACITY = 4 * MiB;

static struct me_source value_to_source(VALUE r_source_pair) {
  Check_Type(r_source_pair, T_ARRAY);
  VALUE rpath = RARRAY_AREF(r_source_pair, 0);
  VALUE rsource = RARRAY_AREF(r_source_pair, 1);
  return (struct me_source){
    .path = StringValueCStr(rpath),
    .source = StringValueCStr(rsource),
  };
}

static VALUE ext_iseq_initialize(VALUE rself, VALUE rsources) {
  Check_Type(rsources, T_ARRAY);
  long source_count = RARRAY_LEN(rsources);
  if (source_count <= 0) {
    rb_raise(rb_eArgError, "can't create empty instruction sequence");
  }

  struct me_source sources[source_count + 1];
  for (long i = 0; i < source_count; ++i) {
    sources[i] = value_to_source(RARRAY_AREF(rsources, i));
  }
  sources[source_count] = (struct me_source){0};

  struct me_memory_pool_err pool_err;
  struct me_memory_pool *allocator = me_memory_pool_new(COMPILE_MEMORY_CAPACITY, &pool_err);
  check_memory_pool_err(&pool_err);

  struct me_iseq_err err;
  struct me_iseq *iseq = me_iseq_new(sources, allocator, &err);
  me_memory_pool_destroy(allocator);
  check_iseq_dump_err(&err);

  DATA_PTR(rself) = iseq;
  return Qnil;
}

static VALUE ext_iseq_size(VALUE rself) {
  struct me_iseq *iseq = ext_iseq_unwrap(rself);

  return ULONG2NUM(me_iseq_size(iseq));
}

static VALUE ext_iseq_hash(VALUE rself) {
  struct me_iseq *iseq = ext_iseq_unwrap(rself);

  return ULONG2NUM((unsigned long)me_iseq_hash(iseq));
}

__attribute__((visibility("default")))
void Init_mruby_engine(void) {
  rb_require("json");

  me_ext_id_guest_backtrace_eq = rb_intern("guest_backtrace=");
  me_ext_id_generate = rb_intern("generate");
  me_ext_id_instructions = rb_intern("instructions");
  me_ext_id_memory = rb_intern("memory");
  me_ext_id_mul = rb_intern("*");

  me_ext_m_json = rb_path2class("JSON");

  me_ext_c_mruby_engine = rb_define_class("MRubyEngine", rb_cObject);
  rb_define_alloc_func(me_ext_c_mruby_engine, ext_mruby_engine_alloc);
  rb_define_method(me_ext_c_mruby_engine, "initialize", ext_mruby_engine_initialize, -1);
  rb_define_method(me_ext_c_mruby_engine, "sandbox_eval", ext_mruby_engine_eval, 2);
  rb_define_method(me_ext_c_mruby_engine, "load_instruction_sequence", ext_mruby_engine_load, 1);
  rb_define_method(me_ext_c_mruby_engine, "inject", ext_mruby_engine_inject, 2);
  rb_define_method(me_ext_c_mruby_engine, "extract", ext_mruby_engine_extract, 1);
  rb_define_method(me_ext_c_mruby_engine, "stat", ext_mruby_engine_stat, 0);

  me_ext_c_iseq = rb_define_class_under(
    me_ext_c_mruby_engine,
    "InstructionSequence",
    rb_cObject);
  rb_define_alloc_func(me_ext_c_iseq, ext_iseq_alloc);
  rb_define_method(me_ext_c_iseq, "initialize", ext_iseq_initialize, 1);
  rb_define_method(me_ext_c_iseq, "size", ext_iseq_size, 0);
  rb_define_private_method(me_ext_c_iseq, "compute_hash", ext_iseq_hash, 0);

  me_ext_e_engine_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineError", rb_eStandardError);
  me_ext_e_engine_runtime_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineRuntimeError", me_ext_e_engine_error);
  me_ext_e_engine_type_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineTypeError", me_ext_e_engine_error);
  me_ext_e_engine_syntax_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineSyntaxError", me_ext_e_engine_error);
  me_ext_e_engine_quota_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineQuotaError", me_ext_e_engine_error);
  me_ext_e_engine_memory_quota_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineMemoryQuotaError", me_ext_e_engine_quota_error);
  me_ext_e_engine_instruction_quota_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineInstructionQuotaError", me_ext_e_engine_quota_error);
  me_ext_e_engine_time_quota_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineTimeQuotaError", me_ext_e_engine_quota_error);
  me_ext_e_engine_stack_exhausted_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineStackExhaustedError", me_ext_e_engine_quota_error);
  me_ext_e_engine_internal_error = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineInternalError", me_ext_e_engine_error);
  me_ext_e_engine_quota_already_reached = rb_define_class_under(
    me_ext_c_mruby_engine, "EngineQuotaAlreadyReached", me_ext_e_engine_quota_error);

  init_memory_pool_tests();
  init_raise_internal_error_test();
}
