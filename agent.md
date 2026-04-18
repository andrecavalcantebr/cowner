# agent.md — C-Owner

Este arquivo é o ponto de entrada para o agente de IA que auxilia no desenvolvimento da biblioteca **cowner**.

A especificação completa do projeto — objetivos, arquitetura, API pública, convenções e regras de ciclo de vida — está em:

👉 **[README.md](README.md)**

Descrição das decisões de projeto, bugs e outras infos para desenvolvedores:

👉 **[DECISIONS.md](DECISIONS.md)**

---

## Contexto para o agente

- Linguagem: C (padrão `gnu2x`, GCC/Clang)
- Extensões usadas: `__attribute__((cleanup(fn)))` e `__attribute__((error(...)))`
- Arquivos principais: `src/cowner.h`, `src/cowner.c`, `src/main.c`
- Build: `make` / `make clean`
- Verificação de leaks: `valgrind --leak-check=full ./cowner`
- Repositório: https://github.com/andrecavalcantebr/cowner
