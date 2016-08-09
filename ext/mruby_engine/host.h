#ifndef MRUBY_ENGINE_HOST_H
#define MRUBY_ENGINE_HOST_H

#include "value.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

void *me_host_malloc(size_t size);
void me_host_free(void *block);

me_host_backtrace_t me_host_backtrace_new(void);
void me_host_backtrace_push_location(me_host_backtrace_t, const char *);

me_host_exception_t me_host_argument_error_new(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
me_host_exception_t me_host_type_error_new(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
me_host_exception_t me_host_runtime_error_new(
  const char *type,
  size_t type_len,
  const char *message,
  me_host_backtrace_t backtrace);
me_host_exception_t me_host_syntax_error_new(
  const char *path, int line, int column, const char *message);
me_host_exception_t me_host_memory_quota_error_new(
  size_t requested, size_t total, size_t capacity);
me_host_exception_t me_host_instruction_quota_error_new(uint64_t instruction_quota);
me_host_exception_t me_host_quota_already_reached_new(const char *format, ...);
me_host_exception_t me_host_time_quota_error_new(struct timespec time_quota);
me_host_exception_t me_host_stack_exhausted_error_new(void);
me_host_exception_t me_host_internal_error_new(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
me_host_exception_t me_host_internal_error_new_from_err_no(const char *message, int err_no);
void me_host_raise(me_host_exception_t exception)
  __attribute__((noreturn));

void *me_host_invoke_unblocking(void *(*)(void *), void *);

#endif
