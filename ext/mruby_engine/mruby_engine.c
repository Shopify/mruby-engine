#include "mruby_engine_private.h"
#include "platform.h"
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/dump.h>
#include <mruby/error.h>
#include <mruby/hash.h>
#include <mruby/opcode.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <stdlib.h>

static const uint64_t COMPILER_INSTRUCTION_QUOTA = 100000;
static const struct timespec COMPILER_TIME_QUOTA = { 1, 0 };

struct me_iseq {
  size_t size;
  uint8_t *data;
};

static void mruby_engine_signal_memory_quota_reached(struct me_mruby_engine *self, size_t size) {
  struct me_eval_err err = (struct me_eval_err){
    .type = ME_EVAL_MEMORY_QUOTA_REACHED,
    .memory_quota_reached = {
      .size = size,
      .allocation = me_memory_pool_get_allocation(self->allocator),
      .capacity = me_memory_pool_get_capacity(self->allocator),
    },
  };
  me_mruby_engine_eval_leave(self, err);
}

static void mruby_engine_signal_instruction_quota_reached(struct me_mruby_engine *self) {
  struct me_eval_err err = (struct me_eval_err){
    .type = ME_EVAL_INSTRUCTION_QUOTA_REACHED,
    .instruction_quota_reached = {
      .instruction_quota = self->instruction_quota,
    },
  };
  me_mruby_engine_eval_leave(self, err);
}

#ifdef ME_EVAL_MONITORED_P
static void mruby_engine_signal_stack_exhausted(
  struct me_mruby_engine *self)
{
  struct me_eval_err err = (struct me_eval_err){
    .type = ME_EVAL_STACK_EXHAUSTED,
  };
  me_mruby_engine_eval_leave(self, err);
}
#endif

static void *mruby_engine_allocf(struct mrb_state *state, void *block, size_t size, void *data) {
  (void)state;

  struct me_mruby_engine *engine = data;

  if (size == 0 && block != NULL) {
    me_memory_pool_free(engine->allocator, block);
    return NULL;
  }

  if (block == NULL) {
    block = me_memory_pool_malloc(engine->allocator, size);
  } else {
    block = me_memory_pool_realloc(engine->allocator, block, size);
  }

  if (block == NULL) {
    mruby_engine_signal_memory_quota_reached(engine, size);
  }
  return block;
}

bool me_mruby_engine_get_quota_exception_raised(struct me_mruby_engine *self) {
  return self->quota_error_raised;
}

me_host_exception_t me_mruby_engine_get_exception(struct me_mruby_engine *self) {
  if (self->state->exc == NULL)
    return ME_HOST_NIL;

  mrb_value exception = mrb_obj_value(self->state->exc);

  if (mrb_obj_is_kind_of(self->state, exception, mrb_class_get(self->state, "ExitException"))) {
    self->state->exc = NULL;
    return ME_HOST_NIL;
  }

  intptr_t host_backtrace = me_host_backtrace_new();
  mrb_value backtrace = mrb_exc_backtrace(self->state, exception);
  mrb_int backtrace_len = mrb_ary_len(self->state, backtrace);
  for (int i = 0; i < backtrace_len; ++i) {
    mrb_value location = mrb_ary_entry(backtrace, i);
    me_host_backtrace_push_location(host_backtrace, mrb_string_value_cstr(self->state, &location));
  }

  struct RClass *class = mrb_class(self->state, exception);
  mrb_value class_name_obj = mrb_obj_as_string(self->state, mrb_obj_value(class));
  struct RString *class_name = mrb_str_ptr(class_name_obj);
  mrb_value message = mrb_funcall_argv(self->state, exception, self->sym_to_s, 0, NULL);
  me_host_exception_t err = me_host_runtime_error_new(
    RSTR_PTR(class_name),
    RSTR_LEN(class_name),
    mrb_string_value_cstr(self->state, &message),
    host_backtrace);

  self->state->exc = NULL;
  return err;
}

#ifdef ME_EVAL_MONITORED_P
static const ptrdiff_t STACK_MINIMUM = 0x10000;

static void mruby_engine_check_stack(
  struct me_mruby_engine *self)
{
  ptrdiff_t stack_remaining =
    (uint8_t *)&stack_remaining - (uint8_t *)self->eval_state.stack_base;
  if (stack_remaining < STACK_MINIMUM) {
    mruby_engine_signal_stack_exhausted(self);
  }
}
#endif

static void mruby_engine_code_fetch_hook(
  struct mrb_state* mrb,
  struct mrb_irep *irep,
  mrb_code *pc,
  mrb_value *regs)
{
  (void)irep;
  (void)regs;

  struct me_mruby_engine *engine = mrb->allocf_ud;

  if (engine->instruction_count >= engine->instruction_quota) {
    mruby_engine_signal_instruction_quota_reached(engine);
  }

  engine->instruction_count++;

#ifdef ME_EVAL_MONITORED_P
  switch (GET_OPCODE(*pc)) {
  case OP_SEND:
  case OP_SENDB:
  case OP_FSEND:
    {
      mruby_engine_check_stack(engine);
      break;
    }
  }
#else
  (void)pc;
#endif
}

static mrb_value mruby_engine_exit(struct mrb_state *state, mrb_value rvalue) {
  mrb_raise(state, mrb_class_get(state, "ExitException"), "exit exception");
  return rvalue;
}

struct me_mruby_engine *me_mruby_engine_new(
  struct me_memory_pool *allocator,
  uint64_t instruction_quota,
  struct timespec time_quota)
{
  struct me_mruby_engine *self = me_memory_pool_malloc(allocator, sizeof(struct me_mruby_engine));
  self->allocator = allocator;
  self->state = mrb_open_allocf(mruby_engine_allocf, self);

  if (self->state == NULL) {
    me_memory_pool_free(allocator, self);
    return NULL;
  }

  self->sym_to_s = mrb_intern_cstr(self->state, "to_s");

  mrb_define_class(self->state, "ExitException", mrb_class_get(self->state, "Exception"));
  mrb_define_method(self->state , self->state->kernel_module, "exit", mruby_engine_exit, 1);

  self->instruction_quota = instruction_quota;
  self->instruction_count = 0;
  self->quota_error_raised = false;
  self->state->code_fetch_hook = mruby_engine_code_fetch_hook;
  self->time_quota = time_quota;
  self->ctx_switches_v = -1;
  self->ctx_switches_iv = -1;
  self->cpu_time_ns = 0;

  return self;
}

void me_mruby_engine_destroy(struct me_mruby_engine *self) {
  struct me_memory_pool *allocator = me_mruby_engine_get_allocator(self);
  mrb_close(self->state);
  me_memory_pool_free(allocator, self);
}

struct me_memory_pool *me_mruby_engine_get_allocator(struct me_mruby_engine *self) {
  return self->allocator;
}

static struct RProc *generate_code(
  struct mrb_state *state,
  struct mrbc_context *context,
  const char *source,
  me_host_exception_t *err)
{
  struct mrb_parser_state *parser_state = mrb_parser_new(state);
  parser_state->s = source;
  parser_state->send = source + strlen(source);
  mrb_parser_parse(parser_state, context);
  if (parser_state->nerr > 0) {
    *err = me_host_syntax_error_new(
      parser_state->filename,
      parser_state->error_buffer[0].lineno,
      parser_state->error_buffer[0].column,
      parser_state->error_buffer[0].message);
    mrb_parser_free(parser_state);
    return NULL;
  }

  struct RProc *proc = mrb_generate_code(state, parser_state);
  mrb_parser_free(parser_state);
  if (proc == NULL) {
    *err = me_host_internal_error_new("code generation failed");
    return NULL;
  }

  return proc;
}

struct me_proc *me_mruby_engine_generate_code(
  struct me_mruby_engine *self,
  const char *path,
  const char *source,
  me_host_exception_t *err)
{
  struct mrbc_context *context = mrbc_context_new(self->state);
  context->capture_errors = true;
  mrbc_filename(self->state, context, path);

  struct RProc *proc = generate_code(self->state, context, source, err);
  mrbc_context_free(self->state, context);

  return (struct me_proc *)proc;
}

void me_mruby_engine_iseq_load(
  struct me_mruby_engine *self,
  const struct me_iseq *iseq,
  me_host_exception_t *err)
{

  mrb_irep *irep = mrb_read_irep(self->state, iseq->data);

  if(!irep) {
    *err = me_host_internal_error_new("mrb_read_irep returned invalid");
    return;
  }
  struct RProc *proc = mrb_proc_new(self->state, irep);
  me_mruby_engine_eval(self,(struct me_proc *)proc, err);
}

void me_mruby_engine_inject(
  struct me_mruby_engine *self,
  const char *ivar_name,
  me_host_value_t value,
  struct me_value_err *err)
{
  mrb_sym ivar_name_mrb = mrb_intern_cstr(self->state, ivar_name);
  mrb_value value_mrb = (mrb_value){ .w = me_value_to_guest(self, value, err) };
  if (err->type != ME_VALUE_NO_ERR) {
    return;
  }

  mrb_iv_set(self->state, mrb_top_self(self->state), ivar_name_mrb, value_mrb);
}

me_host_value_t me_mruby_engine_extract(
  struct me_mruby_engine *self,
  const char *ivar_name,
  struct me_value_err *err)
{
  if (self == NULL) {
    me_host_raise(me_host_internal_error_new("invalid parameter: self == NULL"));
  }
  if (ivar_name == NULL) {
    me_host_raise(me_host_internal_error_new("invalid parameter: ivar_name == NULL"));
  }
  if (err == NULL) {
    me_host_raise(me_host_internal_error_new("invalid parameter: err == NULL"));
  }

  mrb_sym ivar_name_mrb = mrb_intern_cstr(self->state, ivar_name);
  mrb_value value = mrb_iv_get(self->state, mrb_top_self(self->state), ivar_name_mrb);
  return me_value_to_host(self, value.w, err);
}

uint64_t me_mruby_engine_get_instruction_count(struct me_mruby_engine *self) {
  return self->instruction_count;
}

uint64_t me_mruby_engine_get_memory_count(struct me_mruby_engine *self) {
  return me_memory_pool_get_allocation(self->allocator);
}

int64_t me_mruby_engine_get_ctx_switches_voluntary(struct me_mruby_engine *self) {
  return self->ctx_switches_v;
}

int64_t me_mruby_engine_get_ctx_switches_involuntary(struct me_mruby_engine *self) {
  return self->ctx_switches_iv;
}

uint64_t me_mruby_engine_get_cpu_time(struct me_mruby_engine *self) {
  return self->cpu_time_ns;
}

static int next_source(struct mrb_parser_state *parser_state) {
  struct mrbc_context *context = parser_state->cxt;
  struct me_source *sources = context->partial_data;

  if (sources->source == NULL) {
    return -1;
  }

  parser_state->s = sources->source;
  parser_state->send = sources->source + strlen(sources->source);
  mrb_parser_set_filename(parser_state, sources->path);
  context->partial_data = sources + 1;
  return 0;
}

struct me_iseq *me_iseq_new(
  struct me_source sources[],
  struct me_memory_pool *allocator,
  struct me_iseq_err *err)
{
  struct me_mruby_engine *engine = me_mruby_engine_new(
    allocator,
    COMPILER_INSTRUCTION_QUOTA,
    COMPILER_TIME_QUOTA);

  struct mrbc_context *context = mrbc_context_new(engine->state);
  context->no_exec = TRUE;
  context->capture_errors = true;
  mrbc_partial_hook(engine->state, context, next_source, sources + 1);
  mrbc_filename(engine->state, context, sources->path);

  me_host_exception_t host_err = ME_HOST_NIL;
  struct RProc *proc = generate_code(engine->state, context, sources->source, &host_err);
  mrbc_context_free(engine->state, context);
  if (proc == NULL) {
    me_host_raise(host_err);
  }

  mrb_irep *irep = proc->body.irep;
  uint8_t *irep_data;
  size_t irep_data_size;
  int status = mrb_dump_irep(
    engine->state,
    irep,
    DUMP_DEBUG_INFO | DUMP_ENDIAN_NAT,
    &irep_data,
    &irep_data_size);
  if (status == MRB_DUMP_OK) {
    err->type = ME_ISEQ_NO_ERR;
  } else {
    err->type = ME_ISEQ_GENERAL_FAILURE;
    me_mruby_engine_destroy(engine);
    return NULL;
  }

  struct me_iseq *iseq = me_host_malloc(sizeof(struct me_iseq));
  *iseq = (struct me_iseq){
    .data = me_host_malloc(irep_data_size),
    .size = irep_data_size,
  };
  memcpy(iseq->data, irep_data, irep_data_size);
  me_memory_pool_free(allocator, irep_data);

  me_mruby_engine_destroy(engine);
  return iseq;
}

void me_iseq_destroy(struct me_iseq *iseq) {
  free(iseq->data);
  free(iseq);
}

size_t me_iseq_size(struct me_iseq *iseq) {
  return iseq->size;
}

static const uint32_t HASH_FACTOR = 65599;

uint32_t me_iseq_hash(struct me_iseq *iseq) {
  uint32_t hash = 0;
  for (size_t i = 0; i < iseq->size; ++i) {
    hash = hash * HASH_FACTOR + iseq->data[i];
  }
  return hash;
}

me_host_exception_t me_eval_err_to_host(struct me_eval_err *err) {
  switch (err->type) {
  case ME_EVAL_NO_ERR:
    return ME_HOST_NIL;
  case ME_EVAL_INSTRUCTION_QUOTA_REACHED:
    return me_host_instruction_quota_error_new(err->instruction_quota_reached.instruction_quota);
  case ME_EVAL_MEMORY_QUOTA_REACHED:
    return me_host_memory_quota_error_new(
      err->memory_quota_reached.size,
      err->memory_quota_reached.allocation,
      err->memory_quota_reached.capacity);
  case ME_EVAL_STACK_EXHAUSTED:
    return me_host_stack_exhausted_error_new();
  case ME_EVAL_SYSTEM_ERROR:
    return me_host_internal_error_new_from_err_no(
      err->system_error.err_source,
      err->system_error.err_no);
  default:
    return me_host_internal_error_new("unknown %d", err->type);
  }
}
