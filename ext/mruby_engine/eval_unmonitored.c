#include "mruby_engine_private.h"

#ifndef ME_EVAL_MONITORED_P

void me_mruby_engine_eval(
  struct me_mruby_engine *self,
  struct me_proc *proc,
  me_host_exception_t *err)
{
  mrb_context_run(self->state, &proc->proc, mrb_top_self(self->state), 0);
  *err = me_mruby_engine_get_exception(self);
}

void me_mruby_engine_eval_leave(struct me_mruby_engine *self, struct me_eval_err err) {
  self->quota_error_raised = true;
  me_host_raise(me_eval_err_to_host(&err));
}

#endif
