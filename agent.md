# C-Owner (Safe Pointers Library)

## Objetivo

Desenvolver uma biblioteca básica (ADTs e funções associadas) que implementam ponteiros seguros (safe pointers) em C.

Vamos usar somente umas poucas extensões gcc (mas suportada pelo clang também: `__atribute__((cleanup(fn)))` e `__atribute__((error(...)))`) necessárias para a implementação. Todo o resto do código respeita C padrão.

## Ponteiros seguros

Há basicamente 3 tipos de ponteiros seguros:

- `UniquePtr`: dono único. Não pode ser copiado, apenas movido (transferência de ownership). Liberado automaticamente ao final do escopo via `UNIQ_SCOPED`.
- `SharedPtr`: dono compartilhado. Pode ser clonado para múltiplas referências fortes. Quando `strong_count` chega a zero, os dados são destruídos. O bloco de controle só é liberado quando também `weak_count == 0`.
- `WeakRef`: observador sem ownership. Não sustenta o objeto vivo. Deve ser promovido via `WeakRef_lock` para um `SharedPtr` antes de usar — o resultado é um `OptRef` que indica se o objeto ainda existe.

Em todos os casos a variável, após liberada, é deixada em estado inválido (ponteiro interno vai para NULL).

---

## Arquitetura

### CtrlBlock (privado — só no `.c`)

Fat pointer com prefixo antes dos dados do usuário:

```plaintext
[ kind | dtor | size | max_count | strong_count | weak_count | data[] ]
                                                               ▲
                                               user recebe ponteiro aqui
```

- `kind`: `KIND_UNIQUE` ou `KIND_SHARED` — política do bloco
- `dtor`: destrutor do conteúdo (padrão: NULL; customizável)
- `size`: bytes alocados em `data[]`
- `max_count`: 1 = unique, SIZE_MAX = shared
- `strong_count`: donos ativos — controla quando os dados morrem
- `weak_count`: observadores ativos — controla quando o bloco morre
- `data[]`: FAM com `alignas(max_align_t)` para alinhamento correto

Navegação: `CTRL_BLOCK(ptr)` — macro `container_of` interna via `offsetof`.

### Handles públicos (no `.h`)

Todos são thin wrapper structs com um único campo `data`:

```c
typedef struct { void *data; } UniquePtr;
typedef struct { void *data; } SharedPtr;
typedef struct { void *data; } WeakRef;
typedef struct { void *data; } AnyPtr;          // kind lido do CtrlBlock
typedef struct { bool is_valid; void *data; } OptRef;  // resultado de WeakRef_lock
```

`OptRef` é efêmero — não participa de contadores, não tem free.

---

## Convenções de nomenclatura

| Categoria | Convenção | Exemplos |
| --- | --- | --- |
| Tipos | `PascalCase` | `UniquePtr`, `CtrlBlock`, `OptRef` |
| Funções | `PascalCase_verb` | `UniquePtr_make`, `WeakRef_lock` |
| `_Generic` polimórficos | `PascalCase_verb` (idem funções) | `SmartPtr_free`, `SmartPtr_copy` |
| Campos e variáveis | `snake_case` | `strong_count`, `is_valid`, `max_count` |
| Macros com impacto sintático | `ALL_CAPS` | `UNIQ_SCOPED`, `UNIQUE(size)` |

Justificativa de `_Generic` seguir a regra de funções: o caller não precisa saber que é uma macro — comporta-se como despacho polimórfico transparente.

Evitar sufixo `_t` nos tipos (reservado pela stdlib/POSIX) e nomes `unique_ptr`/`shared_ptr` em minúsculas (reservados para futura adoção pelo padrão C).

### Prefixos e include guards

| Uso | Prefixo | Exemplo | Justificativa |
| --- | --- | --- | --- |
| Include guards | `UFAM_` + nome do arquivo | `UFAM_COWNER_H` | evita colisão com headers de outros projetos do aluno |
| Macros internas (`.c`) | sem prefixo | `CTRL_BLOCK(ptr)` | visível só dentro do `.c`, sem risco de colisão |
| Macros públicas pedagógicas | sem prefixo | `UNIQUE(size)`, `UNIQ_SCOPED` | clareza para o aluno, foco no conceito |
| Macros públicas de dispatch | sem prefixo | `SmartPtr_free(p)` | segue convenção de função |

Nome da biblioteca: **cowner** (C + owner). Prefixo `UFAM_` reservado para guards — dentro do projeto é verboso sem benefício real.

---

## API pública

### UniquePtr

```c
UniquePtr UniquePtr_make(size_t size);
UniquePtr UniquePtr_make_dtor(size_t size, void (*dtor)(void *));
UniquePtr UniquePtr_move(UniquePtr *src);     // zera src->data
void     *UniquePtr_get(UniquePtr *src);      // borrow sem ownership
void      UniquePtr_reset(UniquePtr *p);      // chama dtor, zera data
```

### SharedPtr

```c
SharedPtr SharedPtr_make(size_t size);
SharedPtr SharedPtr_make_dtor(size_t size, void (*dtor)(void *));
SharedPtr SharedPtr_copy(SharedPtr *src);     // strong_count++
SharedPtr SharedPtr_move(SharedPtr *src);     // zera src, sem incrementar
void     *SharedPtr_get(SharedPtr *src);      // borrow
size_t    SharedPtr_use_count(SharedPtr *src);
void      SharedPtr_reset(SharedPtr *p);      // strong--; two-phase free
```

### WeakRef

```c
WeakRef   WeakRef_from(SharedPtr *src);       // weak_count++
OptRef    WeakRef_lock(WeakRef *w);           // promove: { is_valid, data }
bool      WeakRef_expired(WeakRef *w);        // strong_count == 0
void      WeakRef_reset(WeakRef *p);          // weak--; free bloco se strong==0&&weak==0
```

### AnyPtr (coleções heterogêneas)

```c
void  AnyPtr_free(AnyPtr *p);                 // lê kind do CtrlBlock, despacha
AnyPtr AnyPtr_copy(AnyPtr *p);
```

### Operações proibidas — erro em compile-time

Operações semanticamente inválidas são declaradas com `__attribute__((error("...")))`. Isso garante falha em **compile-time** com mensagem pedagógica, sem depender de `assert` (que some com `NDEBUG`) ou erro de linker (mensagem críptica).

```c
// UniquePtr não pode ser copiado — ownership é exclusivo
void UniquePtr_copy(UniquePtr *src)
    __attribute__((error("UniquePtr cannot be copied — use UniquePtr_move()")));

// WeakRef não pode acessar dados diretamente — exige lock e verificação
void *WeakRef_get(WeakRef *w)
    __attribute__((error("WeakRef cannot access data directly — use WeakRef_lock() and check OptRef.is_valid")));

// WeakRef não pode ser criado sem um SharedPtr de origem
WeakRef WeakRef_make(size_t size)
    __attribute__((error("WeakRef cannot be created directly — use WeakRef_from(SharedPtr *)")));

// WeakRef não pode observar UniquePtr — sem suporte a weak_count
WeakRef WeakRef_from_unique(UniquePtr *src)
    __attribute__((error("WeakRef cannot observe a UniquePtr — convert to SharedPtr first")));
```

**Regra geral**: proibição explícita aplica-se quando a operação ausente é tentadora por analogia com outro tipo e causa corrupção de memória ou violação de invariante.

### Macros

```c
UNIQUE(size)       // AnyPtr com kind=KIND_UNIQUE
SHARED(size)       // AnyPtr com kind=KIND_SHARED
UNIQ_SCOPED        // __attribute__((cleanup(UniquePtr_reset)))
SHARED_SCOPED      // __attribute__((cleanup(SharedPtr_reset)))
SmartPtr_free(p)   // _Generic → reset correto para cada handle
SmartPtr_copy(p)   // _Generic → copy correto para cada handle
```

---

## Regras de ciclo de vida do CtrlBlock

| Evento | Ação |
| --- | --- |
| `strong_count` → 0 | chama `dtor(data)`, invalida dados |
| `strong_count == 0 && weak_count == 0` | `free(bloco)` |
| `weak_lock` com `strong_count == 0` | retorna `{ .is_valid = false }` |
