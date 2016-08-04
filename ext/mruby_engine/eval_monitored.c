#include "mruby_engine_private.h"

#ifdef ME_EVAL_MONITORED_P

#include "host.h"
#include "platform.h"
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

static void timespec_add(struct timespec *dst, const struct timespec *src) {
  dst->tv_sec += src->tv_sec;
  dst->tv_nsec += src->tv_nsec;
  if (dst->tv_nsec >= 1000000000) {
    dst->tv_nsec -= 1000000000;
    dst->tv_sec += 1;
  }
}

#define ME_PTHREAD_CALL(engine, f, ...)                             \
  ({                                                                \
    int err_no = f(__VA_ARGS__);                                    \
    if (err_no && engine->eval_state.err.type == ME_EVAL_NO_ERR) {  \
      engine->eval_state.err = (struct me_eval_err){                \
        .type = ME_EVAL_SYSTEM_ERROR,                               \
        .system_error = {                                           \
          .err_no = err_no,                                         \
          .err_source = #f,                                         \
        },                                                          \
      };                                                            \
    }                                                               \
    err_no;                                                         \
  })

static void mruby_engine_monitored_eval_cleanup(void *data) {
  struct me_mruby_engine *self = data;

  int oldstate;
  ME_PTHREAD_CALL(self, pthread_setcancelstate, PTHREAD_CANCEL_DISABLE, &oldstate);
  ME_PTHREAD_CALL(self, pthread_mutex_lock, &self->eval_state.mutex);

  self->eval_state.eval_done_p = true;

  if (ME_PTHREAD_CALL(self, pthread_mutex_unlock, &self->eval_state.mutex)) {
    return;
  }

  ME_PTHREAD_CALL(self, pthread_cond_signal, &self->eval_state.cond);
}

static void *mruby_engine_monitored_eval(void *data) {
  struct me_mruby_engine *self = data;

  pthread_cleanup_push(mruby_engine_monitored_eval_cleanup, data);

  if (ME_PTHREAD_CALL(self, me_platform_get_stack_base, &self->eval_state.stack_base)) {
    pthread_exit(NULL);
  };

  int oldtype;
  if (ME_PTHREAD_CALL(self, pthread_setcanceltype, PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype)) {
    pthread_exit(NULL);
  }
  pthread_testcancel();

  mrb_context_run(self->state, &self->eval_state.proc->proc, mrb_top_self(self->state), 0);

  pthread_cleanup_pop(true);
  return NULL;
}

static void mruby_engine_eval_state_init(
  struct me_mruby_engine *self,
  struct me_proc *proc,
  me_host_exception_t *err)
{
  int err_no;

  self->eval_state = (struct me_eval_state){
    .proc = proc,
    .err = { .type = ME_EVAL_NO_ERR },
  };

  pthread_condattr_t eval_condattr;
  if ((err_no = pthread_condattr_init(&eval_condattr))) {
    *err = me_host_internal_error_new_from_err_no("pthread_condattr_init", err_no);
    return;
  }

  if ((err_no = pthread_condattr_setclock(&eval_condattr, CLOCK_MONOTONIC))) {
    *err = me_host_internal_error_new_from_err_no("pthread_condattr_setclock", err_no);
    return;
  }

  if ((err_no = pthread_cond_init(&self->eval_state.cond, &eval_condattr))) {
    *err = me_host_internal_error_new_from_err_no("pthread_cond_init", err_no);
    return;
  }

  if ((err_no = pthread_mutex_init(&self->eval_state.mutex, NULL))) {
    *err = me_host_internal_error_new_from_err_no("pthread_mutex_init", err_no);
    pthread_cond_destroy(&self->eval_state.cond);
    return;
  }
}

static void *mruby_engine_wait_without_gvl(void *data) {
  struct me_mruby_engine *self = data;
  return (void *)(intptr_t)pthread_cond_timedwait(
    &self->eval_state.cond,
    &self->eval_state.mutex,
    &self->eval_state.deadline);
}

void me_mruby_engine_eval(
  struct me_mruby_engine *self,
  struct me_proc *proc,
  me_host_exception_t *err)
{
  int err_no;

  if (self == NULL) {
    me_host_raise(me_host_internal_error_new("invalid parameter: self == NULL"));
  }
  if (proc == NULL) {
    me_host_raise(me_host_internal_error_new("invalid parameter: proc == NULL"));
  }
  if (err == NULL) {
    me_host_raise(me_host_internal_error_new("invalid parameter: err == NULL"));
  }

  mruby_engine_eval_state_init(self, proc, err);
  if (*err != ME_HOST_NIL) {
    return;
  }

  if ((err_no = pthread_mutex_lock(&self->eval_state.mutex))) {
    *err = me_host_internal_error_new_from_err_no("pthread_mutex_lock", err_no);
    goto cleanup;
  }

  pthread_t thread;
  if ((err_no = pthread_create(&thread, NULL, mruby_engine_monitored_eval, self))) {
    *err = me_host_internal_error_new_from_err_no("pthread_create", err_no);
    goto cleanup;
  }

  if (clock_gettime(CLOCK_MONOTONIC, &self->eval_state.deadline)) {
    *err = me_host_internal_error_new_from_err_no("clock_gettime", errno);
    goto cleanup;
  }
  timespec_add(&self->eval_state.deadline, &self->time_quota);

  int wait_result;
  do {
    wait_result = (int)(intptr_t)me_host_invoke_unblocking(mruby_engine_wait_without_gvl, self);
    if (wait_result && wait_result != ETIMEDOUT) {
      *err = me_host_internal_error_new_from_err_no("pthread_cond_timedwait", err_no);
      goto cleanup;
    }
  } while (!wait_result && !self->eval_state.eval_done_p);

  if ((err_no = pthread_mutex_unlock(&self->eval_state.mutex))) {
    *err = me_host_internal_error_new_from_err_no("pthread_mutex_unlock", err_no);
    goto cleanup;
  }

  if (wait_result) {
    if ((err_no = pthread_cancel(thread))) {
      *err = me_host_internal_error_new_from_err_no("pthread_cancel", err_no);
      goto cleanup;
    }
  }

  if ((err_no = pthread_join(thread, NULL))) {
    *err = me_host_internal_error_new_from_err_no("pthread_join", err_no);
    goto cleanup;
  }

  if (wait_result == ETIMEDOUT) {
    *err = me_host_time_quota_error_new(self->time_quota);
    goto cleanup;
  }

cleanup:
  if ((pthread_mutex_destroy(&self->eval_state.mutex)) && *err == ME_HOST_NIL) {
    *err = me_host_internal_error_new_from_err_no("pthread_mutex_destroy", err_no);
  }
  if ((err_no = pthread_cond_destroy(&self->eval_state.cond)) && *err == ME_HOST_NIL) {
    *err = me_host_internal_error_new_from_err_no("pthread_cond_destroy", err_no);
  }
  if (*err == ME_HOST_NIL) {
    *err = me_eval_err_to_host(&self->eval_state.err);
  }
  if (*err == ME_HOST_NIL) {
    *err = me_mruby_engine_get_exception(self);
  }
}

void me_mruby_engine_eval_leave(struct me_mruby_engine *self, struct me_eval_err err) {
  self->eval_state.err = err;
  self->quota_error_raised = true;
  pthread_exit(NULL);
}

#endif
