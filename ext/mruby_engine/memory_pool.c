#include "memory_pool.h"
#include "mruby_engine.h"
#include "dlmalloc.h"
#undef NOINLINE
#include <ruby.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>

#define barrier() __asm__ __volatile__("mfence": : :"memory")

#if defined(MAP_ANONYMOUS)
#define ME_MAP_ANONYMOUS MAP_ANONYMOUS
#elif defined(MAP_ANON)
#define ME_MAP_ANONYMOUS MAP_ANON
#else
#error "this gem requires anonymous memory regions"
#endif

struct me_memory_pool {
  mspace mspace;
  uint8_t *start;
  size_t capacity;
  char state;
};

#define CAPACITY_MIN ((size_t)(256 * KiB))
#define CAPACITY_MAX ((size_t)(256 * MiB))
#define ALLOC_MAX ((size_t)(256 * MiB))

static size_t round_capacity(size_t capacity) {
  size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE);
  size_t partial_page_p = capacity & (page_size - 1);
  if (partial_page_p)
    capacity = (capacity & ~(page_size - 1)) + page_size;
  return capacity;
}

struct me_memory_pool *me_memory_pool_new(size_t capacity, struct me_memory_pool_err *err) {
  size_t rounded_capacity = round_capacity(capacity);
  if (rounded_capacity < CAPACITY_MIN || CAPACITY_MAX < rounded_capacity) {
    err->type = ME_MEMORY_POOL_INVALID_CAPACITY;
    err->data.invalid_capacity.min = CAPACITY_MIN;
    err->data.invalid_capacity.max = CAPACITY_MAX;
    err->data.invalid_capacity.capacity = capacity;
    err->data.invalid_capacity.rounded_capacity = rounded_capacity;
    return NULL;
  }

  uint8_t *bytes = mmap(NULL, rounded_capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | ME_MAP_ANONYMOUS, -1, 0);
  if (bytes == MAP_FAILED) {
    err->type = ME_MEMORY_POOL_SYSTEM_ERR;
    err->data.system_err.err_no = errno;
    err->data.system_err.capacity = capacity;
    err->data.system_err.rounded_capacity = rounded_capacity;
    return NULL;
  }

  mspace mspace = create_mspace_with_base(bytes, rounded_capacity, 0);
  mspace_set_footprint_limit(mspace, rounded_capacity);
  struct me_memory_pool *self = mspace_malloc(mspace, sizeof(struct me_memory_pool));
  self->mspace = mspace;
  self->start = bytes;
  self->capacity = rounded_capacity;

  err->type = ME_MEMORY_POOL_NO_ERR;
  return self;
}

void me_memory_pool_hack(struct me_memory_pool *self) {
  const char *state;
  barrier();
  switch(self->state) {
    case 0: state = "IDLE"; break;
    case 1: state = "MALLOC"; break;
    case 2: state = "REALLOC"; break;
    case 3: state = "FREE"; break;
    default: state = "WTF?!";
  }
  fprintf(stderr, "[SCRIPT-DEBUG] alloc-state=%s\n", state);
  log_stuff(self->mspace);
}

struct meminfo me_memory_pool_info(struct me_memory_pool *self) {
  me_memory_pool_hack(self);
  struct meminfo info;
  struct mallinfo dlinfo = mspace_mallinfo(self->mspace);
  info.arena = dlinfo.arena;
  info.hblkhd = dlinfo.hblkhd;
  info.uordblks = dlinfo.uordblks;
  info.fordblks = dlinfo.fordblks;
  return info;
}

size_t me_memory_pool_get_capacity(struct me_memory_pool *self) {
  return self->capacity;
}

void *me_memory_pool_malloc(struct me_memory_pool *self, size_t size) {
  self->state = 1;
  barrier();
  void *ptr = mspace_malloc(self->mspace, size);
  self->state = 0;
  barrier();
  return ptr;
}

void *me_memory_pool_realloc(struct me_memory_pool *self, void *block, size_t size) {
  self->state = 2;
  barrier();
  void *ptr = mspace_realloc(self->mspace, block, size);
  self->state = 0;
  barrier();
  return ptr;
}

void me_memory_pool_free(struct me_memory_pool *self, void *block) {
  self->state = 3;
  barrier();
  mspace_free(self->mspace, block);
  self->state = 0;
  barrier();
}

void me_memory_pool_destroy(struct me_memory_pool *self) {
  uint8_t *start = self->start;
  size_t capacity = self->capacity;
  destroy_mspace(self->mspace);
  munmap(start, capacity);
}
