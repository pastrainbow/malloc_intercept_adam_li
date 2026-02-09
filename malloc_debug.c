#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

// Suppose size_t is n bytes large, then number of decimal digits we need is
// ⌊log10​(2^(8n)−1)⌋+1
// This is upper bounded by log10(2^(8n)) + 1, which is around 2.408 n + 1.
// Since n > 15 (size_t's maximum must be at least 65535), we can safely
// round this to 3n.
// This overestimates 4 bytes (20 bytes actual, 24 bytes estmiated) if size_t 
// is 64 bytes wide, but we can guarantee portability this way. Also,
// considering alignment, the overhead should be negligible 
#define SIZE_T_MAX_DIGIT_COUNT (sizeof(size_t) * 3)

static pthread_once_t once_control = PTHREAD_ONCE_INIT;

static void* (*real_malloc)(size_t);
static void init_real_malloc(void) {
  real_malloc = dlsym(RTLD_NEXT, "malloc"); 
}

static const char info_prefix[] = "Total bytes allocated so far: ";
// -1 to exclude the null terminator
static size_t info_prefix_length = sizeof(info_prefix) - 1;

// Total bytes need to be kept track of per process,
// in which case we need to make it mutually exclusive for threads
static _Atomic size_t total_bytes_allocated = 0;

void* malloc(size_t size) {
  // Need the real malloc function to perform the actual functionality.
  // malloc can run concurrently, and multiple programs can load this library,
  // so if we just use a static variable here without pthread_once, we will 
  // have a data race. Although the race probably won't cause much issue due to 
  // assignment usually being atomic, it's still safer to use pthread_once to
  // avoid going into the undefined behaviour territory.
  // pthread_once does not call malloc(), so safe
  pthread_once(&once_control, init_real_malloc);

  // atomic_fetch_add is atomic at hardware level, so no malloc is used
  size_t new_total = atomic_fetch_add(&total_bytes_allocated, size) + size;

  // We have the prefix, the number, the line break, and finally the null
  // terminator
  char info[info_prefix_length + SIZE_T_MAX_DIGIT_COUNT + 2];

  strncpy(info, info_prefix, info_prefix_length);

  // Copy size's string representation to the current end of info, which should
  // be just the end of the prefix
  // Also need the size of the entire info string, so that we can write info to
  // stderr without truncation or trailing garbage characters
  int info_str_size = 
    info_prefix_length + 
    // We are just formatting a size_t, so snprintf should not allocate memory
    snprintf(
      info + info_prefix_length,
      // Plus 1 for the line break 
      SIZE_T_MAX_DIGIT_COUNT + 1, 
      "%zu\n", new_total
    );
  
  // Cannot use printf here, since it uses malloc, meaning that we will
  // run into infinite recursion. We could make printf use the real malloc
  // instead of this overriding one, but write works and is simpler to implement 
  write(STDERR_FILENO, info, (size_t)info_str_size);
  return real_malloc(size);
}
