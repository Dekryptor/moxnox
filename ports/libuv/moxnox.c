/* Copyright libuv project contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <time.h>
#include <unistd.h>

void uv_loadavg(double avg[3]) {
  avg[0] = 0.0;
  avg[1] = 0.0;
  avg[2] = 0.0;
}


int uv_exepath(char* buffer, size_t* size) {
  /* Moxnox has no interface for retrieving the executable's path yet. */
  return UV_ENOSYS;
}


uint64_t uv_get_free_memory(void) {
  return 0;  /* unknown */
}


uint64_t uv_get_total_memory(void) {
  long pages;
  long pagesize;

  pages = sysconf(_SC_PHYS_PAGES);
  pagesize = sysconf(_SC_PAGESIZE);

  if (pages < 0 || pagesize < 0)
    return 0;

  return (uint64_t)pages * (uint64_t)pagesize;
}


uint64_t uv_get_constrained_memory(void) {
  return 0;  /* unknown */
}


uint64_t uv_get_available_memory(void) {
  return 0;  /* unknown */
}


int uv_resident_set_memory(size_t* rss) {
  return UV_ENOSYS;
}


int uv_uptime(double* uptime) {
  struct timespec ts;

  /* The monotonic clock starts at boot. */
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return UV__ERR(errno);

  *uptime = ts.tv_sec + ts.tv_nsec / 1e9;
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  return UV_ENOSYS;
}


int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  *addresses = NULL;
  *count = 0;
  return UV_ENOSYS;
}
