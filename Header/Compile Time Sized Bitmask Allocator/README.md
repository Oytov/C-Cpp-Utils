C89 compatible library for compile time sized bitmask/slab allocators.

Example:
```c
#include <stdio.h>
#include "oyt_BitmaskAllocator.h"

#define MAX_ALLOCATIONS 1000

int main(void) {
	size_t i, lastIdx, idx;
	int allocationFailed;

	OYT_BITMASK_DEFINE(MAX_ALLOCATIONS, allocator);
	OYT_BITMASK_INIT(allocator, MAX_ALLOCATIONS);

	allocationFailed = 0;
	OYT_BITMASK_ALLOCATE(allocator, lastIdx);
	for (i = 1; i < MAX_ALLOCATIONS; i++) {
		OYT_BITMASK_ALLOCATE(allocator, idx);

		if (idx != lastIdx + 1) {
			allocationFailed = 1;
			printf("Current allocation %zu isn't after previous allocation %zu\n", idx, lastIdx);
		}

		lastIdx = idx;
	}
	
	if (!allocationFailed) {
		puts("All allocations succeeded");
	}
	else {
		puts("At least one allocation failure");
		return 0;
	}

	OYT_BITMASK_FREE(allocator, 0);
	for (i = 1; i < MAX_ALLOCATIONS; i++) {
		OYT_BITMASK_FREE(allocator, i);
	}

	return 0;
}
```