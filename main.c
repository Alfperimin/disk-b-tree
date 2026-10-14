#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define MKDIR(p)  _mkdir(p)
    #define CLEAR()   system("cls")
    // rename não sobrescreve no Windows, MoveFileEx sim. Ambos devolvem != 0 em falha
    #define REPLACE(from, to)  (!MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING))
#else
    #include <sys/stat.h>
    #define MKDIR(p)  mkdir(p, 0755)
    #define CLEAR()   system("clear")
    #define REPLACE(from, to)  rename(from, to)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define META_FILE  "bin/tree.meta"
#define GAP 2   // espaço mínimo entre subárvores irmãs na impressão

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

/**
 * Utilitários
 */

// erro de disco é fatal: mostra o motivo e encerra (os nós já gravados continuam válidos)
static void die(const char *what){
    perror(what);
    exit(1);
}

char *copyString(const char *str){
    char *copy = malloc(strlen(str) + 1);
    strcpy(copy, str);
    return copy;
}

// nome do arquivo de um novo nó: bin/node_<id>.bin
char *nominate(void){
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "bin/node_%ld.bin", nextId++);
    return copyString(buffer);
}

int initialize(BTree *bt, int t){
    if(t < 2 || t > 1000) return 0;

    bt->root = NULL;
    bt->t = t;
    return 1;
}

/**
 * Memória
 */

// libera a cópia em memória de um nó (o arquivo continua em disco)
void freeLoadedNode(Node *node, int t){
    for(int i = 0; i < 2 * t; i++)
        free(node->children[i]);

    free(node->children);
    free(node->keys);
    free(node->filename);
    free(node);
}

// libera só o nome da raiz, os nós continuam em disco
void freeBTree(BTree *bt){
    free(bt->root);
    bt->root = NULL;
}

/**
 * Disco
 */

// formato: [n][leaf][keys: 2t-1 ints][para cada i em 0..2t-1: [exists][len][string]]
// grava em .tmp e substitui o original, para uma falha no meio não corromper o nó
void diskWrite(Node *node, int t){
    char temporary[128];
    snprintf(temporary, sizeof(temporary), "%s.tmp", node->filename);

    FILE *file = fopen(temporary, "wb");
    if(file == NULL) die(temporary);

    size_t nkeys = 2 * (size_t) t - 1;
    int ok = 1;

    ok &= fwrite(&node->n, sizeof(int), 1, file) == 1;
    ok &= fwrite(&node->leaf, sizeof(int), 1, file) == 1;
    ok &= fwrite(node->keys, sizeof(int), nkeys, file) == nkeys;

    for(int i = 0; i < 2 * t; i++){
        int exists = node->children[i] != NULL;
        ok &= fwrite(&exists, sizeof(int), 1, file) == 1;

        if(exists){
            size_t len = strlen(node->children[i]) + 1;
            ok &= fwrite(&len, sizeof(size_t), 1, file) == 1;
            ok &= fwrite(node->children[i], sizeof(char), len, file) == len;
        }
    }

    ok &= fclose(file) == 0;

    if(!ok || REPLACE(temporary, node->filename) != 0) die(node->filename);
}

// lê um nó do arquivo (formato do diskWrite)
Node* diskRead(const char *filename, int t){
    FILE *file = fopen(filename, "rb");
    if(file == NULL) die(filename);

    Node *node = malloc(sizeof(Node));
    node->filename = copyString(filename);
    node->keys = malloc((2 * t - 1) * sizeof(int));
    node->children = calloc(2 * t, sizeof(char *));

    size_t nkeys = 2 * (size_t) t - 1;
    int ok = 1;

    ok &= fread(&node->n, sizeof(int), 1, file) == 1;
    ok &= fread(&node->leaf, sizeof(int), 1, file) == 1;
    ok &= fread(node->keys, sizeof(int), nkeys, file) == nkeys;

    for(int i = 0; ok && i < 2 * t; i++){
        int exists;
        ok &= fread(&exists, sizeof(int), 1, file) == 1;

        if(ok && exists){
            size_t len;
            ok &= fread(&len, sizeof(size_t), 1, file) == 1;

            if(ok){
                node->children[i] = malloc(len);
                ok &= fread(node->children[i], sizeof(char), len, file) == len;
            }
        }
    }

    fclose(file);

    if(!ok) die(filename);
    return node;
}

// formato: [t][nextId][len][nome do arquivo raiz com \0]
void saveMeta(const BTree *bt){
    const char *temporary = META_FILE ".tmp";
    FILE *f = fopen(temporary, "wb");
    if(f == NULL) die(temporary);

    long next = nextId;
    size_t len = (bt->root != NULL) ? strlen(bt->root) + 1 : 0;
    int ok = 1;

    ok &= fwrite(&bt->t, sizeof(int), 1, f) == 1;
    ok &= fwrite(&next, sizeof(long), 1, f) == 1;
    ok &= fwrite(&len, sizeof(size_t), 1, f) == 1;
    if(len > 0) ok &= fwrite(bt->root, sizeof(char), len, f) == len;
    ok &= fclose(f) == 0;

    if(!ok || REPLACE(temporary, META_FILE) != 0) die(META_FILE);
}

// retorna 1 se carregou, 0 se não há meta (primeira execução), -1 se o meta é inválido
int loadMeta(BTree *bt){
    FILE *f = fopen(META_FILE, "rb");
    if(f == NULL) return 0;

    int t;
    long next;
    size_t len;
    char *root = NULL;

    int ok = fread(&t, sizeof(int), 1, f) == 1;
    ok = ok && fread(&next, sizeof(long), 1, f) == 1;
    ok = ok && fread(&len, sizeof(size_t), 1, f) == 1;

    if(ok && len > 0){
        root = malloc(len);
        ok = fread(root, sizeof(char), len, f) == len;
    }

    fclose(f);

    if(!ok){
        free(root);
        return -1;
    }

    bt->t = t;
    bt->root = root;
    nextId = next;
    return 1;
}

// recupera a árvore do disco ou pergunta o t ao usuário
int startTree(BTree *bt){
    int r = loadMeta(bt);

    if(r < 0){
        printf("bin/tree.meta is invalid. Fix it or delete the bin/ directory to start over.\n");
        return 0;
    }

    if(r > 0){
        printf("B Tree recovered from disk (t = %d).\n", bt->t);
        return 1;
    }

    int t;
    int c;

    while(1){
        printf("Define value t of B Tree: ");

        if(scanf("%d", &t) != 1){
            printf("Invalid input\n");
            while((c = getchar()) != '\n' && c != EOF);
            if(c == EOF) return 0;
            continue;
        }

        while((c = getchar()) != '\n' && c != EOF);

        if(initialize(bt, t)) break;
        printf("Value of t must be between 2 and 1000\n\n");
    }

    printf("B Tree initialized with t = %d.\n", bt->t);
    return 1;
}

/**
 * Inserção
 */

// cria um nó vazio em memória, só reserva o nome do arquivo
Node* createNode(int t, int leaf){
    Node *node = malloc(sizeof(Node));

    node->n = 0;
    node->leaf = leaf;
    node->keys = calloc(2 * t - 1, sizeof(int));
    node->children = calloc(2 * t, sizeof(char*));
    node->filename = nominate();

    return node;
}

// divide o filho cheio dad->children[i] em dois nós com t-1 chaves
// a chave do meio sobe para o pai e o novo nó vira children[i+1]
void splitNode(BTree *bt, Node *dad, int i){
    int t = bt->t;

    Node *child = diskRead(dad->children[i], t);
    Node *newn = createNode(t, child->leaf);

    int promoted = child->keys[t - 1];

    for(int j = 0; j < t - 1; j++)
        newn->keys[j] = child->keys[j + t];

    newn->n = t - 1;

    // nó interno também transfere os t últimos filhos
    if(!child->leaf){
        for(int j = 0; j < t; j++){
            newn->children[j] = child->children[j + t];
            child->children[j + t] = NULL;
        }
    }

    child->n = t - 1;

    // abre espaço no pai para o novo filho e para a chave promovida
    for(int j = dad->n; j > i; j--){
        dad->children[j + 1] = dad->children[j];
        dad->keys[j] = dad->keys[j - 1];
    }

    dad->children[i + 1] = copyString(newn->filename);
    dad->keys[i] = promoted;
    dad->n++;

    // o novo nó vai primeiro para o pai nunca apontar para um arquivo inexistente
    diskWrite(newn, t);
    diskWrite(dad, t);
    diskWrite(child, t);

    freeLoadedNode(child, t);
    freeLoadedNode(newn, t);
}

// insere a chave na subárvore em filename (o nó não pode estar cheio)
// divide os filhos cheios no caminho, então nunca precisa voltar para cima
void insertNonFull(BTree *bt, const char *filename, int key){
    int t = bt->t;
    Node *node = diskRead(filename, t);
    int i = node->n - 1;

    if(node->leaf){
        while(i >= 0 && key < node->keys[i]){
            node->keys[i + 1] = node->keys[i];
            i--;
        }

        node->keys[i + 1] = key;
        node->n++;

        diskWrite(node, t);
        freeLoadedNode(node, t);
        return;
    }

    i = 0;
    while(i < node->n && key > node->keys[i])
        i++;

    Node *child = diskRead(node->children[i], t);
    int childFull = (child->n == 2 * t - 1);
    freeLoadedNode(child, t);

    if(childFull){
        splitNode(bt, node, i);

        if(key > node->keys[i])
            i++;
    }

    char *childFilename = copyString(node->children[i]);
    freeLoadedNode(node, t);

    insertNonFull(bt, childFilename, key);
    free(childFilename);
}

void insert(BTree *bt, int key){
    int t = bt->t;

    // árvore vazia: a raiz é uma folha com a primeira chave
    if(bt->root == NULL){
        Node *root = createNode(t, 1);

        root->keys[0] = key;
        root->n = 1;

        diskWrite(root, t);
        bt->root = copyString(root->filename);

        freeLoadedNode(root, t);
        return;
    }

    Node *root = diskRead(bt->root, t);
    int rootFull = (root->n == 2 * t - 1);
    freeLoadedNode(root, t);

    if(!rootFull){
        insertNonFull(bt, bt->root, key);
        return;
    }

    // raiz cheia: nova raiz com a antiga como único filho, a árvore cresce em altura
    Node *newRoot = createNode(t, 0);
    newRoot->children[0] = copyString(bt->root);

    splitNode(bt, newRoot, 0);

    free(bt->root);
    bt->root = copyString(newRoot->filename);

    int i = (key > newRoot->keys[0]) ? 1 : 0;
    char *childFilename = copyString(newRoot->children[i]);

    freeLoadedNode(newRoot, t);

    insertNonFull(bt, childFilename, key);
    free(childFilename);
}

/**
 * Remoção
 */

// índice da primeira chave >= key (ou n se todas forem menores)
int findKeyIndex(Node *node, int key){
    int idx = 0;
    while(idx < node->n && node->keys[idx] < key)
        idx++;

    return idx;
}

// maior chave da subárvore em filename
int getPred(BTree *bt, const char *filename){
    int t = bt->t;
    Node *cur = diskRead(filename, t);

    while(!cur->leaf){
        char *next = copyString(cur->children[cur->n]);
        freeLoadedNode(cur, t);

        cur = diskRead(next, t);
        free(next);
    }

    int key = cur->keys[cur->n - 1];
    freeLoadedNode(cur, t);

    return key;
}

// menor chave da subárvore em filename
int getSucc(BTree *bt, const char *filename){
    int t = bt->t;
    Node *cur = diskRead(filename, t);

    while(!cur->leaf){
        char *next = copyString(cur->children[0]);
        freeLoadedNode(cur, t);

        cur = diskRead(next, t);
        free(next);
    }

    int key = cur->keys[0];
    freeLoadedNode(cur, t);

    return key;
}

// empresta uma chave do irmão da esquerda para o filho node->children[idx]
void borrowFromPrev(BTree *bt, Node *node, int idx){
    int t = bt->t;

    Node *child = diskRead(node->children[idx], t);
    Node *sibling = diskRead(node->children[idx - 1], t);

    for(int j = child->n - 1; j >= 0; j--)
        child->keys[j + 1] = child->keys[j];

    if(!child->leaf){
        for(int j = child->n; j >= 0; j--)
            child->children[j + 1] = child->children[j];

        child->children[0] = sibling->children[sibling->n];
        sibling->children[sibling->n] = NULL;
    }

    child->keys[0] = node->keys[idx - 1];
    node->keys[idx - 1] = sibling->keys[sibling->n - 1];

    child->n++;
    sibling->n--;

    diskWrite(child, t);
    diskWrite(sibling, t);

    freeLoadedNode(child, t);
    freeLoadedNode(sibling, t);
}

// empresta uma chave do irmão da direita para o filho node->children[idx]
void borrowFromNext(BTree *bt, Node *node, int idx){
    int t = bt->t;

    Node *child = diskRead(node->children[idx], t);
    Node *sibling = diskRead(node->children[idx + 1], t);

    child->keys[child->n] = node->keys[idx];

    if(!child->leaf)
        child->children[child->n + 1] = sibling->children[0];

    node->keys[idx] = sibling->keys[0];

    for(int j = 1; j < sibling->n; j++)
        sibling->keys[j - 1] = sibling->keys[j];

    if(!sibling->leaf){
        for(int j = 1; j <= sibling->n; j++)
            sibling->children[j - 1] = sibling->children[j];

        sibling->children[sibling->n] = NULL;
    }

    child->n++;
    sibling->n--;

    diskWrite(child, t);
    diskWrite(sibling, t);

    freeLoadedNode(child, t);
    freeLoadedNode(sibling, t);
}

// funde node->children[idx] e node->children[idx+1]: [filho][chave do pai][irmão]
// o arquivo do irmão é apagado e o pai perde uma chave e um filho
void merge(BTree *bt, Node *node, int idx){
    int t = bt->t;

    Node *child = diskRead(node->children[idx], t);
    Node *sibling = diskRead(node->children[idx + 1], t);

    child->keys[t - 1] = node->keys[idx];

    for(int j = 0; j < sibling->n; j++)
        child->keys[j + t] = sibling->keys[j];

    if(!child->leaf){
        for(int j = 0; j <= sibling->n; j++){
            child->children[j + t] = sibling->children[j];
            sibling->children[j] = NULL;
        }
    }

    free(node->children[idx + 1]);

    for(int j = idx + 1; j < node->n; j++)
        node->keys[j - 1] = node->keys[j];

    for(int j = idx + 2; j <= node->n; j++)
        node->children[j - 1] = node->children[j];

    node->children[node->n] = NULL;

    child->n += sibling->n + 1;
    node->n--;

    diskWrite(child, t);
    remove(sibling->filename);

    freeLoadedNode(child, t);
    freeLoadedNode(sibling, t);
}

// garante que node->children[idx] tenha pelo menos t chaves:
// empresta da esquerda, empresta da direita ou funde
void fill(BTree *bt, Node *node, int idx){
    int t = bt->t;

    if(idx != 0){
        Node *prev = diskRead(node->children[idx - 1], t);
        int enough = (prev->n >= t);
        freeLoadedNode(prev, t);

        if(enough){
            borrowFromPrev(bt, node, idx);
            return;
        }
    }

    if(idx != node->n){
        Node *next = diskRead(node->children[idx + 1], t);
        int enough = (next->n >= t);
        freeLoadedNode(next, t);

        if(enough){
            borrowFromNext(bt, node, idx);
            return;
        }
    }

    // sem irmão com chaves sobrando: funde com a direita, ou com a esquerda se for o último
    merge(bt, node, (idx != node->n) ? idx : idx - 1);
}

void removeFromLeaf(Node *node, int idx){
    for(int j = idx + 1; j < node->n; j++)
        node->keys[j - 1] = node->keys[j];

    node->n--;
}

void removeFromNode(BTree *bt, const char *filename, int key);

// remove node->keys[idx] de um nó interno
// grava o próprio nó antes de descer
void removeFromInternal(BTree *bt, Node *node, int idx){
    int t = bt->t;
    int k = node->keys[idx];

    Node *left = diskRead(node->children[idx], t);
    int leftEnough = (left->n >= t);
    freeLoadedNode(left, t);

    // filho da esquerda com chaves sobrando: troca pelo predecessor
    if(leftEnough){
        int pred = getPred(bt, node->children[idx]);

        node->keys[idx] = pred;
        diskWrite(node, t);

        removeFromNode(bt, node->children[idx], pred);
        return;
    }

    Node *right = diskRead(node->children[idx + 1], t);
    int rightEnough = (right->n >= t);
    freeLoadedNode(right, t);

    // filho da direita com chaves sobrando: troca pelo sucessor
    if(rightEnough){
        int succ = getSucc(bt, node->children[idx + 1]);

        node->keys[idx] = succ;
        diskWrite(node, t);

        removeFromNode(bt, node->children[idx + 1], succ);
        return;
    }

    // os dois com t-1 chaves: funde e remove k do nó resultante
    merge(bt, node, idx);
    diskWrite(node, t);

    removeFromNode(bt, node->children[idx], k);
}

// remove key da subárvore em filename
// garante que todo filho em que entra tenha pelo menos t chaves
void removeFromNode(BTree *bt, const char *filename, int key){
    int t = bt->t;
    Node *node = diskRead(filename, t);
    int idx = findKeyIndex(node, key);

    if(idx < node->n && node->keys[idx] == key){
        if(node->leaf){
            removeFromLeaf(node, idx);
            diskWrite(node, t);
        } else{
            removeFromInternal(bt, node, idx);
        }

        freeLoadedNode(node, t);
        return;
    }

    if(node->leaf){
        freeLoadedNode(node, t);
        return;
    }

    Node *child = diskRead(node->children[idx], t);
    int needFill = (child->n < t);
    freeLoadedNode(child, t);

    if(needFill){
        fill(bt, node, idx);
        diskWrite(node, t);

        // se fundiu com o irmão da esquerda, o filho agora é idx - 1
        if(idx > node->n)
            idx--;
    }

    char *childFilename = copyString(node->children[idx]);
    freeLoadedNode(node, t);

    removeFromNode(bt, childFilename, key);
    free(childFilename);
}

void removeKey(BTree *bt, int key){
    if(bt->root == NULL) return;

    int t = bt->t;

    removeFromNode(bt, bt->root, key);

    Node *root = diskRead(bt->root, t);

    if(root->n > 0){
        freeLoadedNode(root, t);
        return;
    }

    // raiz vazia: se era interna, o único filho vira a raiz, se era folha a árvore ficou vazia
    char *newRoot = root->leaf ? NULL : copyString(root->children[0]);

    remove(root->filename);
    freeLoadedNode(root, t);

    free(bt->root);
    bt->root = newRoot;
}

/**
 * Busca
 */

// retorna o nó (cópia lida do disco, quem chama libera) que contém k, ou NULL
Node* search(BTree *bt, const char *filename, int k){
    if(filename == NULL) return NULL;

    int t = bt->t;
    Node *node = diskRead(filename, t);
    int i = findKeyIndex(node, k);

    if(i < node->n && node->keys[i] == k) return node;

    if(node->leaf){
        freeLoadedNode(node, t);
        return NULL;
    }

    char *childFilename = copyString(node->children[i]);
    freeLoadedNode(node, t);

    Node *result = search(bt, childFilename, k);
    free(childFilename);

    return result;
}

/**
 * Impressão
 */

// "| a b c |" (cada chave ocupa até 12 caracteres)
void nodeString(Node *node, char *buffer){
    strcpy(buffer, "| ");

    for(int i = 0; i < node->n; i++){
        char temp[16];
        sprintf(temp, "%d ", node->keys[i]);
        strcat(buffer, temp);
    }

    strcat(buffer, "|");
}

// o desenho é feito em 3 fases: monta uma árvore de layout paralela à árvore em disco,
// calcula a coluna de cada nó e desenha tudo em uma matriz de caracteres
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

// fase 1: monta o layout e calcula as larguras (de baixo para cima)
static LNode* buildLayout(BTree *bt, const char *filename, int level, int *maxLevel){
    int t = bt->t;
    Node *node = diskRead(filename, t);
    LNode *ln = malloc(sizeof(LNode));

    ln->level = level;
    ln->span = 0;
    ln->nk = 0;
    ln->kids = NULL;

    if(level > *maxLevel) *maxLevel = level;

    ln->str = malloc(node->n * 12 + 8);
    nodeString(node, ln->str);
    ln->w = (int) strlen(ln->str);

    if(!node->leaf){
        ln->nk = node->n + 1;
        ln->kids = malloc(ln->nk * sizeof(LNode*));

        for(int j = 0; j < ln->nk; j++){
            ln->kids[j] = buildLayout(bt, node->children[j], level + 1, maxLevel);
            ln->span += ln->kids[j]->slot;
        }

        ln->span += GAP * (ln->nk - 1);
    }

    freeLoadedNode(node, t);

    ln->slot = (ln->w > ln->span) ? ln->w : ln->span;
    return ln;
}

// fase 2: define a coluna de cada nó (de cima para baixo)
static void placeLayout(LNode *ln, int left){
    ln->x = left + (ln->slot - ln->w) / 2;

    int cursor = left + (ln->slot - ln->span) / 2;
    for(int i = 0; i < ln->nk; i++){
        placeLayout(ln->kids[i], cursor);
        cursor += ln->kids[i]->slot + GAP;
    }
}

// fase 3: desenha nós e conectores (cada nível usa duas linhas: nós e conectores)
static void drawLayout(LNode *ln, char **grid){
    int row = 2 * (ln->level - 1);

    memcpy(grid[row] + ln->x, ln->str, ln->w);

    int pc = ln->x + ln->w / 2;
    for(int i = 0; i < ln->nk; i++){
        LNode *kid = ln->kids[i];
        int cc = kid->x + kid->w / 2;
        char ch;
        int col;

        if(cc < pc){ ch = '/';  col = cc + 1; }
        else if(cc > pc){ ch = '\\'; col = cc - 1; }
        else { ch = '|'; col = cc; }

        grid[row + 1][col] = ch;
        drawLayout(kid, grid);
    }
}

static void freeLayout(LNode *ln){
    for(int i = 0; i < ln->nk; i++) freeLayout(ln->kids[i]);
    free(ln->kids);
    free(ln->str);
    free(ln);
}

void printBTree(BTree *bt){
    if(bt->root == NULL) return;

    int maxLevel = 0;
    LNode *root = buildLayout(bt, bt->root, 1, &maxLevel);

    placeLayout(root, 0);

    int width = root->slot;
    int rows = 2 * maxLevel - 1;
    char **grid = malloc(rows * sizeof(char*));

    for(int r = 0; r < rows; r++){
        grid[r] = malloc(width + 1);
        memset(grid[r], ' ', width);
        grid[r][width] = '\0';
    }

    drawLayout(root, grid);

    // dw é o número de dígitos do maior nível, para alinhar o prefixo
    int dw = 1;
    for(int m = maxLevel; m >= 10; m /= 10) dw++;

    for(int r = 0; r < rows; r++){
        if(r % 2 == 0) printf("Nível %*d: ", dw, r / 2 + 1);
        else printf("%*s", 8 + dw, "");
        printf("%s\n", grid[r]);
    }

    for(int r = 0; r < rows; r++) free(grid[r]);
    free(grid);
    freeLayout(root);
}

/**
 * Menus
 */

// lê um inteiro, em caso de entrada inválida descarta a linha e retorna 0
static int readInt(int *value){
    if(scanf("%d", value) == 1) return 1;

    printf("Invalid input\n");
    while(getchar() != '\n');
    return 0;
}

// o primeiro getchar consome o \n deixado pelo scanf, o segundo espera o ENTER
static void waitEnter(void){
    printf("\nPress ENTER to continue ");
    getchar();
    getchar();
}

void menuInsert(BTree *bt){
    int op;

    printf("1. Insert N values\n");
    printf("2. Insert one value\n");
    printf("\nSelect option: ");

    if(!readInt(&op)) return;

    if(op == 1){
        int n, max;

        printf("Value of N: ");
        if(!readInt(&n)) return;

        printf("Max value of N: ");
        if(!readInt(&max)) return;

        if(n <= 0 || max <= 0){
            printf("Invalid values\n");
            return;
        }

        printf("Entered values: ");
        for(int k = 0; k < n; k++){
            int num = rand() % max + 1;
            printf("%d ", num);
            insert(bt, num);
        }
        printf("\n");

    } else if(op == 2){
        int value;

        printf("Value to insert: ");
        if(!readInt(&value)) return;

        insert(bt, value);

    } else{
        printf("\nInvalid option\n");
    }

    waitEnter();
}

// remove valores em loop até digitar -1
void menuRemove(BTree *bt){
    int value;

    while(1){
        printBTree(bt);

        printf("Value to remove (-1 to exit): ");
        if(!readInt(&value)) continue;

        if(value == -1) break;

        Node *found = search(bt, bt->root, value);

        if(found == NULL){
            printf("Value %d not found in the tree\n", value);
        } else{
            freeLoadedNode(found, bt->t);
            removeKey(bt, value);
            printf("Value %d removed successfully\n", value);
        }

        waitEnter();
    }
}

void menuSearch(BTree *bt){
    int value;

    printf("Value to search: ");
    if(!readInt(&value)) return;

    Node *result = search(bt, bt->root, value);

    if(result != NULL){
        printf("Value %d FOUND in node: ", value);
        for(int i = 0; i < result->n; i++)
            printf("%d ", result->keys[i]);
        printf("\n");

        freeLoadedNode(result, bt->t);
    } else{
        printf("Value %d not found\n", value);
    }

    waitEnter();
}

int main(void){
    BTree bt;

    srand((unsigned) time(NULL));

    // se a pasta já existir o MKDIR falha, o que é inofensivo
    MKDIR("bin");

    if(!startTree(&bt)) return 1;

    printf("\nPress ENTER to continue ");
    getchar();

    int op;
    do{
        CLEAR();
        printf("----- Menu -----\n");
        printf("1. Insert value\n");
        printf("2. Remove value\n");
        printf("3. Search value\n");
        printf("4. Print B Tree\n");
        printf("0. Exit\n");
        printf("\nSelect option: ");

        if(!readInt(&op)) break;

        CLEAR();
        switch(op){
            case 1: menuInsert(&bt); break;
            case 2: menuRemove(&bt); break;
            case 3: menuSearch(&bt); break;
            case 4:
                printBTree(&bt);
                waitEnter();
                break;
            case 0: break;
            default: printf("Invalid option\n");
        }

        // a raiz e o nextId mudam em insert/remove, então salva a cada volta do menu
        saveMeta(&bt);
        printf("\n");
    } while(op != 0);

    saveMeta(&bt);
    freeBTree(&bt);
    return 0;
}