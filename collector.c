/* The following source code was derived from here: https://github.com/munificent/mark-sweep */

/*
 * This uses the MIT License:
 * 
 * Copyright (c) 2013 Robert Nystrom
 * 
 * Permission is hereby granted, free of charge, to
 * any person obtaining a copy of this software and
 * associated documentation files (the "Software"),
 * to deal in the Software without restriction,
 * including without limitation the rights to use,
 * copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is
 * furnished to do so, subject to the following
 * conditions:
 * 
 * The above copyright notice and this permission
 * notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
 * WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
 * EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

enum ObjectType {
    OBJECT_TYPE_INTEGER,
    OBJECT_TYPE_PAIR
};

typedef enum ObjectType ObjectType;

typedef struct Object Object;

struct Object {
    ObjectType type;
    bool marked;

    /**
     * The next object in the linked list of heap allocated objects.
     */
    Object* next;

    union {
        /* OBJECT_TYPE_INTEGER */
        int32_t value;

        /* OBJECT_TYPE_PAIR */
        struct {
            Object* left;
            Object* right;
        };
    };
};

#define STACK_MAX 256

struct Context {
    Object* stack[STACK_MAX];
    int32_t stackSize;

    /**
     * The first object in the linked-list of all objects on the heap.
     */
    Object* firstObject;

    /**
     * The total number of currently allocated objects.
     */
    int32_t objectCount;

    /**
     * The number of objects required to trigger a garbage collection.
     */
    int32_t maxObjects;
};

typedef struct Context Context;

Context* newContext() {
    Context* context = malloc(sizeof(Context));
    context->stackSize = 0;
    context->firstObject = NULL;
    context->objectCount = 0;
    context->maxObjects = 8;
    return context;
}

void deleteContext(Context *context) {
    context->stackSize = 0;
    collect(context);
    free(context);
}

void push(Context* context, Object* value) {
    context->stack[context->stackSize++] = value;
}

Object* pop(Context* context) {
    return context->stack[--context->stackSize];
}

Object* newObject(Context* context, ObjectType type) {
    if (context->objectCount == context->maxObjects) {
        collect(context);
    }

    Object* object = malloc(sizeof(Object));
    object->type = type;
    object->next = context->firstObject;
    context->firstObject = object;
    object->marked = false;

    context->objectCount++;

    return object;
}

void pushInteger(Context* context, int32_t value) {
    Object* object = newObject(context, OBJECT_TYPE_INTEGER);
    object->value = value;

    push(context, object);
}

Object* pushPair(Context* context) {
    Object* object = newObject(context, OBJECT_TYPE_PAIR);
    object->right = pop(context);
    object->left = pop(context);

    push(context, object);
    
    return object;
}

void objectPrint(Object* object) {
    switch (object->type) {
        case OBJECT_TYPE_INTEGER: {
            printf("%d", object->value);
            break;
        }

        case OBJECT_TYPE_PAIR: {
            printf("(");
            objectPrint(object->left);
            printf(", ");
            objectPrint(object->right);
            printf(")");
            break;
        }
    }
}

/******************************************************************************
 * Collector                                                                  *
 ******************************************************************************/

void mark(Object* object) {
    /* If already marked, we are done. Check this first to avoid recursing
     * on cycles in the object graph.
     */
    if (!object->marked) {
        object->marked = true;

        if (object->type == OBJECT_TYPE_PAIR) {
            mark(object->left);
            mark(object->right);
        }
    }
}

void markAll(Context* context) {
    int32_t i;
    for (i = 0; i < context->stackSize; i++) {
        mark(context->stack[i]);
    }
}

void sweep(Context* context) {
    Object** object = &context->firstObject;
    while (*object != NULL) {
        if (!(*object)->marked) {
            /* This object cannot be reached.
             * Therefore, so remove it from the list and free it.
             */
            Object* unreached = *object;

            *object = unreached->next;
            free(unreached);
            context->objectCount--;
        }
        else {
            /* This object was reached, so unmark it for the next garbage collection. */
            (*object)->marked = false;
            object = &(*object)->next;
        }
    }
}

void collect(Context* context) {
    int32_t objectCount = context->objectCount;

    markAll(context);
    sweep(context);

    context->maxObjects = context->objectCount * 2;

    printf("[info] Collected %d objects, %d remaining.\n", objectCount - context->objectCount,
         context->objectCount);
}

/******************************************************************************
 * Test                                                                       *
 ******************************************************************************/

void assert(bool condition, const char* message) {
    if (!condition) {
        printf("[assertion failure] %s\n", message);
        exit(1);
    }
}

void test1() {
    printf("Test 1: Objects on stack are preserved.\n");
    Context* context = newContext();
    
    pushInteger(context, 1);
    pushInteger(context, 2);
    collect(context);
    assert(context->objectCount == 2, "Should have preserved objects.");

    deleteContext(context);
}

void test2() {
  printf("Test 2: Unreached objects are collected.\n");
  
  Context* context = newContext();
  pushInteger(context, 1);
  pushInteger(context, 2);
  pop(context);
  pop(context);

  collect(context);
  assert(context->objectCount == 0, "Should have collected objects.");

  deleteContext(context);
}

void test3() {
    printf("Test 3: Reach nested objects.\n");
  
    Context* context = newContext();
    pushInteger(context, 1);
    pushInteger(context, 2);
    pushPair(context);
    pushInteger(context, 3);
    pushInteger(context, 4);
    pushPair(context);
    pushPair(context);

    collect(context);
    assert(context->objectCount == 7, "Should have reached objects.");

    deleteContext(context);
}

void test4() {
    printf("Test 4: Handle cycles.\n");
  
    Context* context = newContext();
    pushInteger(context, 1);
    pushInteger(context, 2);
    Object* a = pushPair(context);
    pushInteger(context, 3);
    pushInteger(context, 4);
    Object* b = pushPair(context);
    /* Set up a cycle, and also make 2 and 4 unreachable and collectible. */
    a->right = b;
    b->right = a;

    collect(context);
    assert(context->objectCount == 4, "Should have collected objects.");

    deleteContext(context);
}

void testPerformance() {
    printf("Performance Test\n");
    Context* context = newContext();

    int32_t i;
    for (i = 0; i < 1000; i++) {
        int32_t j;
        for (j = 0; j < 20; j++) {
            pushInteger(context, i);
        }

        int32_t k;
        for (int k = 0; k < 20; k++) {
          pop(context);
        }
    }

    deleteContext(context);
}

int main() {
    test1();
    test2();
    test3();
    test4();
    testPerformance();

    return 0;
}