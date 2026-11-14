C89 compatible library for compile time sized bitmask/slab allocator.

Example:
```c
#include <stdio.h>

#define OYT_BITMASK_ALLOC_SIZE 1000
#include "oyt_BitmaskAllocator.h"

int main(void) {
	unsigned long i, lastIdx, idx;
	
	oyt_BitmaskAlloc1000 alloc;
	oyt_BitmaskAlloc1000_init(&alloc);

	/* Allocation order promise */
	lastIdx = oyt_BitmaskAlloc1000_alloc(&alloc);
	for (i = 1; i < 1000; i++) {
		idx = oyt_BitmaskAlloc1000_alloc(&alloc);
		if (idx != lastIdx + 1) {
			puts("[-] Lowest index first promise not met");
			return 0;
		}
		lastIdx = idx;
	}

	/* Verify no over allocation */
	for (i = 0; i < 42; i++) {
		if (oyt_BitmaskAlloc1000_alloc(&alloc) != 1000) {
			puts("[-] More allocations than the maximum are occuring");
			return 0;
		}
	}

	/* Freeing */
	for (i = 0; i < 1000; i++) {
		oyt_BitmaskAlloc1000_free(&alloc, i);
	}

	puts("[+] Succeeded");
	return 0;
}
```