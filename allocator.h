// Saturday, May 30 2020

#ifndef SPARROW_ALLOCATOR_H
#define SPARROW_ALLOCATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SP_PAGE_SIZE 4096

/******************************************************************************
 * AllocatorStatistics                                                        *
 ******************************************************************************/

struct sp_AllocatorStatistics_t {
	int32_t pagesMapped;
	int32_t pagesUnmapped;
	int32_t chunksAllocated;
	int32_t chunksFreed;
	int32_t freeLength;
};

typedef struct sp_AllocatorStatistics_t sp_AllocatorStatistics_t;

/******************************************************************************
 * FreeList                                                                   *
 ******************************************************************************/

struct sp_FreeList_t {
   size_t size;
   struct sp_FreeList_t* next;
};

typedef struct sp_FreeList_t sp_FreeList_t;

/******************************************************************************
 * Allocator                                                                  *
 ******************************************************************************/

struct sp_Allocator_t {
    sp_AllocatorStatistics_t statistics;
    sp_FreeList_t* freeList;
};

typedef struct sp_Allocator_t sp_Allocator_t;

void sp_Allocator_initialize(sp_Allocator_t* allocator);
void sp_Allocator_destroy(sp_Allocator_t* allocator);
void* sp_Allocator_allocate(sp_Allocator_t* allocator, size_t size);
void sp_Allocator_deallocate(sp_Allocator_t* allocator, void* object);

#endif /* SPARROW_ALLOCATOR_H */
