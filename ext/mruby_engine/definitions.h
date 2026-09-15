#ifndef MRUBY_ENGINE_DEFINITIONS_H
#define MRUBY_ENGINE_DEFINITIONS_H

#ifdef __linux__
#define ME_EVAL_MONITORED_P
#endif

#define KiB 1024
#define MiB (1024 * KiB)

#if defined(__SANITIZE_ADDRESS__) // gcc
#  define ME_ADDRESS_SANITIZER 1
#elif defined(__has_feature) // clang
#  define ME_ADDRESS_SANITIZER __has_feature(address_sanitizer)
#else
#  define ME_ADDRESS_SANITIZER 0
#endif

#endif
