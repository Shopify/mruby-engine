#if defined(__APPLE__) && defined(__MACH__)

#include "platform.h"
#include <string.h>

void me_platform_strerror(int err, char *buffer, size_t buffer_len) {
  (void)strerror_r(err, buffer, buffer_len);
}

#endif
