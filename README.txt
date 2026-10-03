# C.E.R.A.S.

**C.E.R.A.S.** é uma linguagem de programação experimental desenvolvida em **C**, com inspiração em **programação funcional** e **abstração** e uma sintaxe baseada em **latim**.

O projeto tem como objetivo explorar a criação de uma linguagem própria desde seus fundamentos, incluindo sintaxe, análise léxica, estruturas de dados, interpretação e, futuramente, compilação.

> *Computatio, Est, Recursio, Abstractio, Syntaxis.*

## Objetivos

O C.E.R.A.S. busca experimentar uma linguagem que combine:

* Sintaxe inspirada no latim (por pura diversão minha);
* Conceitos de programação funcional;
* Funções como elementos fundamentais da linguagem, introduzindo autorreferencia;
* Abstração;
* Tipagem e estruturas de dados;
* Implementação de um interpretador/compilador próprio;
* Desenvolvimento de uma linguagem do zero.

## Conceito

A proposta do C.E.R.A.S. é utilizar elementos da língua latina para construir uma identidade sintática própria.

```

a linguagem busca experimentar alternativas inspiradas no latim.

A ideia não é simplesmente traduzir outra linguagem para latim, mas utilizar a estrutura própria do latim como parte da identidade da linguagem.
```

> Os exemplos acima representam a direção conceitual da linguagem e podem não corresponder à sintaxe atualmente implementada.

## Arquitetura

A arquitetura planejada para a linguagem pode ser representada da seguinte forma:

### Lexer

Responsável por transformar o código-fonte em uma sequência de token

### Parser

Recebe os tokens e constrói uma representação estruturada do codiguim.

### AST

A **Abstract Syntax Tree** representa a estrutura lógica do código.

Por exemplo:

```text
        Call
       /    \
    soma    args
           /    \
          2      3
```

### Interpretador

Executa a AST diretamente, permitindo testar a linguagem antes da existência de um compilador nativo.

## Programação funcional

Um dos principais aspectos experimentais do C.E.R.A.S. é a utilização de conceitos de programação funcional.

Entre eles:

* Funções;
* Recursão;
* Composição;
* Imutabilidade;
* Expressões;
* Funções de primeira classe;
* Abstração.

## Tecnologias

Atualmente, o projeto utiliza principalmente:

* **C**
* CMake/Make, conforme a organização do projeto
* GCC/Clang
* Git
* Linux

A evolução do projeto também considera **Rust** para componentes futuros da implementação da linguagem.

## Filosofia

A proposta é compreender uma linguagem de programação não apenas como uma ferramenta para executar código, mas como um sistema completo composto por:

```text
Linguagem
   +
Sintaxe
   +
Semântica
   +
Representação
   +
Execução
```

O projeto serve também como laboratório para estudar:

* Teoria de linguagens de programação;
* Compiladores;
* Interpretadores;
* Estruturas de dados;
* Algoritmos;
* Programação funcional;
* Análise léxica;
* Parsing;
* Árvores sintáticas;
* Sistemas de tipos.

## Status

🚧 **Em desenvolvimento**

O C.E.R.A.S. ainda está em fase experimental. A sintaxe e a arquitetura podem sofrer alterações durante o desenvolvimento.

## Autor

**Daniel Martins**

Projeto experimental de desenvolvimento de uma linguagem de programação própria.

---
