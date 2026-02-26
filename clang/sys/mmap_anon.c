#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
  #define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MAP_NORESERVE
  #define MAP_NORESERVE 0
#endif

fn void *sys_mmap_anon(size_t bytes) {
  int   prot  = PROT_READ | PROT_WRITE;
  int   flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
  void *map   = mmap(NULL, bytes, prot, flags, -1, 0);
  if (map == MAP_FAILED) {
    return NULL;
  }
  return map;
}
