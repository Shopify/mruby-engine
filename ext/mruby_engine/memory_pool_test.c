#include "host.h"
#include "memory_pool.h"
#include "native_tests.h"
#include <ruby.h>

static VALUE test_trigger_user_error(VALUE self) {
  struct me_memory_pool_err err;
  struct me_memory_pool *allocator = me_memory_pool_new(1 << 22, &err);
  if (allocator == NULL) {
    me_host_raise(me_host_internal_error_new("failed to create memory pool"));
  }

  void *block = me_memory_pool_malloc(allocator, 16);
  me_memory_pool_free(allocator, block);
  me_memory_pool_free(allocator, block);
  me_memory_pool_destroy(allocator);
  return Qnil;
}

void init_memory_pool_tests(void) {
  VALUE m_memory_pool_tests = rb_define_module("MemoryPoolTests");
  rb_define_singleton_method(m_memory_pool_tests, "trigger_user_error!", test_trigger_user_error, 0);
}
