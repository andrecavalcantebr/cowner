/*
    FILE: main.c 
    DESC: entry point of the system.
        Test of safety pointer in C
    AUTHOR: Andre Cavalcante
    LICENSE: GPL-v3
    DATE: Apr, 26
*/
#include "cowner.h"
#include <stdio.h>

// ── tipos da lista ────────────────────────────────────────────────

typedef struct Node {
    int       value;
    UniquePtr next;   // dono do próximo nó — NULL = fim da lista
} Node;

typedef struct {
    UniquePtr head;   // dono do primeiro nó
    size_t    len;
} List;

// ── destrutor: chamado recursivamente ao liberar um nó ────────────

static void node_dtor(void *raw) {
    Node *n = (Node *)raw;
    UniquePtr_reset(&n->next);   // libera o filho antes do bloco pai
}

static void list_dtor(void *raw) {
    List *l = (List *)raw;
    UniquePtr_reset(&l->head);   // dispara node_dtor em cascata
}

// ── operações da lista ────────────────────────────────────────────

static void list_push(List *l, int value) {
    UniquePtr node_ptr = UniquePtr_make_dtor(sizeof(Node), node_dtor);
    if (!node_ptr.data) return;   // OOM

    Node *n  = UniquePtr_get(&node_ptr);
    n->value = value;
    n->next  = UniquePtr_move(&l->head);   // novo nó aponta para o antigo head
    l->head  = UniquePtr_move(&node_ptr);  // lista passa a ownar o novo nó
    l->len++;
}

static void list_print(List *l) {
    Node *cur = UniquePtr_get(&l->head);
    while (cur) {
        printf("%d ", cur->value);
        cur = UniquePtr_get(&cur->next);
    }
    printf("\n");
}

// ── main ──────────────────────────────────────────────────────────

int main(void) {
    puts("=== UniquePtr: lista ligada com cleanup automático ===");
    {
        UNIQ_SCOPED UniquePtr list_ptr =
            UniquePtr_make_dtor(sizeof(List), list_dtor);

        List *l = UniquePtr_get(&list_ptr);
        list_push(l, 10);
        list_push(l, 20);
        list_push(l, 30);
        list_print(l);   // 30 20 10
        // fim do bloco → UniquePtr_reset → list_dtor → node_dtor recursivo
    }
    puts("Lista liberada automaticamente ao sair do escopo.");

    puts("\n=== SharedPtr: dado compartilhado entre dois handles ===");
    {
        SharedPtr a = SharedPtr_make(sizeof(int));
        *(int *)SharedPtr_get(&a) = 42;

        SharedPtr b = SharedPtr_copy(&a);   // strong_count = 2
        printf("a = %d, b = %d, use_count = %zu\n",
               *(int *)SharedPtr_get(&a),
               *(int *)SharedPtr_get(&b),
               SharedPtr_use_count(&a));    // 2

        SharedPtr_reset(&a);   // strong_count = 1 — dado ainda vivo
        printf("Após reset de a: use_count = %zu\n",
               SharedPtr_use_count(&b));    // 1

        SharedPtr_reset(&b);   // strong_count = 0 — dado destruído + free
        puts("Dado liberado ao zerar último strong_count.");
    }

    puts("\n=== WeakRef: observador sem ownership ===");
    {
        SharedPtr owner = SharedPtr_make(sizeof(int));
        *(int *)SharedPtr_get(&owner) = 99;

        WeakRef observer = WeakRef_from(&owner);

        OptRef ref = WeakRef_lock(&observer);
        if (ref.is_valid)
            printf("Valor observado: %d\n", *(int *)ref.data);   // 99

        SharedPtr_reset(&owner);   // dado destruído; bloco ainda vivo (weak=1)

        OptRef ref2 = WeakRef_lock(&observer);
        printf("Após reset do owner: is_valid = %s\n",
               ref2.is_valid ? "true" : "false");   // false

        WeakRef_reset(&observer);   // weak=0 → free(bloco)
        puts("Bloco liberado ao zerar último weak_count.");
    }

    return 0;
}
