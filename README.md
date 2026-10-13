# Disk B-Tree

Trabalho desenvolvido para a disciplina de **Estruturas de Dados II**.

## Descrição

O Disk B-Tree é uma implementação de uma Árvore B dinâmica desenvolvida em linguagem C, com armazenamento persistente em arquivos binários. O projeto utiliza uma estrutura de árvore balanceada para organizar valores inteiros, permitindo realizar operações de busca, inserção e remoção.

O grau mínimo `t` da árvore é definido pelo usuário durante a inicialização, permitindo configurar sua capacidade de armazenamento e adaptar a estrutura a diferentes cenários.

Diferentemente de uma implementação convencional, cada nó da árvore é armazenado em um arquivo binário individual. Os nós são acessados por meio de seus nomes de arquivo, reduzindo a necessidade de manter toda a estrutura carregada na memória principal. As informações necessárias para recuperar a árvore também são persistidas em um arquivo de metadados.

## Funcionalidades

- Inicialização da Árvore B com grau mínimo configurável;
- Inserção de valores inteiros:
  - Inserção individual;
  - Inserção de múltiplos valores aleatórios;
  - Divisão de nós cheios (*split*);
- Remoção de valores:
  - Remoção de chaves em nós folha;
  - Substituição por predecessor ou sucessor;
  - Redistribuição de chaves entre nós irmãos;
  - Fusão de nós (*merge*);
  - Ajuste da raiz após a remoção;
- Busca de valores na árvore;
- Impressão visual da árvore, organizada por níveis;
- Armazenamento de cada nó em um arquivo binário separado;
- Geração dinâmica de nomes para os arquivos dos nós;
- Persistência dos metadados da árvore;
- Recuperação automática da estrutura ao iniciar o programa novamente;
- Alocação dinâmica de memória para as chaves e referências aos nós filhos.

## Estruturas de Dados Utilizadas

- **Árvore B:** estrutura de busca balanceada que mantém as chaves ordenadas e permite operações eficientes.
- **Arquivos binários:** armazenamento persistente dos nós da árvore.
- **Referências a arquivos:** nomes dos arquivos utilizados para localizar os nós filhos.
- **Alocação dinâmica:** gerenciamento de chaves, referências e demais estruturas conforme o grau mínimo configurado.

## Armazenamento em Disco

O projeto utiliza o diretório `bin/` para armazenar os dados persistentes da árvore.

- `bin/node_<id>.bin`: arquivos binários individuais que representam os nós da árvore.
- `bin/tree.meta`: arquivo de metadados contendo informações necessárias para recuperar a estrutura, incluindo o grau mínimo, a referência à raiz e o próximo identificador de nó.

Ao encerrar o programa, os metadados são salvos para permitir a recuperação da árvore na próxima execução.

**Importante:** mantenha o diretório `bin/` e seus arquivos para preservar os dados entre execuções. Remover esses arquivos reinicia o armazenamento persistente.

## Como compilar

Com o compilador GCC instalado, execute o comando abaixo no diretório do projeto, substituindo `main.c` pelo nome do arquivo-fonte, caso seja diferente.

```bash
gcc main.c -o disk-b-tree
```

## Como executar

```bash
./disk-b-tree
```

Na primeira execução, o programa solicitará o grau mínimo `t` da árvore. São aceitos valores entre 2 e 1000.

Nas execuções seguintes, caso os metadados e os arquivos necessários estejam disponíveis e válidos, a árvore será recuperada automaticamente.

## Como usar

Ao executar o programa, será exibido um menu com as seguintes opções:

1. Inserir valores;
2. Remover valores;
3. Buscar um valor;
4. Imprimir a Árvore B;
0. Encerrar o programa.

### Exemplo de utilização

1. Execute o programa e defina o grau mínimo da árvore.
2. Insira valores individualmente ou gere uma sequência de valores aleatórios.
3. Utilize a opção de impressão para visualizar a estrutura e seus níveis.
4. Realize buscas para verificar a presença de valores.
5. Remova valores e observe as alterações na estrutura.
6. Encerre o programa e execute-o novamente para verificar a recuperação dos dados persistidos.

## Tecnologias

- Linguagem C
- Compilador GCC
- Arquivos binários
- Sistema de arquivos Linux/POSIX

## Objetivos acadêmicos

O projeto tem como objetivo aplicar os conceitos de Árvores B estudados em Estruturas de Dados II, explorando algoritmos de busca, inserção e remoção, balanceamento da estrutura, alocação dinâmica e armazenamento de dados em memória secundária.

A implementação também permite estudar os desafios de representar e manipular estruturas de dados persistentes por meio de arquivos.

## Autores

- Eduardo de Araújo ALexandre
