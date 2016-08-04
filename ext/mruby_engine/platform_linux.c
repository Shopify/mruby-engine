#ifdef __linux__

#include "platform.h"
#include "definitions.h"
#include <pthread.h>
#include <string.h>

int me_platform_get_stack_base(void **base) {
  pthread_t thread = pthread_self();

  pthread_attr_t attr;
  int result = pthread_getattr_np(thread, &attr);
  if (result) {
    return result;
  }

  size_t size;
  return pthread_attr_getstack(&attr, base, &size);
}

void me_platform_strerror(int err, char *buffer, size_t buffer_len) {
  char *result = strerror_r(err, buffer, buffer_len);
  if (result != buffer) {
    strncpy(buffer, result, buffer_len);
    buffer[buffer_len - 1] = '\0';
  }
}

#endif
