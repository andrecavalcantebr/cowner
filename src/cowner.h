/*
    FILE: cowner.h
    DESC: Declarations of safe pointers lib in C
    AUTHOR: Andre Cavalcante
    LICENSE: GPL-v3
    DATE: Apr, 26
*/

#ifndef UFAM_COWNER_H__
#define UFAM_COWNER_H__

#include <stdbool.h>
#include <stddef.h>

// Public handles 

typedef struct {
    void *data;
} UniquePtr;

typedef struct {
    void *data;
} SharedPtr;

typedef struct {
    void *data;
} WeakRef;

typedef union {
    UniquePtr unique;
    SharedPtr shared;    
} AnyPtr;

typedef struct {
    bool is_valid;
    void *data;
} OptRef;

// Unique Pointer semantics

UniquePtr UniquePtr_make(size_t size);
UniquePtr UniquePtr_make_dtor(size_t size, void (*dtor)(void *));
UniquePtr UniquePtr_move(UniquePtr *src);
void     *UniquePtr_get(UniquePtr *src);
void      UniquePtr_reset(UniquePtr *p);

// Shared Pointer semantics

SharedPtr SharedPtr_make(size_t size);
SharedPtr SharedPtr_make_dtor(size_t size, void (*dtor)(void *));
SharedPtr SharedPtr_copy(SharedPtr *src);
SharedPtr SharedPtr_move(SharedPtr *src);
void     *SharedPtr_get(SharedPtr *src); 
size_t    SharedPtr_use_count(SharedPtr *src);
void      SharedPtr_reset(SharedPtr *p);

// Weak Pointer semantics

WeakRef   WeakRef_from(SharedPtr *src);
OptRef    WeakRef_lock(WeakRef *w);
bool      WeakRef_expired(WeakRef *w);
void      WeakRef_reset(WeakRef *p);

// Any Pointers semantics (heterogeneous collections)
void      AnyPtr_free(AnyPtr *p);
AnyPtr    AnyPtr_copy(AnyPtr *p);


// Prohibit operations (compile-time error)

// UniquePtr cannot be copied - exclusive ownership
void UniquePtr_copy(UniquePtr *src)
    __attribute__((error("UniquePtr cannot be copied — use UniquePtr_move()")));

// WeakRef cannot acess data directly — need lock and check
void *WeakRef_get(WeakRef *w)
    __attribute__((error("WeakRef cannot access data directly — use WeakRef_lock() and check OptRef.is_valid")));

// WeakRef cannot be created without a SharedPtr source
WeakRef WeakRef_make(size_t size)
    __attribute__((error("WeakRef cannot be created directly — use WeakRef_from(SharedPtr *)")));

// WeakRef cannot observe a UniquePtr
WeakRef WeakRef_from_unique(UniquePtr *src)
    __attribute__((error("WeakRef cannot observe a UniquePtr — convert to SharedPtr first")));


//Auxiliars Macros

#define UNIQUE(size) ((AnyPtr){ .unique = UniquePtr_make(size) })
#define SHARED(size) ((AnyPtr){ .shared = SharedPtr_make(size) })
#define UNIQ_SCOPED   __attribute__((cleanup(UniquePtr_reset)))
#define SHARED_SCOPED __attribute__((cleanup(SharedPtr_reset)))
#define SmartPtr_free(p) _Generic((p),   \
    UniquePtr *: UniquePtr_reset,        \
    SharedPtr *: SharedPtr_reset,        \
    WeakRef   *: WeakRef_reset           \
)(p)

#define SmartPtr_copy(p) _Generic((p),   \
    UniquePtr *: UniquePtr_copy,         \
    SharedPtr *: SharedPtr_copy          \
)(p)


#endif //UFAM_COWNER_H__
