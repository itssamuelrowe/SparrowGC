// Saturday, May 30 2020

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>

#include "allocator.h"

static int32_t countFreeLists(sp_Allocator_t* allocator);
static bool isSorted(sp_Allocator_t* allocator);
static void coalesce(sp_Allocator_t* allocator);
static void insertFreeList(sp_Allocator_t* allocator, sp_FreeList_t* freeList);
static void addPage(sp_Allocator_t* allocator);
static sp_FreeList_t* findChunk(sp_Allocator_t* allocator, size_t size);
static size_t divide(size_t a, size_t b);
static void* allocateLarge(sp_Allocator_t* allocator, size_t size);

int32_t countFreeLists(sp_Allocator_t* allocator) {
    int result = 0;

    sp_FreeList_t* current = allocator->freeList;
    while (current != NULL) {
        result++;
        current = current->next;
    }

    return result;
}

bool isSorted(sp_Allocator_t* allocator) {
    sp_FreeList_t* current = allocator->freeList;
    bool result = true;
    while (current != NULL) {
        if (current > current->next) {
            result = false;
            break;
        }
        current = current->next;
    }
    return result;
}

void coalesce(sp_Allocator_t* allocator) {
    sp_FreeList_t* current = allocator->freeList;
    while (current != NULL) {
        if ((void*)current + current->size == (void*)current->next) {
            current->size = current->size + current->next->size;
            current->next = current->next->next;
        }
        else {
            current = current->next;
        }
    }

    if (!isSorted(allocator)) {
        printf("[internal error] The free lists are unsorted.\n");
    }
}

void insertFreeList(sp_Allocator_t* allocator, sp_FreeList_t* freeList) {
    sp_FreeList_t* current = allocator->freeList;
    if (current == NULL) {
        /* There is no free list. The specified free list is the first one. */
        allocator->freeList = freeList;
    }
    else if (freeList < current) {
        /* Insert the new free list at the head of the linked list. */
        freeList->next = current;
        allocator->freeList = freeList;
    }
    else {
        while (true) {
            /* We are either at the end of the list or the new list should be
             * inserted between the current and current's successor.
             */
            if((current->next == NULL) ||
               (freeList > current && freeList < current->next)) {
                freeList->next = current->next;
                current->next = freeList;
                break;
            }
            // increment
            current = current->next;
        }
    }
}

void addPage(sp_Allocator_t* allocator) {
    void* address = mmap(NULL, SP_PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if ((int32_t)address == -1) {
        printf("[internal error] Failed to map a page.\n");
    }
    else {
        sp_FreeList_t* freeList = (sp_FreeList_t*)address;
        freeList->size = SP_PAGE_SIZE;
        freeList->next = NULL;
        insertFreeList(allocator, freeList);
        allocator->statistics.pagesMapped++;
    }
}

sp_FreeList_t* findChunk(sp_Allocator_t* allocator, size_t size) {
    sp_FreeList_t* current = allocator->freeList;
    sp_FreeList_t* previous = NULL;
    sp_FreeList_t* secondBest = NULL;
    size_t minSize = 5000;
    sp_FreeList_t* bestChunk = NULL;

    while (current != NULL) {
        if ((current->size >= size) && (current->size < minSize)) {
            minSize = current->size;
            bestChunk = current;
            secondBest = previous;
        }
        previous = current;
        current = current->next;
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
            secondBest->next = bestChunk->next;
        }
        else {
            allocator->freeList = bestChunk->next;
        }

        /* Evaluate the unused memory in the best chunk. If it's large enough,
         * return it back to the free list.
         */
        size_t excessAmount = bestChunk->size - size;
        if (excessAmount > sizeof(sp_FreeList_t*) + sizeof(size_t)) {
            bestChunk->size = size;
            void* nextFreeAddress = (void*)bestChunk + size;
            sp_FreeList_t* excess = (sp_FreeList_t*)nextFreeAddress;
            excess->size = excessAmount;
            excess->next = NULL;
            insertFreeList(allocator, excess);
        }
    }
    return result;
}

size_t divide(size_t a, size_t b) {
    size_t result = a / b;
    if (result * b != a) {
        result++;
    }
    return result;
}

void* allocateLarge(sp_Allocator_t* allocator, size_t size) {
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
        newChunk->size = pageCount * SP_PAGE_SIZE;
        newChunk->next = 0;

        allocator->statistics.pagesMapped += pageCount;

        result = address + sizeof (size_t);
    }
    return result;
}

#define OBJECT_HEADER_SIZE sizeof (size_t)

void sp_Allocator_initialize(sp_Allocator_t* allocator) {
    allocator->freeList = NULL;
    allocator->statistics.pagesMapped = 0;
    allocator->statistics.pagesUnmapped = 0;
    allocator->statistics.chunksAllocated = 0;
    allocator->statistics.chunksFreed = 0;
    allocator->statistics.freeLength = 0;
}

void sp_Allocator_destroy(sp_Allocator_t* allocator) {
}

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
            allocator->statistics.chunksAllocated++;

            result = address + OBJECT_HEADER_SIZE;
        }
    }
    return result;
}

void sp_Allocator_deallocate(sp_Allocator_t* allocator, void* object) {
    allocator->statistics.chunksFreed++;
    sp_FreeList_t* chunk = (sp_FreeList_t*)(object - OBJECT_HEADER_SIZE);

    chunk->next = NULL;
    if (chunk->size > SP_PAGE_SIZE) {
        int32_t pages = divide(chunk->size, SP_PAGE_SIZE);
        int result = munmap(chunk, chunk->size);
        if (result == -1) {
            printf("[internal error] Failed to unmap large page.\n");
        }
        else {
            allocator->statistics.pagesUnmapped += pages;
        }
    }
    else {
        insertFreeList(allocator, chunk);
    }

    coalesce(allocator);
}