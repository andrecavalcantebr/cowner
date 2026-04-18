/*
    FILE: cowner.c 
    DESC: Implementation of safe pointers in C
    AUTHOR: Andre Cavalcante
    LICENSE: GPL-v3
    DATE: Apr, 26
*/

#include "cowner.h"

#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internals types

typedef enum {
    KIND_UNIQUE = 1,
    KIND_SHARED,
} KindPtrEnum;

typedef struct {
    KindPtrEnum kind;
    void (*dtor)(void *);
    size_t size;
    size_t max_count;
    size_t strong_count;
    size_t weak_count;
    alignas(max_align_t) char data[];
} CtrlBlock;

#define CTRL_BLOCK(ptr) ((CtrlBlock *)((char *)(ptr) - offsetof(CtrlBlock, data)))

static bool ckd_add(size_t *var, size_t size1, size_t size2) {
    *var = 0;
    if(size1 > SIZE_MAX - size2 || size2 > SIZE_MAX - size1) return true;
    *var = size1 + size2; 
    return false;
}

static CtrlBlock *ctrl_block_alloc(KindPtrEnum kind, size_t size, void(*dtor)(void *)) {
    size_t total;
    if (ckd_add(&total, sizeof(CtrlBlock), size)) return NULL;
    CtrlBlock *blk = calloc(1, total);
    if(blk) {
        blk->kind = kind;
        blk->dtor = dtor;
        blk->size = size;
        blk->max_count = (kind == KIND_UNIQUE) ? 1 : SIZE_MAX;
        blk->strong_count = 1;
        blk->weak_count = 0;
    }
    return blk;
}

// Unique Pointer semantics

UniquePtr UniquePtr_make(size_t size) {
    auto blk = ctrl_block_alloc(KIND_UNIQUE, size, NULL);
    if(blk) return (UniquePtr){.data = blk->data};
    else    return(UniquePtr){.data = NULL};
}

UniquePtr UniquePtr_make_dtor(size_t size, void (*dtor)(void *)) {
    auto blk = ctrl_block_alloc(KIND_UNIQUE, size, dtor);
    if(blk) return (UniquePtr){.data = blk->data};
    else    return(UniquePtr){.data = NULL};
}

UniquePtr UniquePtr_move(UniquePtr *src) {
    UniquePtr dst = {.data = src->data};
    src->data = NULL;
    return dst;
}

void *UniquePtr_get(UniquePtr *src) {
    return src->data;
}

void UniquePtr_reset(UniquePtr *p) {
    if(!(p->data)) return;
    CtrlBlock *blk = CTRL_BLOCK(p->data);
    if(blk->dtor) blk->dtor(p->data);
    explicit_bzero(blk, sizeof(CtrlBlock) + blk->size);
    free(blk);
    p->data = NULL;
}

// Shared Pointer semantics

SharedPtr SharedPtr_make(size_t size) {
    auto blk = ctrl_block_alloc(KIND_SHARED, size, NULL);
    if(blk) return (SharedPtr){.data = blk->data};
    else    return(SharedPtr){.data = NULL};
}

SharedPtr SharedPtr_make_dtor(size_t size, void (*dtor)(void *)) {
    auto blk = ctrl_block_alloc(KIND_SHARED, size, dtor);
    if(blk) return (SharedPtr){.data = blk->data};
    else    return(SharedPtr){.data = NULL};
}

SharedPtr SharedPtr_copy(SharedPtr *src) {
    if(!src->data) return (SharedPtr){.data=NULL};
    CtrlBlock *blk = CTRL_BLOCK(src->data);
    SharedPtr dst = {.data = src->data};
    blk->strong_count++;
    return dst;
}

SharedPtr SharedPtr_move(SharedPtr *src) {
    SharedPtr dst = {.data = src->data};
    src->data = NULL;
    return dst;
}

void *SharedPtr_get(SharedPtr *src) {
    return src->data;
}

size_t SharedPtr_use_count(SharedPtr *src) {
    if(!src->data) return 0;
    CtrlBlock *blk = CTRL_BLOCK(src->data);    
    return blk->strong_count;
}

void SharedPtr_reset(SharedPtr *p) {
    if(!(p->data)) return;
    CtrlBlock *blk = CTRL_BLOCK(p->data);
    blk->strong_count--;
    if(blk->strong_count == 0) {
        if(blk->dtor) 
            blk->dtor(p->data);
        explicit_bzero(blk->data, blk->size);
        if(blk->weak_count == 0) 
            free(blk);
    }
    p->data = NULL;
}

// Weak Pointer semantics

WeakRef WeakRef_from(SharedPtr *src) {
    if(!src->data) return (WeakRef){.data = NULL};
    CtrlBlock *blk = CTRL_BLOCK(src->data);
    blk->weak_count++;
    return (WeakRef){.data = src->data};
}

OptRef WeakRef_lock(WeakRef *w) {
    if (!w->data) return (OptRef){.is_valid=false, .data=NULL};
    CtrlBlock *blk = CTRL_BLOCK(w->data);
    if (blk->strong_count == 0) return (OptRef){.is_valid=false, .data=NULL};
    return (OptRef){.is_valid=true, .data=w->data};
}

bool WeakRef_expired(WeakRef *w) {
    if(!w->data) return false;
    CtrlBlock *blk = CTRL_BLOCK(w->data);
    return (blk->strong_count == 0);
}

void WeakRef_reset(WeakRef *p) {
    if(!(p->data)) return;
    CtrlBlock *blk = CTRL_BLOCK(p->data);
    blk->weak_count--;
    if((blk->strong_count == 0) && (blk->weak_count == 0)) {
        free(blk);
    }
    p->data = NULL;
}

// Any Pointers semantics (heterogeneous collections)
void AnyPtr_free(AnyPtr *p) {
    if(p->unique.data) {
        CtrlBlock *blk = CTRL_BLOCK(p->unique.data);
        if(blk->kind == KIND_UNIQUE) UniquePtr_reset(&(p->unique));
        else SharedPtr_reset(&(p->shared));
    }
}

AnyPtr AnyPtr_copy(AnyPtr *p) {
    if(!p->shared.data) return (AnyPtr){.shared.data=NULL};
    CtrlBlock *blk = CTRL_BLOCK(p->shared.data);
    if(blk->kind == KIND_SHARED) 
        return (AnyPtr){.shared = SharedPtr_copy(&(p->shared))};
    // KIND_UNIQUE — copy semantics proibida em runtime
    // abort() não pode ser eliminado pelo compilador (diferente de assert com NDEBUG)
    fprintf(stderr, "AnyPtr_copy: cannot copy a KIND_UNIQUE pointer — use AnyPtr with SharedPtr only\n");
    abort();
}
