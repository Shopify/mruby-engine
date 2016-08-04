#include "host.h"
#include "native_tests.h"
#include <ruby.h>

static VALUE l_mruby_engine_raise_error(VALUE rself) {
  me_host_exception_t exception = me_host_internal_error_new("test internal error");
  me_host_raise(exception);
  return rself;
}

void init_raise_internal_error_test(void) {
  VALUE l_libunwind_tests = rb_define_module("LibunwindTest");
  rb_define_singleton_method(l_libunwind_tests, "raise_test_internal_error", l_mruby_engine_raise_error, 0);
}
