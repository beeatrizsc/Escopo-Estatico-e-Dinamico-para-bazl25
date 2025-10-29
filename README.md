# Sobre o Trabalho
Primeiro Trabalho de Implementação da matéria **Paradigmas de Programação** da Universidade Federal Fluminense (**UFF**).

O projeto consiste em um **simulador/interpretador** para a pseudolinguagem **bazl25**. O objetivo é ilustrar e comparar as regras de **escopo estático** e **escopo dinâmico**, com:
* Interpretação do pseudocódigo linha por linha.
* Exibição do estado da **pilha de ativação** (Activation Stack) a cada passo.


## Tecnologias Utilizadas

O projeto foi desenvolvido inteiramente na linguagem:
* **C**
* Requer um compilador C (como o **GCC**) para a compilação ou uma IDE para a linguagem (como o **Code::Blocks**).


## Como Compilar e Executar o Projeto

Para rodar o simulador, você precisará de um compilador C instalado e acesso ao terminal/prompt de comando, ou uma IDE da sua escolha. É crucial que o arquivo de código (`.bazl25`) esteja na **mesma pasta** do seu arquivo executável.

### 1. Compilação
Abra o terminal na pasta raiz do repositório (onde está o arquivo `simulador.c`) e compile o código:

```bash
gcc simulador.c -o bazl25_simulador
```
O comando acima irá gerar o arquivo executável chamado bazl25_simulador (ou bazl25_simulador.exe no Windows).

### 2. Execução
Execute o programa no terminal e ele solicitará as informações de entrada (nome do arquivo e modo de escopo).
* **Interação no terminal**
    *  O programa pedirá o **nome do arquivo** (.bazl25). Você deve digitar o nome exato do seu arquivo, ex: exemplo.bazl25
         
    *  O programa pedirá o **Modo de escopo** [static/dynamic]. Você deve digitar: *static* ou *dynamic*
       
## Estrutura de pseudocódigos .bazl25
Caso deseje fazer seu próprio pseudocódigo, siga a BNF de bazl25 abaixo:
```
|  <programa> ::= [<var>] <funcao>+                                   |
|  <funcao> ::= 'func <id> '( <list_ids> ') '{ [<var>] <comando>+ '}  |
|  <list_ids> ::= <id> ', <list_ids> | <id>                           |
|  <var> ::= 'var <list_ids>                                          |
|  <comando> ::= <atrib> | <chamada> | <impressao>                    |
|  <atrib> ::= <id> '= <expr>                                         |
|  <chamada> ::= <id> '( <list_expr> ')                               |
|  <list_expr> ::= <expr> ', <list_expr> | <expr>                     |
|  <expr> ::= <arg> [ '+ <expr> ]                                     |
|  <arg> ::= <id> | <num>                                             |
|  <impressao> ::= 'print <list_ids>                                  |
```
