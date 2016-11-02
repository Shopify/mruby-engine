#ifndef MRUBY_ENGINE_DLMALLOC_CONFIG_H
#define MRUBY_ENGINE_DLMALLOC_CONFIG_H

#define ONLY_MSPACES 1
#define HAVE_MREMAP 0
//#define FOOTERS 1

#include "host.h"

#define CORRUPTION_ERROR_ACTION(state)                                  \
  do {                                                                  \
    me_host_exception_t exception = me_host_internal_error_new(         \
      "memory corruption (state: %p)",                                  \
      state);                                                           \
    me_host_raise(exception);                                           \
  } while(0)

#define USAGE_ERROR_ACTION(state, chunk)                                \
  do {                                                                  \
    me_host_exception_t exception = me_host_internal_error_new(         \
      "user memory error (state: %p, chunk: %p)",                       \
      state,                                                            \
      chunk);                                                           \
    me_host_raise(exception);                                           \
  } while(0)

#endif
