# Disk B-Tree

Trabalho desenvolvido para a disciplina de **Estruturas de Dados II**.

## Descrição

O Disk B-Tree é uma implementação de uma Árvore B dinâmica desenvolvida em linguagem C, com armazenamento persistente em arquivos binários. O projeto utiliza uma estrutura de árvore balanceada para organizar valores inteiros, permitindo realizar operações de busca, inserção e remoção.

O grau mínimo `t` da árvore é definido pelo usuário na primeira execução, permitindo configurar sua capacidade de armazenamento e adaptar a estrutura a diferentes cenários. Depois de definido, `t` é salvo nos metadados e não pode mais ser alterado, pois o formato dos arquivos de nós depende dele.

Diferentemente de uma implementação convencional, cada nó da árvore é armazenado em um arquivo binário individual. Os nós são acessados por meio de seus nomes de arquivo e carregados em memória apenas durante a operação que os utiliza, de modo que a estrutura completa nunca precisa estar na memória principal. As informações necessárias para recuperar a árvore também são persistidas em um arquivo de metadados.

## Funcionalidades

- Inicialização da Árvore B com grau mínimo configurável (valores entre 2 e 1000);
- Inserção de valores inteiros:
  - Inserção individual;
  - Inserção de N valores aleatórios entre 1 e um valor máximo informado;
  - Divisão preventiva de nós cheios (*split*) durante a descida, em uma única passada;
- Remoção de valores, em uma única passada e em laço até digitar `-1`:
  - Remoção de chaves em nós folha;
  - Substituição por predecessor ou sucessor em nós internos;
  - Empréstimo de chaves dos nós irmãos (esquerdo ou direito);
  - Fusão de nós (*merge*), com exclusão do arquivo do nó absorvido;
  - Ajuste da raiz após a remoção (a altura diminui ou a árvore fica vazia);
- Busca de valores na árvore, exibindo o nó em que o valor foi encontrado;
- Suporte a chaves duplicadas (cada remoção elimina uma única ocorrência);
- Impressão visual da árvore, organizada por níveis e com conectores entre pai e filhos;
- Armazenamento de cada nó em um arquivo binário separado;
- Geração dinâmica de nomes para os arquivos dos nós;
- Gravação segura dos arquivos: cada escrita é feita em um arquivo temporário (`.tmp`) que depois substitui o original;
- Persistência dos metadados da árvore;
- Recuperação automática da estrutura ao iniciar o programa novamente;
- Alocação dinâmica de memória para as chaves e referências aos nós filhos.

## Estruturas de Dados Utilizadas

- **Árvore B:** estrutura de busca balanceada que mantém as chaves ordenadas e permite operações eficientes.
- **Arquivos binários:** armazenamento persistente dos nós da árvore.
- **Referências a arquivos:** nomes dos arquivos utilizados para localizar os nós filhos (o vetor de filhos é `char **`, não ponteiros).
- **Alocação dinâmica:** gerenciamento de chaves, referências e demais estruturas conforme o grau mínimo configurado.
- **Árvore de layout:** estrutura auxiliar temporária usada apenas para calcular o desenho da árvore na impressão.

## Armazenamento em Disco

O projeto utiliza o diretório `bin/` para armazenar os dados persistentes da árvore. O diretório é criado automaticamente caso não exista.

- `bin/node_<id>.bin`: arquivos binários individuais que representam os nós da árvore. O identificador vem de um contador global, então nenhum nome se repete.
- `bin/tree.meta`: arquivo de metadados contendo as informações necessárias para recuperar a estrutura: o grau mínimo, o próximo identificador de nó e a referência à raiz.

Os metadados são salvos ao final de cada operação do menu e também ao encerrar o programa, pois a raiz e o contador de identificadores podem mudar em inserções e remoções. Os arquivos de nós não são apagados ao sair.

Erros de leitura ou escrita em disco são considerados fatais: o programa mostra o motivo e encerra. Como cada arquivo é gravado em `.tmp` e depois substituído, uma falha no meio da escrita não corrompe um nó já existente. Porém, uma interrupção entre a gravação de dois nós de uma mesma operação pode deixar a árvore inconsistente.

**Importante:** mantenha o diretório `bin/` e seus arquivos para preservar os dados entre execuções. Para reiniciar o armazenamento, apague o diretório `bin/` **inteiro**. Apagar apenas o `tree.meta` faz o programa iniciar uma árvore nova e reutilizar identificadores de nós, sobrescrevendo os arquivos antigos.

## Como compilar

Com o compilador GCC instalado, execute o comando abaixo no diretório do projeto, substituindo `main.c` pelo nome do arquivo-fonte, caso seja diferente.

```bash
gcc main.c -o disk-b-tree
```

O código é compatível com Linux/macOS (POSIX) e com Windows (por exemplo, via MinGW).

## Como executar

Linux/macOS:

```bash
./disk-b-tree
```

Windows:

```bat
disk-b-tree.exe
```

Na primeira execução, o programa solicitará o grau mínimo `t` da árvore. São aceitos valores entre 2 e 1000. Entradas inválidas são descartadas e o valor é solicitado novamente.

Nas execuções seguintes, a árvore é recuperada automaticamente a partir de `bin/tree.meta`. Se esse arquivo existir mas estiver incompleto ou inválido, o programa informa o problema e encerra sem alterar nenhum arquivo, para não sobrescrever uma árvore recuperável.

## Como usar

Ao executar o programa, será exibido um menu com as seguintes opções:

1. Inserir valor:
   1. Inserir N valores aleatórios (informa-se N e o valor máximo);
   2. Inserir um único valor;
2. Remover valor (a árvore é impressa a cada passo; digite `-1` para sair);
3. Buscar um valor;
4. Imprimir a Árvore B;
0. Encerrar o programa.

As mensagens do programa são exibidas em inglês. Uma entrada não numérica nos submenus é descartada e a operação é cancelada; no menu principal, ela encerra o programa (salvando a árvore antes).

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
- Sistemas de arquivos Linux/POSIX e Windows

## Objetivos acadêmicos

O projeto tem como objetivo aplicar os conceitos de Árvores B estudados em Estruturas de Dados II, explorando algoritmos de busca, inserção e remoção, balanceamento da estrutura, alocação dinâmica e armazenamento de dados em memória secundária.

A implementação também permite estudar os desafios de representar e manipular estruturas de dados persistentes por meio de arquivos.

## Autores

- Eduardo de Araújo ALexandre