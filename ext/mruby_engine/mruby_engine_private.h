#ifndef MRUBY_ENGINE_EVAL_H
#define MRUBY_ENGINE_EVAL_H

#include "mruby_engine.h"
#include "definitions.h"
#include "host.h"
#include <mruby/proc.h>
#include <stdbool.h>

struct me_proc {
  struct RProc proc;
};

#ifdef ME_EVAL_MONITORED_P
#include <pthread.h>
struct me_eval_state {
  struct me_proc *proc;
  struct me_eval_err err;
  struct timespec deadline;
  volatile bool eval_done_p;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  void *stack_base;
};
#endif

struct me_mruby_engine {
  struct mrb_state *state;
  struct me_memory_pool *allocator;

#ifdef ME_EVAL_MONITORED_P
  struct me_eval_state eval_state;
#endif

  uint64_t instruction_count;
  uint64_t instruction_quota;
  bool quota_error_raised;
  struct timespec time_quota;
  int64_t ctx_switches_v;
  int64_t ctx_switches_iv;
  int64_t cpu_time_ns;

  mrb_sym sym_to_s;
};

me_host_exception_t me_eval_err_to_host(struct me_eval_err *err);

me_host_exception_t me_mruby_engine_get_exception(struct me_mruby_engine *self);
void me_mruby_engine_eval_leave(
  struct me_mruby_engine *self,
  struct me_eval_err err)
  __attribute__((noreturn));

#endif
