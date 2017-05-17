#ifndef MRUBY_ENGINE_MEMORY_POOL_H
#define MRUBY_ENGINE_MEMORY_POOL_H

#include <stddef.h>

enum me_memory_pool_err_type {
  ME_MEMORY_POOL_NO_ERR = 0,
  ME_MEMORY_POOL_INVALID_CAPACITY,
  ME_MEMORY_POOL_SYSTEM_ERR,
};

struct me_memory_pool_err {
  enum me_memory_pool_err_type type;
  union {
    struct {
      size_t min;
      size_t max;
      size_t capacity;
      size_t rounded_capacity;
    } invalid_capacity;
    struct {
      int err_no;
      size_t capacity;
      size_t rounded_capacity;
    } system_err;
  } data;
};

struct meminfo {
  size_t arena;
  size_t hblkhd;   /* space in mmapped regions */
  size_t uordblks; /* total allocated space */
  size_t fordblks; /* total free space */
  size_t malloc_size;
  unsigned long malloc_count;
};

struct me_memory_pool;

struct me_memory_pool *me_memory_pool_new(size_t capacity, struct me_memory_pool_err *err);
void me_memory_pool_destroy(struct me_memory_pool *self);

struct meminfo me_memory_pool_info(struct me_memory_pool *self);
size_t me_memory_pool_get_capacity(struct me_memory_pool *self);
void *me_memory_pool_malloc(struct me_memory_pool *self, size_t size);
void *me_memory_pool_realloc(struct me_memory_pool *self, void *block, size_t size);
void me_memory_pool_free(struct me_memory_pool *self, void *block);

#endif
