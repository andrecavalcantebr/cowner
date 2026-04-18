# DECISIONS.md — Decisões de Design do cowner

Registro das decisões técnicas não-óbvias tomadas durante o desenvolvimento, com a justificativa de cada uma. Serve como contexto para o agente e para novos colaboradores.

---

## Alocação e layout de memória

### `calloc` em vez de `malloc`

O `CtrlBlock` é alocado com `calloc(1, total)` em vez de `malloc + memset`. Além de zerar o bloco (evitando leitura de lixo nos campos `strong_count`, `weak_count`, etc.), o `calloc` pode ser mais eficiente em sistemas que já entregam páginas zeradas, e documenta intenção.

### FAM com `alignas(max_align_t)`

O campo `data[]` usa `alignas(max_align_t)` para garantir alinhamento correto para qualquer tipo que o usuário armazene — evita UB de acesso desalinhado independentemente do que for colocado em `data`.

### `ckd_add` próprio em vez de `stdckd.h`

`stdckd.h` (C23) não está disponível no GCC 10/11. Implementamos `ckd_add` manualmente checando overflow antes de atribuir — a versão correta testa primeiro e só atribui se seguro.

### `explicit_bzero` em vez de `memset`

`memset` pode ser removido pelo compilador se ele detectar que o buffer não é mais usado depois. `explicit_bzero` (glibc 2.25+) não pode — é seguro para zerar dados sensíveis antes do `free`.

---

## Semântica de tipos

### `UniquePtr`, `SharedPtr`, `WeakRef` como thin wrappers (não `void *` nu)

Encapsular em `struct { void *data; }` permite ao compilador distinguir os tipos. Sem isso, não há como usar `_Generic` para dispatch em tempo de compilação nem emitir erros via `__attribute__((error(...)))`.

### `OptRef` como retorno de `WeakRef_lock`

`WeakRef_lock` não pode retornar `SharedPtr` diretamente — isso incrementaria `strong_count` de forma silenciosa e mudaria a semântica. `OptRef` é um par `{ bool is_valid; void *data; }`: um borrow efêmero, não um novo dono. O acesso aos dados via `OptRef` é válido apenas enquanto o `SharedPtr` original estiver vivo na mesma thread.

### `AnyPtr` como union de `UniquePtr` e `SharedPtr`

Permite funções como `UNIQUE(size)` e `SHARED(size)` devolverem o mesmo tipo de retorno, facilitando inicialização com um único assignment. O campo compartilhado `.data` no mesmo offset (ambos são `struct { void *data; }`) garante que a leitura via qualquer membro é válida (C permite leitura de union via membro inativo se o layout for idêntico).

---

## Operações proibidas em tempo de compilação

### `__attribute__((error(...)))` em vez de `assert`

`assert` é removido com `-DNDEBUG` (modo release). Para operações que são sempre erros de programação — como copiar um `UniquePtr` — usamos `__attribute__((error("...")))`, que gera um **erro de compilação** se a função for referenciada, independente de flags de build.

Operações proibidas declaradas no `.h`:

- `UniquePtr_copy` — copiar um `UniquePtr` viola unicidade
- `WeakRef_get` — acesso direto a `WeakRef` ignora o verificador de validade
- `WeakRef_make` — `WeakRef` não tem alocação própria
- `WeakRef_from_unique` — `WeakRef` não pode observar `UniquePtr`

---

## Despacho polimórfico

### `_Generic` para `SmartPtr_free` e `SmartPtr_copy`

Permite código genérico sem void pointers soltos. O compilador resolve a função correta em tempo de compilação com base no tipo do ponteiro passado. Não há custo de runtime.

---

## Destruição de objetos

### Dois estágios para `SharedPtr`

1. Quando `strong_count` chega a zero: destrutor é chamado, `data[]` é zerado — o objeto morre.
2. Quando `weak_count` também chega a zero: `free(blk)` — o bloco de controle é liberado.

Isso permite que `WeakRef` detecte a morte do objeto (via `strong_count == 0`) mesmo depois que todos os `SharedPtr` foram resetados.

### Destrutor recursivo no `main.c` (lista ligada)

`node_dtor` chama `UniquePtr_reset(&n->next)`, que por sua vez chama o destrutor do nó seguinte — destruição recursiva da lista. Isso garante que liberar o nó cabeça libera a lista inteira sem loop explícito.

---

## Bugs conhecidos / em investigação

> [!NOTE] Seção para rastrear problemas descobertos — bugs pré-v1 já foram sanados.

| # | Descrição | Arquivo | Status |
| --- | ----------- | --------- | -------- |
| — | — | — | — |
