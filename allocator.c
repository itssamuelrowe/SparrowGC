// Saturday, May 30 2020

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>

#include "allocator.h"

void sp_Allocator_initialize(sp_Allocator_t* allocator) {
    allocator->m_freeList = NULL;
    allocator->m_statistics.m_pagesMapped = 0;
    allocator->m_statistics.m_pagesUnmapped = 0;
    allocator->m_statistics.m_chunksAllocated = 0;
    allocator->m_statistics.m_chunksFreed = 0;
    allocator->m_statistics.m_freeLength = 0;
}

void sp_Allocator_destroy(sp_Allocator_t* allocator) {
}

static long countFreeLists(sp_Allocator_t* allocator) {
    int result = 0;

    // Lock before reading
    sp_FreeList_t* current = allocator->m_freeList;
    while (current != NULL) {
        result++;
        current = current->m_next;
    }
    // Unlock after reading

    return result;
}

static bool isSorted(sp_Allocator_t* allocator) {
    sp_FreeList_t* current = allocator->m_freeList;
    bool result = true;
    while (current != NULL) {
        if (current > current->m_next) {
            result = false;
            break;
        }
        current = current->m_next;
    }
    return result;
}

static void coalesce(sp_Allocator_t* allocator) {
    sp_FreeList_t* current = allocator->m_freeList;
    while (current != NULL) {
        if ((void*)current + current->m_size == (void*)current->m_next) {
            current->m_size = current->m_size + current->m_next->m_size;
            current->m_next = current->m_next->m_next;
        }
        else {
            current = current->m_next;
        }
    }

    if (!isSorted(allocator)) {
        printf("[internal error] The free lists are unsorted.\n");
    }
}

static void insertFreeList(sp_Allocator_t* allocator, sp_FreeList_t* freeList) {
    sp_FreeList_t* current = allocator->m_freeList;
    if (current == NULL) {
        /* There is no free list. The specified free list is the first one. */
        allocator->m_freeList = freeList;
    }
    else if (freeList < current) {
        /* Insert the new free list at the head of the linked list. */
        freeList->m_next = current;
        allocator->m_freeList = freeList;
    }
    else {
        while (true) {
            /* We are either at the end of the list or the new list should be
             * inserted between the current and current's successor.
             */
            if((current->m_next == NULL) ||
               (freeList > current && freeList < current->m_next)) {
                freeList->m_next = current->m_next;
                current->m_next = freeList;
                break;
            }
            // increment
            current = current->m_next;
        }
    }
}

static void addPage(sp_Allocator_t* allocator) {
    void* address = mmap(NULL, SP_PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if ((int32_t)address == -1) {
        printf("[internal error] Failed to map a page.\n");
    }
    else {
        sp_FreeList_t* freeList = (sp_FreeList_t*)address;
        freeList->m_size = SP_PAGE_SIZE;
        freeList->m_next = NULL;
        insertFreeList(allocator, freeList);
        allocator->m_statistics.m_pagesMapped++;
    }
}

static sp_FreeList_t* findChunk(sp_Allocator_t* allocator, size_t size) {
    sp_FreeList_t* current = allocator->m_freeList;
    sp_FreeList_t* previous = NULL;
    sp_FreeList_t* secondBest = NULL;
    size_t minSize = 5000;
    sp_FreeList_t* bestChunk = NULL;

    while (current != NULL) {
        if ((current->m_size >= size) && (current->m_size < minSize)) {
            minSize = current->m_size;
            bestChunk = current;
            secondBest = previous;
        }
        previous = current;
        current = current->m_next;
    }

    sp_FreeList_t* result = bestChunk;
    /* If we did not find a chunk large enough, add another page
     * and try again.
     */
    if (bestChunk == NULL) {
        addPage(allocator);
        result = findChunk(allocator, size);
    }
    else {
        /* Remove the chunk from the free list before it is returned. */
        if (secondBest != NULL) {
            secondBest->m_next = bestChunk->m_next;
        }
        else {
            allocator->m_freeList = bestChunk->m_next;
        }

        /* Evaluate the unused memory in the best chunk. If it's large enough,
         * return it back to the free list.
         */
        size_t excessAmount = bestChunk->m_size - size;
        if (excessAmount > sizeof(sp_FreeList_t*) + sizeof(size_t)) {
            bestChunk->m_size = size;
            void* nextFreeAddress = (void*)bestChunk + size;
            sp_FreeList_t* excess = (sp_FreeList_t*)nextFreeAddress;
            excess->m_size = excessAmount;
            excess->m_next = NULL;
            insertFreeList(allocator, excess);
        }
    }
    return result;
}

static size_t divide(size_t a, size_t b) {
    size_t result = a / b;
    if (result * b != a) {
        result++;
    }
    return result;
}

static void* allocateLarge(sp_Allocator_t* allocator, size_t size) {
    int pageCount = divide(size, SP_PAGE_SIZE);

    /* Map enough pages for the large allocation. */
    void* address = mmap(NULL, pageCount * SP_PAGE_SIZE,
        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    void* result = NULL;
    if ((int32_t)address == -1) {
        printf("[internal error] Failed to map a large page.\n");
    }
    else {
        sp_FreeList_t* newChunk = (sp_FreeList_t*)address;
        newChunk->m_size = pageCount * SP_PAGE_SIZE;
        newChunk->m_next = 0;

        allocator->m_statistics.m_pagesMapped += pageCount;

        result = address + sizeof (size_t);
    }
    return result;
}

#define OBJECT_HEADER_SIZE sizeof (size_t)

void* sp_Allocator_allocate(sp_Allocator_t* allocator, size_t size) {
    void* result = NULL;
    if (size > 0) {
        /* The chunk size requested does not include the header. Therefore,
         * we add the header size to the requested size to evaluate the
         * true size.
         */
        size += OBJECT_HEADER_SIZE;

        // TODO: Check for integer overflows!
        
        if (size > SP_PAGE_SIZE) {
            result = allocateLarge(allocator, size);
        }
        else {
            void* address = findChunk(allocator, size);
            allocator->m_statistics.m_chunksAllocated++;

            result = address + OBJECT_HEADER_SIZE;
        }
    }
    return result;
}

void sp_Allocator_deallocate(sp_Allocator_t* allocator, void* object) {
    allocator->m_statistics.m_chunksFreed++;
    sp_FreeList_t* chunk = (sp_FreeList_t*)(object - OBJECT_HEADER_SIZE);

    chunk->m_next = NULL;
    if (chunk->m_size > SP_PAGE_SIZE) {
        int32_t pages = divide(chunk->m_size, SP_PAGE_SIZE);
        int result = munmap(chunk, chunk->m_size);
        if (result == -1) {
            printf("[internal error] Failed to unmap large page.\n");
        }
        else {
            allocator->m_statistics.m_pagesUnmapped += pages;
        }
    }
    else {
        insertFreeList(allocator, chunk);
    }

    coalesce(allocator);
}