#include "host.h"
#include "ext.h"
#include "platform.h"
#include <libunwind.h>
#include <ruby.h>
#include <ruby/thread.h>
#include <string.h>

const intptr_t ME_HOST_NIL = Qnil;
const intptr_t ME_HOST_FALSE = Qfalse;
const intptr_t ME_HOST_TRUE = Qtrue;
const int LINE_LENGTH = 80;
const int LINE_QUANTITY = 30;

void *me_host_malloc(size_t size) {
  return ruby_xmalloc(size);
}

void me_host_free(void *block) {
  ruby_xfree(block);
}

me_host_backtrace_t me_host_backtrace_new(void) {
  return rb_ary_new();
}

void me_host_backtrace_push_location(intptr_t backtrace, const char *location) {
  rb_ary_push(backtrace, rb_str_new_cstr(location));
}

me_host_exception_t me_host_argument_error_new(const char *format, ...) {
  va_list args;
  va_start(args, format);
  VALUE message = rb_vsprintf(format, args);
  va_end(args);

  return rb_exc_new_str(rb_eArgError, message);
}

me_host_exception_t me_host_quota_already_reached_new(const char *format, ...) {
  va_list args;
  va_start(args, format);
  VALUE message = rb_vsprintf(format, args);
  va_end(args);

  return rb_exc_new_str(me_ext_e_engine_quota_already_reached, message);
}

me_host_exception_t me_host_runtime_error_new(
  const char *type,
  size_t type_len,
  const char *message,
  me_host_backtrace_t backtrace)
{
  VALUE exception = rb_exc_new_cstr(me_ext_e_engine_runtime_error, message);
  VALUE rtype = rb_utf8_str_new(type, type_len);
  rb_funcall(exception, me_ext_id_type_eq, 1, rtype);
  rb_funcall(exception, me_ext_id_guest_backtrace_eq, 1, backtrace);
  return exception;
}

me_host_exception_t me_host_syntax_error_new(
  const char *path,
  int line_number,
  int column,
  const char *message)
{
  VALUE rmessage = rb_sprintf(
    "%s:%d:%d: %s",
    path,
    line_number,
    column,
    message);
  return rb_exc_new_str(me_ext_e_engine_syntax_error, rmessage);
}

me_host_exception_t me_host_memory_quota_error_new(
  size_t requested,
  size_t total,
  size_t capacity)
{
  VALUE rmessage = rb_sprintf(
    "failed to allocate %"PRIuPTR" bytes "
    "(%"PRIuPTR" bytes out of %"PRIuPTR" in use)",
    requested,
    total,
    capacity);
  return rb_exc_new_str(me_ext_e_engine_memory_quota_error, rmessage);
}

me_host_exception_t me_host_instruction_quota_error_new(
  uint64_t instruction_count)
{
  VALUE rmessage = rb_sprintf(
    "exceeded quota of %"PRIu64" instructions.",
    instruction_count);

  return rb_exc_new_str(me_ext_e_engine_instruction_quota_error, rmessage);
}

me_host_exception_t me_host_time_quota_error_new(struct timespec time_quota) {
  VALUE rmessage = rb_sprintf(
    "exceeded quota of %ld ms.",
    (long)(time_quota.tv_sec * 1000 + time_quota.tv_nsec / 1000000));

  return rb_exc_new_str(me_ext_e_engine_time_quota_error, rmessage);
}

me_host_exception_t me_host_stack_exhausted_error_new(void)
{
  VALUE rmessage = rb_utf8_str_new_cstr("stack exhausted");
  return rb_exc_new_str(me_ext_e_engine_stack_exhausted_error, rmessage);
}

void me_host_unwind(char * unwind_buffer ,size_t buffer_len) {
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp, offset;

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);

  sprintf(unwind_buffer,"\n");

  char *proc_buffer = ALLOC_N(char, LINE_LENGTH);
  char *name_buffer = ALLOC_N(char, LINE_LENGTH);

  size_t str_len_total = 2;
  while (unw_step(&cursor) > 0) {

    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    unw_get_proc_name(&cursor, proc_buffer, LINE_LENGTH, &offset);

    snprintf(
      name_buffer,
      LINE_LENGTH,
      "%p : (%s+0x%"PRIu64") [%p]\n",
      (void *)ip,
      proc_buffer,
      (uint64_t)offset,
      (void *)ip);

    str_len_total += strlen(name_buffer);

    if ((str_len_total) > buffer_len) {
      break;
    }

    strcat(unwind_buffer, name_buffer);

  };
  xfree(proc_buffer);
  xfree(name_buffer);
}

me_host_exception_t me_host_internal_error_new(const char *format, ...) {
  va_list args;
  va_start(args, format);
  VALUE message = rb_vsprintf(format, args);
  va_end(args);

  char *unwind_str = ALLOC_N(char, LINE_QUANTITY * LINE_LENGTH);
  me_host_unwind(unwind_str, LINE_QUANTITY * LINE_LENGTH);
  VALUE unwind_message = rb_sprintf("%"PRIsVALUE"  %s",message, unwind_str);
  xfree(unwind_str);

  return rb_exc_new_str(me_ext_e_engine_internal_error, unwind_message);
}

me_host_exception_t me_host_internal_error_new_from_err_no(const char *message, int err_no) {
  char err_message[1024];
  me_platform_strerror(err_no, err_message, sizeof(err_message));
  return me_host_internal_error_new("%s: %s", message, err_message);
}

void me_host_raise(me_host_exception_t exception) {
  rb_exc_raise(exception);
}

void *me_host_invoke_unblocking(void *(*f)(void *), void *data) {
  return rb_thread_call_without_gvl(f, data, RUBY_UBF_IO, NULL);
}
