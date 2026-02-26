#include <sys/mman.h>

fn void sys_munmap_anon(void *ptr, size_t bytes) {
  if (ptr == NULL || bytes == 0) {
    return;
  }
  munmap(ptr, bytes);
}
