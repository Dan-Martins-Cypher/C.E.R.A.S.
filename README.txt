C.E.R.A.S.

Esta é uma linguagem de programação experimental desenvolvida em C, com inspiração em programação funcional, abstração e uma sintaxe baseada em latim.

O projeto tem como objetivo explorar a criação de uma linguagem própria desde seus fundamentos, incluindo sintaxe, análise léxica, estruturas de dados, interpretação e, futuramente, compilação.

~Computatio Est Recursio Abstractio Syntaxis.

Meu projeto busca no fim experimentar uma linguagem que combine:

Sintaxe inspirada no latim (por pura diversão minha);

Conceitos de programação funcional;

Funções como elementos fundamentais da linguagem, introduzindo autorreferência;

Tipagem e estruturas de dados;

Implementação de um interpretador/compilador próprio;

Desenvolvimento de uma linguagem do zero.

~Conceito~

A proposta do C.E.R.A.S. é utilizar elementos da língua latina para construir uma identidade sintática própria.

A linguagem busca experimentar alternativas inspiradas no latim.

A ideia não é simplesmente traduzir outra linguagem para latim, mas utilizar a estrutura própria do latim como parte da identidade da linguagem.

Os exemplos e conceitos apresentados neste documento representam a direção conceitual da linguagem e podem não corresponder à sintaxe atualmente implementada.

Arquitetura

A linguagem é projetada em etapas que representam o caminho entre o código-fonte e sua execução:

Código-fonte
|
Lexer
|
Tokens
|
Parser
|
AST
|
Interpretador

Lexer

Responsável pela transform do código-fonte em uma sequência de tokens.

Por exemplo, uma expressão como:

soma(2, 3)

pode ser transformada em uma sequência semelhante a:

IDENTIFIER
LPAREN
NUMBER
COMMA
NUMBER
RPAREN

Parser

Recebe os tokens produzidos pelo Lexer e constrói uma representação estruturada do programa.

AST

A Abstract Syntax Tree (AST) representa a estrutura lógica do código.

Por exemplo:

```
    Call
   /    \
soma    args
       /    \
      2      3
```

Interpretador

Executa a AST diretamente, permitindo testar e desenvolver a linguagem antes da existência de um compilador nativo.
