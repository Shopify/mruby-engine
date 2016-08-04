#ifndef MRUBY_ENGINE_PLATFORM_H
#define MRUBY_ENGINE_PLATFORM_H

#include <stddef.h>

void me_platform_strerror(int err, char *buffer, size_t buffer_len);
int me_platform_get_stack_base(void **base);

#endif
