# nsx-utils

`nsx-utils` contains low-level utility code reused across NSX bare-metal apps.

Contents:
- timers and timing helpers
- energy and power measurement helpers
- optional heap helpers used by selected integrations

Performance and PMU profiling helpers now live in:
- `nsx-perf`
- `nsx-pmu-armv8m`

Public interfaces live in `includes-api/`. The module is CMake-first and intended
to be vendored into generated apps.

Malloc and Free work as usual:

```c
// Some compilers need this to be word aligned
uint8_t ucHeap[NS_RPC_MALLOC_SIZE_IN_K*1024]  __attribute__ ((aligned (4)));

void *memPtr = ns_malloc(requested_size);

// do stuff with memPtr

ns_free(memPtr); // put allocated block back into free heap
```
