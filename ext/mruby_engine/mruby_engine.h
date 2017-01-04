#ifndef MRUBY_ENGINE_MRUBY_ENGINE_H
#define MRUBY_ENGINE_MRUBY_ENGINE_H

#include "definitions.h"
#include "host.h"
#include "memory_pool.h"
#include <time.h>
#include <stdbool.h>
#include <stdint.h>

struct me_mruby_engine;
struct me_proc;
struct me_iseq;

struct me_source {
  const char *path;
  const char *source;
};

enum me_iseq_err_type {
  ME_ISEQ_NO_ERR,
  ME_ISEQ_GENERAL_FAILURE,
};

struct me_iseq_err {
  enum me_iseq_err_type type;
};

enum me_eval_err_type {
  ME_EVAL_NO_ERR,
  ME_EVAL_INSTRUCTION_QUOTA_REACHED,
  ME_EVAL_MEMORY_QUOTA_REACHED,
  ME_EVAL_STACK_EXHAUSTED,
  ME_EVAL_SYSTEM_ERROR,
};

struct me_eval_err {
  enum me_eval_err_type type;
  union {
    struct {
      uint64_t instruction_quota;
    } instruction_quota_reached;
    struct {
      uint64_t size;
      uint64_t allocation;
      uint64_t capacity;
    } memory_quota_reached;
    struct {
      int err_no;
      const char *err_source;
    } system_error;
  };
};

struct me_mruby_engine *me_mruby_engine_new(
  struct me_memory_pool *allocator,
  uint64_t instruction_limit,
  struct timespec time_quota);
void me_mruby_engine_destroy(struct me_mruby_engine *self);

struct me_memory_pool *me_mruby_engine_get_allocator(struct me_mruby_engine *self);
uint64_t me_mruby_engine_get_instruction_count(struct me_mruby_engine *self);
struct meminfo me_mruby_engine_get_memory_info(struct me_mruby_engine *self);
int64_t me_mruby_engine_get_ctx_switches_voluntary(struct me_mruby_engine *self);
int64_t me_mruby_engine_get_ctx_switches_involuntary(struct me_mruby_engine *self);
int64_t me_mruby_engine_get_cpu_time(struct me_mruby_engine *self);
bool me_mruby_engine_get_quota_exception_raised(struct me_mruby_engine *self);
struct me_proc *me_mruby_engine_generate_code(
  struct me_mruby_engine *self,
  const char *path,
  const char *source,
  me_host_exception_t *err);
void me_mruby_engine_eval(
  struct me_mruby_engine *self,
  struct me_proc *proc,
  me_host_exception_t *err);
void me_mruby_engine_iseq_load(struct me_mruby_engine *self, const struct me_iseq *iseq, me_host_exception_t *err);
void me_mruby_engine_inject(
  struct me_mruby_engine *self,
  const char *ivar_name,
  me_host_value_t value,
  struct me_value_err *err);
me_host_value_t me_mruby_engine_extract(
  struct me_mruby_engine *self,
  const char *ivar_name,
  struct me_value_err *err);

struct me_iseq *me_iseq_new(
  struct me_source sources[],
  struct me_memory_pool *allocator,
  struct me_iseq_err *err);
void me_iseq_destroy(struct me_iseq *);
size_t me_iseq_size(struct me_iseq *);
void *me_iseq_data(struct me_iseq *);
uint32_t me_iseq_hash(struct me_iseq *);

#endif
