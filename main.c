#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define META_FILE  "bin/tree.meta"
#define META_MAGIC 0x42545245

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(p)  _mkdir(p)
    #define CLEAR()   system("cls")
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define MKDIR(p)  mkdir(p, 0755)
    #define CLEAR()   system("clear")
#endif

long nextId = 1;

/**
 * Estruturas da Arvore B
 */

typedef struct Node{
	int *keys, n, leaf;
	char **children; 
    char *filename;               
} Node;

typedef struct {
    char *root;
    int t;
} BTree;

// inicializa a estrutura da b tree com grau mínimo t
int initialize(BTree *bt, int t) {
    // se o ponteiro for nulo retorna erro
    if (bt == NULL) return 0;

    // grau mínimo da b tree é 2
    if (t < 2) return 0;

    // limite superior arbitrário para evitar valores absurdos
    if (t > 1000) return 0;

    // inicializa um b tree vazia, guarda t e retorna sucesso
    bt->root = NULL;
    bt->t = t;
    return 1;
}

/**
 * Assinaturas dos médotos/funções
 */

char *copyString(const char *str);
char* nominate();
int diskWrite(Node *node, int t);
Node* diskRead(const char *filename, int t);
int saveMeta(const BTree *bt);
int loadMeta(BTree *bt);
int startTree(BTree *bt);

int splitNode(BTree *bt, Node *dad, int i);
Node* createNode(int t, int leaf);
int insertNonFull(BTree *bt, const char *filename, int key);
int insert(BTree *bt, int key);

int findKeyIndex(Node *node, int key);
int getPred(BTree *bt, const char *filename, int *pred);
int getSucc(BTree *bt, const char *filename, int *succ);
int borrowFromPrev(BTree *bt, Node *node, int idx);
int borrowFromNext(BTree *bt, Node *node, int idx);
int merge(BTree *bt, Node *node, int idx);
int fill(BTree *bt, Node *node, int idx);
void removeFromLeaf(Node *node, int idx);
int removeFromInternal(BTree *bt, Node *node, int idx);
int removeFromNode(BTree *bt, const char *filename, int key);
int removeKey(BTree *bt, int key);

Node* search(BTree *bt, const char *filename, int k);

void nodeString(Node *node, char *buffer);
void printSpaces(int n);
void printBTree(BTree *bt);

void freeLoadedNode(Node *node, int t);
void freeBTree(BTree *bt);

/**
 * Funções de manipulação dos arquivos
 */

 // cria uma copia de uma string
char *copyString(const char *str){
    // se não houver o que copiar retorna nulo
    if(str == NULL) return NULL;

    // aloca strlen + 1 bytes (o +1 é para o \0)
    char *copy = malloc(strlen(str) + 1);

    // verifica se o malloc falhou
    if(copy == NULL) return NULL;

    // copia o conteúdo da string e retorna a copia
    strcpy(copy, str);
    return copy;
}

// gera o nome do arquivo de um novo nó no formato: bin/node_<id>.bin
// o id é sequencial e vem da variavel global nextId, sem risco de repeticao 
char* nominate(){
    // buffer temporário que comporta o nome do arquvio, 
    // + um long (até 20 digitos) e o sufixo .bin
    char buffer[64];

    // monta o nome com segurança, snprintf nunca escreve além do sizeof(buffer)
    // nextId++ utiliza o valor atual e depois incrementa, o próximo nó recebe um novo id
    snprintf(buffer, sizeof(buffer), "bin/node_%ld.bin", nextId++);

    // aloca dinamicamente o nome do arquivo
    char *filename = malloc(strlen(buffer) + 1);

    // se o malloc falhar, retorna nulo
    if(filename == NULL) return NULL;

    // copia o nome do buffer e retorna o nome do arquivo
    strcpy(filename, buffer);
    return filename;
}

// grava um nó em disco, no arquivo node->filename
// formato binário na seguinte ordem: 
// [n][leaf][keys: 2t-1 ints][para cada i em 0 até 2t-1: [exists][len][string]]
// escreve primeiro em um arquivo temporário e depois renomeia sobre o original
// nessa abordagem, uma falha no meio do caminho não gera um arquivo do nó corrompido 
int diskWrite(Node *node, int t){
    // sem nó e sem nome de arquivo não há onde garvar
    if(node == NULL || node->filename == NULL) return 0;

    // cria o arquivo temporário, nome original + .tmp
    size_t flen = strlen(node->filename);
    char *temporary = malloc(flen + 5); // 4 de .tmp + 1 de \0
    if(temporary == NULL) return 0; // verifica se falhou
    sprintf(temporary, "%s.tmp", node->filename);

    // wb para escrita binária, trunca o temporário caso já exista
    FILE *file = fopen(temporary, "wb");

    // verifica se foi gerado com sucesso
    if(file == NULL){
        free(temporary); // libera o unico recurso alocado
        return 0;
    }

    // um nó guarda até 2t-1 chaves
    // o cast para size_t evita overflow em int
    size_t nkeys = 2 * (size_t) t - 1;
    int ok = 1; // flag de erro

    // escrita e verificações
    // quantidade de chaves em uso
    if(fwrite(&node->n, sizeof(int), 1, file) != 1)
        ok = 0;

    // se é folha = 1, se não = 0
    if(ok && fwrite(&node->leaf, sizeof(int), 1, file) != 1)
        ok = 0;

    // as chaves (2t-1), mesmo as não usadas
    // assim o registro tem o tamanho de chaves fixo e 
    // + o diskRead lê sempre a mesma quantidade
    if(ok && fwrite(node->keys, sizeof(int), nkeys, file) != nkeys)
        ok = 0;

    // percorre 2t posições, para os filhos
    for(int i = 0; ok && i < 2 * t; i++){
        // flag 1 se a posição i tem filho, se não 0
        int exists = node->children[i] != NULL;

        if(fwrite(&exists, sizeof(int), 1, file) != 1){
            ok = 0;
            break;
        }

        if(exists){
            // tamanho da string incluindo o \0
            size_t len = strlen(node->children[i]) + 1;

            // grava primeiro o tamanho e depois os bytes do nome
            if(fwrite(&len, sizeof(size_t), 1, file) != 1 ||
               fwrite(node->children[i], sizeof(char), len, file) != len){
                ok = 0;
            }
        }
    }

    // fclose descarrega o buffer, erros de escrita podem aparecer aqui,
    // + (disco cheio por exemplo) por isso o retorno deve ser verificado
    if(fclose(file) != 0)
        ok = 0;

    // o temporário sibstitui o definitivo
    if(ok && rename(temporary, node->filename) != 0)
        ok = 0;

    // se algo falhou, apaga o temporario para não deixar erros e bin/
    if(!ok)
        remove(temporary);

    free(temporary);
    return ok;
}

// lê um nó no arquivo e retorna montado na memória
// lê exatamente o formato produzido por diskWrite e valida cada campo
// + para evitar arquivos corrompidos ou truncados
Node* diskRead(const char *filename, int t){
    // se o arquivo não existe não há oque ler
    if(filename == NULL) return NULL;

    // rb para leitura binária
    FILE *file = fopen(filename, "rb");
    if(file == NULL) return NULL;

    // aloca a struct do nó
    Node *node = malloc(sizeof(Node));

    // verifica o malloc
    if(node == NULL){
        fclose(file);
        return NULL;
    }

    // aloca os três blocos internos
    // + nome, array de chaves, array de ponteiros filhos
    // calloc zera os ponteiros filhos, as não preenchidas são nulas
    node->filename = malloc(strlen(filename) + 1);
    node->keys = malloc((2 * t - 1) * sizeof(int));
    node->children = calloc(2 * t, sizeof(char *));

    // se qualquer alocação falhou, libera as demais e retorna
    if(node->filename == NULL || node->keys == NULL || node->children == NULL){
        free(node->filename);
        free(node->keys);
        free(node->children);
        free(node);
        fclose(file);

        return NULL;
    }

    // guarda o nome do arquivo de onde veio o nó
    strcpy(node->filename, filename);

    size_t nkeys = 2 * (size_t) t - 1;
    int ok = 1; // mesma estratégia de flag acumulada do diskWrite

    // lê a quantidade de chaves
    if(fread(&node->n, sizeof(int), 1, file) != 1)
        ok = 0;

    // valida se n não é negativo ou maior que 2t-1
    if(ok && (node->n < 0 || node->n > 2 * t - 1))
        ok = 0;

    // lê o indicador da folha
    if(ok && fread(&node->leaf, sizeof(int), 1, file) != 1)
        ok = 0;

    // valida, valor deve ser 0 ou 1
    if(ok && node->leaf != 0 && node->leaf != 1)
        ok = 0;

    // lê as 2t-1 chaves de uma vez (inclusive as posições vazias)
    if(ok && fread(node->keys, sizeof(int), nkeys, file) != nkeys)
        ok = 0;

    // lê as 2t posições de filhos
    for(int i = 0; ok && i < 2 * t; i++){
        int exists;
        size_t len;
    
        // flag de existência do filho i
        if(fread(&exists, sizeof(int), 1, file) != 1){
            ok = 0;
            break;
        }

        // valida a flag, só 0 ou 1
        if(exists != 0 && exists != 1){
            ok = 0;
            break;
        }

        // validação estrutural, um nó interno com n chaves tem n+1 filhos
        // a expressão direita vale 1 só quando o filho i deveria existir
        // se o arquivo discordar, ele está inconsistente
        if(exists != (!node->leaf && i <= node->n)){
            ok = 0;
            break;
        }

        if(exists){
            // lê o tamanho da string, rejeita 0 (vazia) e valores absurdos que 
            // + indicariam corrupção e poder causar uma alocação gigante
            if(fread(&len, sizeof(size_t), 1, file) != 1 || len == 0 || len > 4096){
                ok = 0;
                break;
            }

            node->children[i] = malloc(len);

            if(node->children[i] == NULL){
                ok = 0;
                break;
            }

            // lê bytes do nome do arquivo filho
            if(fread(node->children[i], sizeof(char), len, file) != len){
                ok = 0;
                break;
            }

            // garante que a string termina com \0
            if(node->children[i][len - 1] != '\0'){
                ok = 0;
                break;
            }
        }
    }

    // fecha o arquivo, falhar aqui invalida a leitura
    if(fclose(file) != 0)
        ok = 0;

    // se a flag ok acusa um erro, libera memória e retorna
    if(!ok){
        for(int i = 0; i < 2 * t; i++)
            free(node->children[i]);

        free(node->children);
        free(node->keys);
        free(node->filename);
        free(node);

        return NULL;
    }

    return node;
}

// salva os metadados da arvore em bin/tree.meta
// formato:
// [magic][t][nextId][len][nome do arquivo raiz com \0]
// grava em um .tmp e depois renomeia, como diskWrite
int saveMeta(const BTree *bt){
    // t < 2 indica uma arvore nunca inicializada
    if(bt == NULL || bt->t < 2) return 0;

    // concatena os literais bin/tree.meta.tmp
    const char *temporary = META_FILE ".tmp";
    FILE *f = fopen(temporary, "wb");

    if(f == NULL) return 0;

    int magic = META_MAGIC; // assinatura para reconhecer os tipos de arquivo
    int t = bt->t;
    long next = nextId; // copia o da global para os ids não começarem em 1

    // tamanho do nome da raiz incluindo o \0, 0 se a arvore está vaiza
    size_t len = (bt->root != NULL) ? strlen(bt->root) + 1 : 0;

    int ok = 1; // flag acumulada, pula após o primeiro erro

    if(fwrite(&magic, sizeof(int), 1, f) != 1) ok = 0;
    if(ok && fwrite(&t, sizeof(int), 1, f) != 1) ok = 0;
    if(ok && fwrite(&next, sizeof(long), 1, f) != 1) ok = 0;
    if(ok && fwrite(&len, sizeof(size_t), 1, f) != 1) ok = 0;

    // só grava a string se a raiz existir
    if(ok && len > 0 && fwrite(bt->root, sizeof(char), len, f) != len) ok = 0;

    // erros de escrita podem aparecer aqui
    if(fclose(f) != 0) ok = 0;

    // troca o metadado antigo pelo novo
    if(ok && rename(temporary, META_FILE) != 0) ok = 0;

    // se algo falhou, remove o temporario
    if(!ok) remove(temporary);

    return ok;
}

// lê bin/tree.meta e restaura a arvore, (t, raiz e nextId)
// em caso de erro não altera bt nem nextId
int loadMeta(BTree *bt){
    if(bt == NULL) return -1;

    FILE *f = fopen(META_FILE, "rb");

    // distingue não existe (ENOENT) de outros erros
    if(f == NULL) return (errno == ENOENT) ? 0 : -1;

    int magic, t;
    long next;
    size_t len;
    char *root = NULL;
    int ok = 1;

    // cada campo é lido e validado na mesma linha
    // magic confere se é mesmo um arquivo de metadados dessa arvore
    if(fread(&magic, sizeof(int), 1, f) != 1 || magic != META_MAGIC) ok = 0;

    // t dentro da mesma faixa aceitada por initialize
    if(ok && (fread(&t, sizeof(int), 1, f) != 1 || t < 2 || t > 1000)) ok = 0;

    // ids começam em 1, valor menor indica erro
    if(ok && (fread(&next, sizeof(long), 1, f) != 1 || next < 1)) ok = 0;

    // 4096 evita alocação gigante por um len corrompido
    if(ok && (fread(&len, sizeof(size_t), 1, f) != 1 || len > 4096)) ok = 0;

    // len = 0 significa arvore vazia, pula e deixa root = NULL
    if(ok && len > 0){
        root = malloc(len);

        // falha de malloc, leitura incompleta ou falta de \0 no final da string
        if(root == NULL || fread(root, sizeof(char), len, f) != len || root[len - 1] != '\0')
            ok = 0;
    }

    // arquivo é só leitura, então o retorno de fclose não é verificado
    fclose(f);

    // verifica se a raiz existe de fato ao tentar ler o nó
    // detecta o caso de meta apontando para um arquivo apagado
    if(ok && root != NULL){
        Node *check = diskRead(root, t);

        if(check == NULL) ok = 0;
        else freeLoadedNode(check, t); // libera a memória
    }

    // verifica flag de erro e libera a memória em caso de falha
    if(!ok){
        free(root);
        return -1;
    }

    // só aqui, com tudo validado, o programa é modificado
    bt->t = t;
    bt->root = root;
    nextId = next;

    return 1;
}

// prepara a arvore ao iniciar o programa, recupera do disco se houver
// + se não, gera uma nova arvore com t fornecido pelo usuario
int startTree(BTree *bt){
    int r = loadMeta(bt);

    // meta corrompido, não segue para não sobrescrever uma arvore
    // o usuario pode tentar corrigir ou apagar bin/
    if(r < 0){
        printf("bin/tree.meta exists but is invalid or points to a missing node.\n");
        printf("Fix it or delete the bin/ directory to start over.\n");
        return 0;
    }

    // arvore recuperada, t já foi carregado
    if(r > 0){
        printf("B Tree recovered from disk (t = %d).\n", bt->t);
        return 1;
    }

    // r = 0, primeira execução, pergunta o t
    int t = 0;

    while (1) {
        printf("Define value t of B Tree: ");

        // scanf devolve quantos itens lei, != 1 significa entra não numérica
        if (scanf("%d", &t) != 1) {
            printf("Invalid input\n");

            // descarta o resto da linha para scanf não travar relendo lixo
            int c;
                    
            continue;
        }

        // entrada válida, descarta o \n que sobrou no buffer
        int c;
                

        // initialize valida a faixa, se rejeitar pergunta de novo
        if (!initialize(bt, t)) {
            printf("Value of t must be between 2 and 1000\n\n");
            continue;
        }
        break;
    }

    printf("B Tree initialized with t = %d.\n", bt->t);
    return 1;
}

/**
 * Funções de inserção da estrutura
 */

// divide o filho cheio dad->children[i] em dois nós com t-1 chaves cada
// a chave do meio (promoted) sobe para o pai e o novo nó vira children[i+1]
// pré-condições: o pai não pode estar cheio e o filho deve ter exatamente 2t-1 chaves
// retorna 1 em caso de sucesso e 0 em caso de erro
int splitNode(BTree *bt, Node *dad, int i){
    // valida ponteiros, o índice do filho (0..n) e se o pai tem espaço para mais uma chave
    if(bt == NULL || dad == NULL || i < 0 || i > dad->n || dad->n >= 2 * bt->t - 1) return 0;

    int t = bt->t;

    // lê o filho cheio do disco
    Node *child = diskRead(dad->children[i], t);
    if(child == NULL) return 0;

    // só faz sentido dividir um nó cheio
    if(child->n != 2 * t - 1){
        freeLoadedNode(child, t);
        return 0;
    }

    // cria o nó irmão, do mesmo tipo do filho (folha ou interno)
    // o nó ainda não está em disco, createNode só reserva o nome do arquivo
    Node *newn = createNode(t, child->leaf);
    if(newn == NULL){
        freeLoadedNode(child, t);
        return 0;
    }

    // cópia do nome do novo nó, que passa a pertencer ao pai
    char *newName = copyString(newn->filename);
    if(newName == NULL){
        freeLoadedNode(child, t);
        freeLoadedNode(newn, t);
        return 0;
    }

    // chave do meio, que vai subir para o pai
    int promoted = child->keys[t - 1];

    // as t-1 últimas chaves do filho vão para o novo nó
    for(int j = 0; j < t - 1; j++)
        newn->keys[j] = child->keys[j + t];

    newn->n = t - 1;

    // nó interno também transfere os t últimos filhos
    // o ponteiro é anulado na origem para não haver double free
    if(!child->leaf){
        for(int j = 0; j < t; j++){
            newn->children[j] = child->children[j + t];
            child->children[j + t] = NULL;
        }
    }

    // o filho fica só com as t-1 primeiras chaves
    // as chaves antigas além de n ficam como lixo no array, mas são ignoradas
    child->n = t - 1;

    // abre espaço no pai para o novo filho na posição i+1
    for(int j = dad->n; j > i; j--)
        dad->children[j + 1] = dad->children[j];

    dad->children[i + 1] = newName;

    // abre espaço no pai para a chave promovida na posição i
    for(int j = dad->n; j > i; j--)
        dad->keys[j] = dad->keys[j - 1];

    dad->keys[i] = promoted;
    dad->n++;

    // grava os três nós modificados
    // o novo nó vai primeiro para que o pai nunca aponte para um arquivo inexistente
    int ok = diskWrite(newn, t);
    if(ok) ok = diskWrite(dad, t);
    if(ok) ok = diskWrite(child, t);

    // as cópias em memória não são mais necessárias
    freeLoadedNode(child, t);
    freeLoadedNode(newn, t);

    return ok;
}

// cria um nó vazio em memória, sem gravar em disco
// só reserva o nome do arquivo (nominate), quem grava é o diskWrite
// retorna NULL se faltar memória
Node* createNode(int t, int leaf){
    Node *newn = malloc(sizeof(Node));

    if(newn == NULL) return NULL;

    newn->n = 0;
    newn->leaf = leaf;

    // calloc zera as chaves e deixa todos os ponteiros de filhos nulos
    newn->keys = calloc(2 * t - 1, sizeof(int));
    newn->children = calloc(2 * t, sizeof(char*));

    newn->filename = nominate();

    // se qualquer alocação falhou, libera as demais (free(NULL) é permitido)
    if(newn->keys == NULL || newn->children == NULL || newn->filename == NULL){
        free(newn->keys);
        free(newn->children);
        free(newn->filename);
        free(newn);

        return NULL;
    }

    return newn;
}

// insere a chave na subárvore cujo nó está em filename
// pré-condição: esse nó NÃO está cheio
// desce da raiz até a folha e divide os filhos cheios no caminho (split preventivo),
// assim nunca é preciso voltar para cima
int insertNonFull(BTree *bt, const char *filename, int key) {
    if(bt == NULL || filename == NULL) return 0;

    int t = bt->t;
    Node *node = diskRead(filename, t);

    if(node == NULL) return 0;

    int i = node->n - 1;

    // caso base: folha, insere a chave na posição ordenada
    if(node->leaf){
        // desloca as chaves maiores uma posição para a direita
        while(i >= 0 && key < node->keys[i]){
            node->keys[i + 1] = node->keys[i];
            i--;
        }

        node->keys[i + 1] = key;
        node->n++;

        int ok = diskWrite(node, t);
        freeLoadedNode(node, t);

        return ok;
    }

    // nó interno: acha o filho por onde descer
    // para na primeira chave >= key, então chave igual desce pelo filho da esquerda
    i = 0;
    while(i < node->n && key > node->keys[i])
        i++;

    // filho inexistente indica nó inconsistente
    if(node->children[i] == NULL){
        freeLoadedNode(node, t);
        return 0;
    }

    // lê o filho só para saber se está cheio
    Node *child = diskRead(node->children[i], t);

    if(child == NULL){
        freeLoadedNode(node, t);
        return 0;
    }

    int childFull = (child->n == 2 * t - 1);
    freeLoadedNode(child, t);

    if(childFull){
        // divide o filho, o nó em memória (node) é atualizado e gravado pelo splitNode
        if(!splitNode(bt, node, i)){
            freeLoadedNode(node, t);
            return 0;
        }

        // a chave promovida ficou em node->keys[i]
        // se key for maior, o destino agora é o novo nó (i+1)
        if(key > node->keys[i])
            i++;
    }

    // copia o nome do filho antes de liberar o nó, pois o nome pertence a ele
    char *childFilename = copyString(node->children[i]);

    freeLoadedNode(node, t);

    if(childFilename == NULL) return 0;

    // desce recursivamente, o filho escolhido com certeza não está cheio
    int ok = insertNonFull(bt, childFilename, key);
    free(childFilename);

    return ok;
}

// insere uma chave na árvore
// trata dois casos especiais (árvore vazia e raiz cheia) e delega o resto ao insertNonFull
int insert(BTree *bt, int key) {
    if(bt == NULL || bt->t < 2) return 0;

    int t = bt->t;

    // árvore vazia: cria a raiz como uma folha com a primeira chave
    if(bt->root == NULL){
        Node *root = createNode(t, 1);
        if(root == NULL) return 0;

        root->keys[0] = key;
        root->n = 1;

        int ok = diskWrite(root, t);

        // só registra a raiz na árvore se a gravação deu certo
        if(ok){
            bt->root = copyString(root->filename);

            if(bt->root == NULL)
                ok = 0;
        }

        freeLoadedNode(root, t);
        return ok;
    }

    // lê a raiz só para saber se está cheia
    Node *root = diskRead(bt->root, t);
    if(root == NULL) return 0;

    int rootFull = (root->n == 2 * t - 1);
    freeLoadedNode(root, t);

    // raiz com espaço: caso comum
    if(!rootFull)
        return insertNonFull(bt, bt->root, key);

    // raiz cheia: a árvore cresce em altura
    // cria uma nova raiz (interna, sem chaves) com a raiz antiga como único filho
    Node *newRoot = createNode(t, 0);
    if(newRoot == NULL) return 0;

    newRoot->children[0] = copyString(bt->root);

    if(newRoot->children[0] == NULL){
        freeLoadedNode(newRoot, t);
        return 0;
    }

    // divide a raiz antiga, a chave do meio sobe para a nova raiz
    // o splitNode também grava a nova raiz em disco
    if(!splitNode(bt, newRoot, 0)){
        freeLoadedNode(newRoot, t);
        return 0;
    }

    char *newRootFilename = copyString(newRoot->filename);

    if(newRootFilename == NULL){
        freeLoadedNode(newRoot, t);
        return 0;
    }

    // troca a raiz da árvore para a nova
    free(bt->root);
    bt->root = newRootFilename;

    // escolhe o lado da nova raiz por onde a chave desce
    int i = (key > newRoot->keys[0]) ? 1 : 0;

    char *childFilename = copyString(newRoot->children[i]);

    freeLoadedNode(newRoot, t);

    if (childFilename == NULL) return 0;

    // os dois filhos agora têm t-1 chaves, então a inserção é segura
    int ok = insertNonFull(bt, childFilename, key);
    free(childFilename);

    return ok;
}

/**
 * Funções de remoção da estrutura
 */

// retorna o índice da primeira chave do nó que seja >= key
// se todas forem menores, retorna node->n
// serve para dois casos: a chave está em keys[idx], ou o filho por onde descer é children[idx]
int findKeyIndex(Node *node, int key){
    int idx = 0;
    while(idx < node->n && node->keys[idx] < key)
        idx++;

    return idx;
}

// obtém o predecessor: a maior chave da subárvore em filename
// desce sempre pelo último filho até chegar numa folha e pega a última chave dela
// o resultado é devolvido em *pred, retorna 1 em sucesso e 0 em erro
int getPred(BTree *bt, const char *filename, int *pred){
    if(bt == NULL || filename == NULL || pred == NULL) return 0;

    int t = bt->t;
    Node *cur = diskRead(filename, t);

    // desce pelo filho mais à direita (children[n]) até uma folha
    while(cur != NULL && !cur->leaf){
        // copia o nome antes de liberar o nó, pois o nome pertence a ele
        char *next = copyString(cur->children[cur->n]);
        freeLoadedNode(cur, t);

        if(next == NULL) return 0;

        cur = diskRead(next, t);
        free(next);
    }

    // falha de leitura em algum ponto da descida
    if(cur == NULL) return 0;

    // folha vazia seria inconsistente, não há chave para devolver
    if(cur->n == 0){
        freeLoadedNode(cur, t);
        return 0;
    }

    // a maior chave da folha é a última
    *pred = cur->keys[cur->n - 1];
    freeLoadedNode(cur, t);

    return 1;
}

// obtém o sucessor: a menor chave da subárvore em filename
// simétrico ao getPred, desce sempre pelo primeiro filho
int getSucc(BTree *bt, const char *filename, int *succ){
    if(bt == NULL || filename == NULL || succ == NULL) return 0;

    int t = bt->t;
    Node *cur = diskRead(filename, t);

    // desce pelo filho mais à esquerda (children[0]) até uma folha
    while(cur != NULL && !cur->leaf){
        char *next = copyString(cur->children[0]);
        freeLoadedNode(cur, t);

        if(next == NULL) return 0;

        cur = diskRead(next, t);
        free(next);
    }

    if(cur == NULL) return 0;

    if(cur->n == 0){
        freeLoadedNode(cur, t);
        return 0;
    }

    // a menor chave da folha é a primeira
    *succ = cur->keys[0];
    freeLoadedNode(cur, t);

    return 1;
}

// empresta uma chave do irmão da esquerda para o filho node->children[idx]
// a última chave do irmão sobe para o pai e a chave do pai desce para o início do filho
// pré-condição: o irmão tem pelo menos t chaves
int borrowFromPrev(BTree *bt, Node *node, int idx){
    // idx >= 1 porque o primeiro filho não tem irmão à esquerda
    if(bt == NULL || node == NULL || idx < 1 || idx > node->n) return 0;

    int t = bt->t;

    Node *child = diskRead(node->children[idx], t);
    Node *sibling = diskRead(node->children[idx - 1], t);

    // free(NULL) é permitido, então libera o que tiver sido lido
    if(child == NULL || sibling == NULL){
        freeLoadedNode(child, t);
        freeLoadedNode(sibling, t);
        return 0;
    }

    // abre espaço no início das chaves do filho
    for(int j = child->n - 1; j >= 0; j--)
        child->keys[j + 1] = child->keys[j];

    // nó interno também desloca os filhos e recebe o último filho do irmão
    if(!child->leaf){
        for(int j = child->n; j >= 0; j--)
            child->children[j + 1] = child->children[j];

        // o nome passa do irmão para o filho, anula a origem para não dar double free
        child->children[0] = sibling->children[sibling->n];
        sibling->children[sibling->n] = NULL;
    }

    // a chave separadora do pai desce para o filho
    // a última chave do irmão sobe para ocupar o lugar dela no pai
    child->keys[0] = node->keys[idx - 1];
    node->keys[idx - 1] = sibling->keys[sibling->n - 1];

    child->n++;
    sibling->n--;

    // grava os dois nós alterados
    int ok = diskWrite(child, t);
    if(ok) ok = diskWrite(sibling, t);

    freeLoadedNode(child, t);
    freeLoadedNode(sibling, t);

    return ok;
}

// empresta uma chave do irmão da direita para o filho node->children[idx]
// a primeira chave do irmão sobe para o pai e a chave do pai desce para o fim do filho
// pré-condição: o irmão tem pelo menos t chaves
int borrowFromNext(BTree *bt, Node *node, int idx){
    // idx < n porque o último filho não tem irmão à direita
    if(bt == NULL || node == NULL || idx < 0 || idx >= node->n) return 0;

    int t = bt->t;

    Node *child = diskRead(node->children[idx], t);
    Node *sibling = diskRead(node->children[idx + 1], t);

    if(child == NULL || sibling == NULL){
        freeLoadedNode(child, t);
        freeLoadedNode(sibling, t);
        return 0;
    }

    // a chave separadora do pai desce para o fim do filho
    child->keys[child->n] = node->keys[idx];

    // nó interno recebe o primeiro filho do irmão
    if(!child->leaf){
        child->children[child->n + 1] = sibling->children[0];
    }

    // a primeira chave do irmão sobe para o pai
    node->keys[idx] = sibling->keys[0];

    // fecha o buraco deixado no início das chaves do irmão
    for(int j = 1; j < sibling->n; j++)
        sibling->keys[j - 1] = sibling->keys[j];

    // nó interno também fecha o buraco nos filhos
    // o ponteiro antigo de children[0] já pertence ao filho, então não há vazamento
    if(!sibling->leaf){
        for(int j = 1; j <= sibling->n; j++)
            sibling->children[j - 1] = sibling->children[j];

        sibling->children[sibling->n] = NULL;
    }

    child->n++;
    sibling->n--;

    int ok = diskWrite(child, t);
    if(ok) ok = diskWrite(sibling, t);

    freeLoadedNode(child, t);
    freeLoadedNode(sibling, t);

    return ok;
}

// funde node->children[idx] com node->children[idx+1] em um único nó
// a chave separadora do pai desce para o meio: [filho][chave do pai][irmão]
// pré-condição: os dois nós têm exatamente t-1 chaves, o resultado fica com 2t-1
// o irmão deixa de existir (arquivo apagado), o pai perde uma chave e um filho
int merge(BTree *bt, Node *node, int idx){
    // idx < n porque precisa existir um irmão à direita
    if(bt == NULL || node == NULL || idx < 0 || idx >= node->n) return 0;

    int t = bt->t;

    Node *child = diskRead(node->children[idx], t);
    Node *sibling = diskRead(node->children[idx + 1], t);

    if(child == NULL || sibling == NULL){
        freeLoadedNode(child, t);
        freeLoadedNode(sibling, t);
        return 0;
    }

    // a chave do pai desce para a posição t-1 do filho (logo após suas t-1 chaves)
    child->keys[t - 1] = node->keys[idx];

    // copia as chaves do irmão logo em seguida
    for(int j = 0; j < sibling->n; j++)
        child->keys[j + t] = sibling->keys[j];

    // nó interno também recebe os filhos do irmão
    // o ponteiro é anulado na origem para evitar double free
    if(!child->leaf){
        for(int j = 0; j <= sibling->n; j++){
            child->children[j + t] = sibling->children[j];
            sibling->children[j] = NULL;
        }
    }

    // o nome do irmão guardado no pai não serve mais, então é liberado
    free(node->children[idx + 1]);
    node->children[idx + 1] = NULL;

    // fecha o buraco no pai, deslocando chaves e filhos para a esquerda
    for(int j = idx + 1; j < node->n; j++)
        node->keys[j - 1] = node->keys[j];

    for(int j = idx + 2; j <= node->n; j++)
        node->children[j - 1] = node->children[j];

    // a última posição de filho ficou duplicada, anula
    node->children[node->n] = NULL;

    // as t-1 chaves do filho + 1 do pai + as do irmão
    child->n += sibling->n + 1;
    node->n--;

    // grava o filho fundido e depois apaga o arquivo do irmão
    int ok = diskWrite(child, t);

    if(ok && remove(sibling->filename) != 0)
        ok = 0;

    freeLoadedNode(child, t);
    freeLoadedNode(sibling, t);

    return ok;
}

// garante que o filho node->children[idx] tenha pelo menos t chaves
// tenta nesta ordem: emprestar da esquerda, emprestar da direita, fundir
int fill(BTree *bt, Node *node, int idx){
    if(bt == NULL || node == NULL || idx < 0 || idx > node->n) return 0;

    int t = bt->t;

    // tem irmão à esquerda: se ele tem chaves sobrando, empresta dele
    if(idx != 0){
        Node *prev = diskRead(node->children[idx - 1], t);
        if(prev == NULL) return 0;

        int enough = (prev->n >= t);
        freeLoadedNode(prev, t);

        if(enough) return borrowFromPrev(bt, node, idx);
    }

    // tem irmão à direita: se ele tem chaves sobrando, empresta dele
    if(idx != node->n){
        Node *next = diskRead(node->children[idx + 1], t);
        if(next == NULL) return 0;

        int enough = (next->n >= t);
        freeLoadedNode(next, t);

        if(enough) return borrowFromNext(bt, node, idx);
    }

    // nenhum irmão pode emprestar, então funde
    // com o irmão da direita se existir, senão com o da esquerda
    // na fusão com a esquerda o filho passa a ser idx - 1
    if(idx != node->n)
        return merge(bt, node, idx);

    return merge(bt, node, idx - 1);
}

// remove a chave na posição idx de uma folha, deslocando as seguintes para a esquerda
void removeFromLeaf(Node *node, int idx){
    for(int j = idx + 1; j < node->n; j++)
        node->keys[j - 1] = node->keys[j];

    node->n--;
}

// remove a chave node->keys[idx] de um nó interno, em três casos:
// 1) filho da esquerda com >= t chaves: troca pelo predecessor e remove ele da esquerda
// 2) filho da direita com >= t chaves: troca pelo sucessor e remove ele da direita
// 3) os dois com t-1 chaves: funde e remove a chave do nó resultante
// diferente das outras, essa função grava o próprio nó (node) antes de descer
int removeFromInternal(BTree *bt, Node *node, int idx){
    if(bt == NULL || node == NULL || node->leaf || idx < 0 || idx >= node->n) return 0;

    int t = bt->t;

    // guarda a chave original, pois keys[idx] pode ser sobrescrita
    int k = node->keys[idx];

    // caso 1: filho da esquerda tem chaves sobrando
    Node *left = diskRead(node->children[idx], t);
    if(left == NULL) return 0;

    int leftEnough = (left->n >= t);
    freeLoadedNode(left, t);

    if(leftEnough){
        int pred;

        if(!getPred(bt, node->children[idx], &pred)) return 0;

        // o predecessor ocupa o lugar de k, que some da árvore
        node->keys[idx] = pred;

        // grava antes de descer, senão a troca se perderia
        if(!diskWrite(node, t)) return 0;

        // agora é preciso remover o predecessor de onde ele estava (na folha)
        return removeFromNode(bt, node->children[idx], pred);
    }

    // caso 2: filho da direita tem chaves sobrando
    Node *right = diskRead(node->children[idx + 1], t);
    if(right == NULL) return 0;

    int rightEnough = (right->n >= t);
    freeLoadedNode(right, t);

    if(rightEnough){
        int succ;

        if(!getSucc(bt, node->children[idx + 1], &succ)) return 0;

        node->keys[idx] = succ;

        if(!diskWrite(node, t)) return 0;

        return removeFromNode(bt, node->children[idx + 1], succ);
    }

    // caso 3: os dois filhos têm t-1 chaves, então funde
    // k desce para o nó fundido, onde agora é removido
    if(!merge(bt, node, idx)) return 0;

    // grava o pai, que perdeu uma chave e um filho no merge
    if(!diskWrite(node, t)) return 0;

    return removeFromNode(bt, node->children[idx], k);
}

// remove key da subárvore cujo nó está em filename
// desce da raiz e garante que todo filho em que entra tenha pelo menos t chaves
// (fill preventivo), assim a remoção nunca precisa voltar para cima
int removeFromNode(BTree *bt, const char *filename, int key){
    if(bt == NULL || filename == NULL) return 0;

    int t = bt->t;
    Node *node = diskRead(filename, t);

    if(node == NULL) return 0;

    int idx = findKeyIndex(node, key);
    int ok;

    // a chave está neste nó
    if(idx < node->n && node->keys[idx] == key){
        if(node->leaf){
            removeFromLeaf(node, idx);
            ok = diskWrite(node, t);
        } else{
            // nó interno: removeFromInternal grava o nó antes de descer
            ok = removeFromInternal(bt, node, idx);
        }

        freeLoadedNode(node, t);
        return ok;
    }

    // a chave não está aqui e não há para onde descer
    if(node->leaf){
        freeLoadedNode(node, t);
        return 1; // chave inexistente, nada a fazer
    }

    // a chave, se existir, está na subárvore children[idx]
    Node *child = diskRead(node->children[idx], t);

    if(child == NULL){
        freeLoadedNode(node, t);
        return 0;
    }

    // filho com t-1 chaves não pode perder nenhuma, precisa ser reforçado antes de descer
    int needFill = (child->n < t);
    freeLoadedNode(child, t);

    if(needFill){
        // fill altera o pai em memória, então o pai precisa ser gravado depois
        if(!fill(bt, node, idx) || !diskWrite(node, t)){
            freeLoadedNode(node, t);
            return 0;
        }

        // se houve fusão com o irmão da esquerda, o pai perdeu uma chave
        // e o filho passou a ser o de índice idx - 1
        if(idx > node->n)
            idx--;
    }

    // copia o nome antes de liberar o nó, pois o nome pertence a ele
    char *childFilename = copyString(node->children[idx]);

    freeLoadedNode(node, t);

    if(childFilename == NULL) return 0;

    // desce recursivamente, o filho escolhido tem pelo menos t chaves
    ok = removeFromNode(bt, childFilename, key);
    free(childFilename);

    return ok;
}

// remove uma chave da árvore
// delega a remoção ao removeFromNode e depois trata a raiz, que pode ter ficado vazia
int removeKey(BTree *bt, int key){
    if(bt == NULL) return 0;

    // árvore vazia, nada a remover
    if(bt->root == NULL) return 1;

    int t = bt->t;

    if(!removeFromNode(bt, bt->root, key)) return 0;

    // relê a raiz, pois ela pode ter mudado em disco
    Node *root = diskRead(bt->root, t);
    if(root == NULL) return 0;

    // raiz ainda tem chaves, está tudo certo
    if(root->n > 0){
        freeLoadedNode(root, t);
        return 1;
    }

    // raiz vazia: a árvore diminui de altura
    // se a raiz era interna, o único filho (children[0]) vira a nova raiz
    // se era folha, a árvore ficou vazia e a nova raiz é NULL
    char *newRoot = NULL;

    if(!root->leaf){
        newRoot = copyString(root->children[0]);

        if(newRoot == NULL){
            freeLoadedNode(root, t);
            return 0;
        }
    }

    // apaga o arquivo da raiz antiga
    int ok = (remove(root->filename) == 0);

    freeLoadedNode(root, t);

    // troca a raiz da árvore, NULL se ficou vazia
    free(bt->root);
    bt->root = newRoot;

    return ok;
}

/**
 * Funções de busca da estrutura
 */

// busca a chave k na subárvore cujo nó está em filename
// se encontrar, retorna o nó (cópia lida do disco) onde a chave está, senão retorna NULL
Node* search(BTree *bt, const char *filename, int k){
    if(bt == NULL || filename == NULL) return NULL;

    int t = bt->t;

    // lê o nó atual do disco
    Node *node = diskRead(filename, t);

    if(node == NULL) return NULL;

    int i = 0;

    // acha a primeira chave >= k
    // (mesma lógica do findKeyIndex, escrita aqui com a comparação invertida)
    while(i < node->n && k > node->keys[i])
        i++;

    // encontrou: devolve o nó sem liberar, a posse passa para o chamador
    if(i < node->n && node->keys[i] == k) return node;

    // não achou e é folha: não há para onde descer, a chave não existe
    if(node->leaf){
        freeLoadedNode(node, t);
        return NULL;
    }

    // a chave, se existir, está na subárvore children[i]
    // copia o nome antes de liberar o nó, pois o nome pertence a ele
    char *childFilename = copyString(node->children[i]);

    // libera o nó atual antes de descer, então só um nó fica em memória por vez
    freeLoadedNode(node, t);

    if(childFilename == NULL) return NULL;

    // desce recursivamente e repassa o resultado para cima
    Node *result = search(bt, childFilename, k);
    free(childFilename);

    return result;
}

/**
 * Funções de impressão da estrutura
 */

// espaçamento mínimo, em colunas, entre subárvores irmãs na impressão
#define GAP 2

// monta em buffer o texto de um nó no formato "| a b c |"
// o buffer deve ter espaço suficiente: cada chave ocupa até 12 caracteres (sinal, 10 dígitos e espaço)
// quem chama (buildLayout) aloca n * 12 + 8 bytes, que comporta tudo
void nodeString(Node *node, char *buffer){
    // zera o buffer para o strcat começar do início
    buffer[0] = '\0';
    strcat(buffer, "| ");
    for(int i = 0; i < node->n; i++){
        // temp comporta o maior int ("-2147483648 ") mais o \0
        char temp[16];
        sprintf(temp, "%d ", node->keys[i]);
        strcat(buffer, temp);
    }
    strcat(buffer, "|");
}

// imprime n espaços, usado para alinhar as linhas de conectores
void printSpaces(int n){
    for(int i = 0; i < n; i++) printf(" ");
}

// o desenho é feito em 3 fases: monta uma árvore de layout (LNode) paralela à árvore em disco,
// calcula a coluna de cada nó e desenha tudo em uma matriz de caracteres (grid) que depois é impressa
// a árvore em disco é lida uma vez só (na fase 1), depois só se usa o layout em memória
typedef struct LNode{
    char *str;              // texto "| a b c |"
    int w;                  // largura do texto
    int span;               // largura ocupada pelos filhos (com gaps)
    int slot;               // faixa horizontal da subárvore: max(w, span)
    int x;                  // coluna inicial do nó
    int level;              // nível (1 = raiz)
    int nk;                 // quantidade de filhos
    struct LNode **kids;
} LNode;

// declaração antecipada, pois o buildLayout usa o freeLayout antes de ele ser definido
static void freeLayout(LNode *ln);

// fase 1: monta a árvore de layout e calcula larguras (de baixo para cima)
// lê o nó do disco, cria o LNode correspondente e chama a si mesma para cada filho
// maxLevel é um parâmetro de saída com a altura da árvore
static LNode* buildLayout(BTree *bt, const char *filename, int level, int *maxLevel){
    int t = bt->t;
    Node *node = diskRead(filename, t);

    if(node == NULL) return NULL;

    LNode *ln = (LNode*) malloc(sizeof(LNode));

    if(ln == NULL){
        freeLoadedNode(node, t);
        return NULL;
    }

    // inicializa tudo que o freeLayout usa, para ele poder ser chamado com segurança em qualquer erro abaixo
    ln->level = level;
    ln->nk = 0;
    ln->kids = NULL;
    ln->span = 0;
    ln->str = NULL;

    // guarda o nível mais profundo visto até agora
    if(level > *maxLevel) *maxLevel = level;

    // texto do nó: "| " + n chaves de até 12 caracteres + "|" + \0
    ln->str = (char*) malloc(node->n * 12 + 8);

    if(ln->str == NULL){
        freeLayout(ln);
        freeLoadedNode(node, t);
        return NULL;
    }

    nodeString(node, ln->str);
    ln->w = (int) strlen(ln->str);

    // nó interno: monta o layout de cada filho
    if(!node->leaf){
        // um nó com n chaves tem até n+1 filhos
        ln->kids = (LNode**) malloc((node->n + 1) * sizeof(LNode*));

        if(ln->kids == NULL){
            freeLayout(ln);
            freeLoadedNode(node, t);
            return NULL;
        }

        for(int j = 0; j <= node->n; j++){
            if(node->children[j] != NULL){
                LNode *kid = buildLayout(bt, node->children[j], level + 1, maxLevel);

                if(kid == NULL){
                    /* libera o que já foi montado, sem deixar vazamento */
                    freeLayout(ln);
                    freeLoadedNode(node, t);
                    return NULL;
                }

                // ln->nk só conta os filhos já inseridos, então o freeLayout nunca toca em posições vazias
                ln->kids[ln->nk++] = kid;

                // a largura dos filhos é a soma dos slots deles
                ln->span += kid->slot;
            }
        }

        // soma o espaçamento entre irmãos (nk-1 intervalos)
        if(ln->nk > 1) ln->span += GAP * (ln->nk - 1);
    }

    // o nó em disco já foi copiado para ln->str, não precisa mais dele
    freeLoadedNode(node, t);

    // a faixa da subárvore é a maior entre o texto do nó e a largura dos filhos
    ln->slot = (ln->w > ln->span) ? ln->w : ln->span;
    return ln;
}

// fase 2: define a coluna de cada nó (de cima para baixo)
// left é a coluna onde começa a faixa (slot) deste nó
static void placeLayout(LNode *ln, int left){
    // centraliza o texto do nó dentro da própria faixa
    ln->x = left + (ln->slot - ln->w) / 2;

    // centraliza o bloco de filhos dentro da faixa
    // (se o texto do pai for mais largo que os filhos, sobra margem dos dois lados)
    int cursor = left + (ln->slot - ln->span) / 2;
    for(int i = 0; i < ln->nk; i++){
        placeLayout(ln->kids[i], cursor);

        // o próximo filho começa depois da faixa do atual mais o espaçamento
        cursor += ln->kids[i]->slot + GAP;
    }
}

// fase 3: desenha nós e conectores no canvas
// cada nível usa duas linhas da grid: a dos nós e a dos conectores logo abaixo
static void drawLayout(LNode *ln, char **grid){
    // nível 1 na linha 0, nível 2 na linha 2, e assim por diante
    int row = 2 * (ln->level - 1);

    // copia o texto sem o \0, pois a grid já está preenchida com espaços
    memcpy(grid[row] + ln->x, ln->str, ln->w);

    // coluna central do pai
    int pc = ln->x + ln->w / 2;
    for(int i = 0; i < ln->nk; i++){
        LNode *kid = ln->kids[i];

        // coluna central do filho
        int cc = kid->x + kid->w / 2;
        char ch;
        int col;

        // o conector fica uma coluna em direção ao pai, para o traço
        // parecer sair do pai e chegar ao filho
        // filho à esquerda do pai: '/', à direita: '\', alinhado: '|'
        if(cc < pc){ ch = '/';  col = cc + 1; }
        else if(cc > pc){ ch = '\\'; col = cc - 1; }
        else { ch = '|'; col = cc; }

        // o conector vai na linha logo abaixo do pai
        grid[row + 1][col] = ch;
        drawLayout(kid, grid);
    }
}

// libera o layout inteiro (filhos primeiro, depois o próprio nó)
static void freeLayout(LNode *ln){
    for(int i = 0; i < ln->nk; i++) freeLayout(ln->kids[i]);
    free(ln->kids);
    free(ln->str);
    free(ln);
}

// imprime a árvore por níveis, com conectores entre pai e filhos
// árvore vazia não imprime nada
void printBTree(BTree *bt){
    if(bt == NULL || bt->root == NULL) return;

    // fase 1: monta o layout e descobre a altura
    int maxLevel = 0;
    LNode *root = buildLayout(bt, bt->root, 1, &maxLevel);

    // falha de leitura ou de alocação, não imprime nada
    if(root == NULL) return;

    // fase 2: calcula as colunas, a raiz começa na coluna 0
    placeLayout(root, 0);

    // a largura da grid é a faixa da raiz, que contém todas as outras
    int width = root->slot;

    // uma linha de nós por nível e uma de conectores entre cada par de níveis
    int rows = 2 * maxLevel - 1;

    // aloca a grid: um vetor de linhas, cada uma com width + 1 (para o \0)
    char **grid = (char**) malloc(rows * sizeof(char*));

    if(grid == NULL){
        freeLayout(root);
        return;
    }

    for(int r = 0; r < rows; r++){
        grid[r] = (char*) malloc(width + 1);

        // se uma linha falhar, libera as anteriores e o resto
        if(grid[r] == NULL){
            for(int k = 0; k < r; k++) free(grid[k]);
            free(grid);
            freeLayout(root);
            return;
        }

        // começa toda em branco, o desenho só escreve por cima
        memset(grid[r], ' ', width);
        grid[r][width] = '\0';
    }

    // fase 3: desenha nós e conectores
    drawLayout(root, grid);

    // dw é o número de dígitos do maior nível, para alinhar o prefixo mesmo com 10+ níveis
    int dw = 1;
    for(int m = maxLevel; m >= 10; m /= 10) dw++;

    for(int r = 0; r < rows; r++){
        // linhas pares: nós, com o prefixo "Nível N: "
        // linhas ímpares: conectores, com espaços do mesmo tamanho do prefixo (8 + dw)
        if(r % 2 == 0) printf("Nível %*d: ", dw, r / 2 + 1);
        else printSpaces(8 + dw);
        printf("%s\n", grid[r]);
    }

    // a grid e o layout só existiram para a impressão
    for(int r = 0; r < rows; r++) free(grid[r]);
    free(grid);
    freeLayout(root);
}

/**
 * Funções de liberação de memória
 */

// libera um nó lido do disco, e só a cópia em memória (o arquivo continua em disco)
// percorre as 2t posições de filhos, pois o calloc deixou as vazias como NULL e free(NULL) é permitido
void freeLoadedNode(Node *node, int t){
    // aceita NULL, então pode ser chamada direto nos caminhos de erro
    if(node == NULL) return;

    // libera cada nome de filho (strings alocadas)
    for(int i = 0; i < 2 * t; i++)
        free(node->children[i]);

    // depois o vetor de ponteiros, as chaves, o nome do arquivo e por fim a struct
    free(node->children);
    free(node->keys);
    free(node->filename);
    free(node);
}

// libera só o que a árvore guarda em memória: o nome do arquivo da raiz
// os nós continuam em disco, é isso que permite recuperar a árvore na próxima execução
void freeBTree(BTree *bt){
    if (bt == NULL) return;

    free(bt->root);
    bt->root = NULL;

    // t = 0 marca a árvore como inválida, e o saveMeta recusaria salvar depois disso
    bt->t = 0;
}

/**
 * Função main e menus
 */

// menu de inserção: permite inserir N valores aleatórios ou um valor digitado
void menuInsert(BTree *bt) {
    int op = 0;

    printf("1. Insert N values\n");
    printf("2. Insert one value\n");
    printf("\nSelect option: ");

    // entrada não numérica: descarta o resto da linha e volta ao menu principal
    if (scanf("%d", &op) != 1) {
        printf("Invalid input\n");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return;
    }

    // opção 1: insere N valores aleatórios entre 1 e max
    if (op == 1) {
        int n, max;
        printf("Value of N: ");
        if (scanf("%d", &n) != 1) {
            printf("Invalid input\n");
            int c;
        while ((c = getchar()) != '\n' && c != EOF);
            return;
        }

        // o "Max value of N" é o limite superior dos valores sorteados
        printf("Max value of N: ");
        if (scanf("%d", &max) != 1) {
            printf("Invalid input\n");
            int c;
        while ((c = getchar()) != '\n' && c != EOF);
            return;
        }

        // n <= 0 não insere nada, e max <= 0 daria módulo por zero ou intervalo inválido
        if (n <= 0 || max <= 0) {
            printf("Invalid values\n");
            return;
        }

        printf("Entered values: ");
        for (int k = 0; k < n; k++) {
            // sorteia um valor de 1 a max
            int num = rand() % max + 1;
            printf("%d ", num);

            // para no primeiro erro de disco ou memória, já que continuar só repetiria a falha
            if (!insert(bt, num)) {
                printf("\nFailed to insert value %d (disk or memory error)\n", num);
                break;
            }
        }
        printf("\n");

    // opção 2: insere um único valor digitado
    } else if (op == 2) {
        int value;
        printf("Value to insert: ");
        if (scanf("%d", &value) != 1) {
            printf("Invalid input\n");
            int c;
        while ((c = getchar()) != '\n' && c != EOF);
            return;
        }

        if (!insert(bt, value))
            printf("Failed to insert value %d (disk or memory error)\n", value);

    } else {
        printf("\nInvalid option\n");
    }

    // dois getchar: o primeiro consome o \n que o scanf deixou no buffer
    // e o segundo espera o ENTER do usuário
    printf("\nPress ENTER to continue ");
    getchar();
    getchar();
}

// menu de remoção: mostra a árvore e remove valores em loop até digitar -1
void menuRemove(BTree *bt) {
    int value;

    while (1) {
        // mostra a árvore atualizada a cada remoção
        printBTree(bt);

        printf("Value to remove (-1 to exit): ");
        if (scanf("%d", &value) != 1) {
            printf("Invalid input\n");
            int c;
        while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }

        // -1 é o valor de saída
        if (value == -1) {
            break;
        }

        // busca antes só para distinguir "não encontrado" de "removido",
        // já que removeKey devolve 1 também quando a chave não existe
        Node *found = search(bt, bt->root, value);
        if (found == NULL) {
            printf("Value %d not found in the tree\n", value);
        } else {
            // o nó é uma cópia lida do disco: libera antes de remover
            freeLoadedNode(found, bt->t);

            if (removeKey(bt, value))
                printf("Value %d removed successfully\n", value);
            else
                printf("Failed to remove value %d (disk or memory error)\n", value);
        }

        // mesmo esquema de pausa do menuInsert
        printf("\nPress ENTER to continue ");
        getchar();
        getchar();
    }
}

// menu de busca: procura um valor e mostra o conteúdo do nó onde ele foi encontrado
void menuSearch(BTree *bt) {
    int value;
    printf("Value to search: ");
    if (scanf("%d", &value) != 1) {
        printf("Invalid input\n");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        return;
    }

    // com árvore vazia (root NULL), o search devolve NULL e cai em "not found"
    Node *result = search(bt, bt->root, value);
    if (result != NULL) {
        // mostra todas as chaves do nó que contém o valor
        printf("Value %d FOUND in node: ", value);
        for (int i = 0; i < result->n; i++)
            printf("%d ", result->keys[i]);
        printf("\n");

        // o search entrega a posse do nó, então quem chama precisa liberar
        freeLoadedNode(result, bt->t);
    } else {
        printf("Value %d not found\n", value);
    }

    printf("\nPress ENTER to continue ");
    getchar();
    getchar();
}

int main(void) {
    // a árvore em si fica só em disco, aqui só guardamos o nome da raiz e o t
    BTree bt;

    // semente do rand, usada na inserção de N valores aleatórios
    srand((unsigned) time(NULL));

    // cria a pasta bin/ onde ficam os nós e o tree.meta
    // se ela já existir (EEXIST) não é erro, é o caso de uma execução posterior
    if (MKDIR("bin") != 0 && errno != EEXIST) {
        printf("Could not create the bin/ directory\n");
        return 1;
    }

    // recupera a árvore do disco ou pergunta o t ao usuário
    // se o meta estiver corrompido, encerra sem tocar em nada
    if (!startTree(&bt)) return 1;

    // pausa para o usuário ler a mensagem de recuperação/inicialização
    // antes de o CLEAR() apagá-la
    printf("\nPress ENTER to continue ");
    getchar();

    int op = 0;
    do {
        // limpa a tela e mostra o menu principal
        CLEAR();
        printf("----- Menu -----\n");
        printf("1. Insert value\n");
        printf("2. Remove value\n");
        printf("3. Search value\n");
        printf("4. Print B Tree\n");
        printf("0. Exit\n");
        printf("\nSelect option: ");

        // entrada não numérica encerra o programa
        // o break sai do laço e cai no saveMeta final, então nada se perde
        if (scanf("%d", &op) != 1) {
            printf("Invalid input\n");
            break;
        }

        // cada opção limpa a tela e chama o menu correspondente
        switch (op) {
            case 1:
                CLEAR();
                menuInsert(&bt);
                break;
            case 2:
                CLEAR();
                menuRemove(&bt);
                break;
            case 3:
                CLEAR();
                menuSearch(&bt);
                break;
            case 4:
                CLEAR();
                printBTree(&bt);
                // dois getchar: o primeiro consome o \n deixado pelo scanf,
                // o segundo espera o ENTER do usuário
                printf("\nPress ENTER to continue ");
                getchar();
                getchar();
                break;
            case 0:
                // saída: só limpa a tela, o while (op != 0) encerra o laço
                CLEAR();
                break;
            default:
                CLEAR();
                printf("Invalid option\n");
        }

        // salva os metadados a cada volta do menu, e não a cada operação
        // isso garante que o bin/tree.meta acompanhe a raiz e o nextId
        // (a raiz pode mudar em insert e removeKey)
        if (!saveMeta(&bt))
            printf("Warning: could not save bin/tree.meta\n");

        printf("\n");
    } while (op != 0);

    // salva mais uma vez ao sair, cobrindo o caso do break por entrada inválida
    // (nesse caso o saveMeta de dentro do laço não chegou a rodar)
    if (!saveMeta(&bt))
        printf("Warning: could not save bin/tree.meta\n");

    // libera só a memória da árvore, os nós continuam em disco
    freeBTree(&bt);
    return 0;
}