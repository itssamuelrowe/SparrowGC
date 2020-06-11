#include <stdio.h>
#include <string.h>
#include "allocator.h"

static void printStatistics(sp_Allocator_t* allocator) {
	sp_AllocatorStatistics_t* statistics = &allocator->statistics;
    printf("[Allocator Statistics]\n");
    printf("Pages Mapped -> %d\n", statistics->pagesMapped);
    printf("Pages Unmapped -> %d\n", statistics->pagesUnmapped);
    printf("Chunks Allocated -> %d\n", statistics->chunksAllocated);
    printf("Chunks Freed -> %d\n", statistics->chunksFreed);
    printf("Free Lists Count -> %d\n", statistics->freeLength);
}

int main() {
	/* Allocate the allocator on the stack. */
	sp_Allocator_t myAllocator;
	/* All the functions to allocator interface require a pointer to the allocator.
	 * Therefore, save a pointer for convienence.
	 */
	sp_Allocator_t* allocator = &myAllocator;
	/* Initial the allocator. */
	sp_Allocator_initialize(allocator);

	/* Allocate a memory chunk. */
	char* message = sp_Allocator_allocate(allocator, 14);
	/* Use the memory chunk as you please. */
	strcpy(message, "Hello, world!");
	printf("Message: %s\n", message);
	/* Deallocate the memory chunk. */
	sp_Allocator_deallocate(allocator, message);


	/* Print statistics and destroy the allocator. */
	printStatistics(allocator);
	sp_Allocator_destroy(allocator);

	return 0;
}
