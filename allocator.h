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
	int32_t m_pagesMapped;
	int32_t m_pagesUnmapped;
	int32_t m_chunksAllocated;
	int32_t m_chunksFreed;
	int32_t m_freeLength;
};

typedef struct sp_AllocatorStatistics_t sp_AllocatorStatistics_t;

/******************************************************************************
 * FreeList                                                                   *
 ******************************************************************************/

struct sp_FreeList_t {
   size_t m_size;
   struct sp_FreeList_t* m_next;
};

typedef struct sp_FreeList_t sp_FreeList_t;

/******************************************************************************
 * Allocator                                                                  *
 ******************************************************************************/

struct sp_Allocator_t {
    sp_AllocatorStatistics_t m_statistics;
    sp_FreeList_t* m_freeList;
};

typedef struct sp_Allocator_t sp_Allocator_t;

void sp_Allocator_initialize(sp_Allocator_t* allocator);
void sp_Allocator_destroy(sp_Allocator_t* allocator);
void* sp_Allocator_allocate(sp_Allocator_t* allocator, size_t size);
void sp_Allocator_deallocate(sp_Allocator_t* allocator, void* object);

#endif /* SPARROW_ALLOCATOR_H */