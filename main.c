#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

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

int initialize(BTree *bt, int t) {
    if (bt == NULL) return 0;
    if (t < 2) return 0;
    if (t > 1000) return 0;

    bt->root = NULL;
    bt->t = t;
    return 1;
}

/**
 * Assinaturas dos médotos/funções
 */

char *copyString(const char *str);
void freeLoadedNode(Node *node, int t);
char* nominate();
int diskWrite(Node *node, int t);
Node* diskRead(const char *filename, int t);

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

void freeBTree(BTree *bt);

/**
 * Funções de manipulação dos arquivos
 */

char *copyString(const char *str){
    if(str == NULL) return NULL;

    char *copy = malloc(strlen(str) + 1);
    if(copy == NULL) return NULL;

    strcpy(copy, str);
    return copy;
}

void freeLoadedNode(Node *node, int t){
    if(node == NULL) return;

    for(int i = 0; i < 2 * t; i++)
        free(node->children[i]);

    free(node->children);
    free(node->keys);
    free(node->filename);
    free(node);
}

char* nominate(){
    char buffer[64];

    snprintf(buffer, sizeof(buffer), "bin/node_%ld.bin", nextId++);

    char *filename = malloc(strlen(buffer) + 1);

    if(filename == NULL) return NULL;

    strcpy(filename, buffer);

    return filename;
}

int diskWrite(Node *node, int t){
    if(node == NULL || node->filename == NULL) return 0;

    FILE *file = fopen(node->filename, "wb");

    if(file == NULL) return 0;

    int ok = 1;

    if(fwrite(&node->n, sizeof(int), 1, file) != 1)
        ok = 0;

    if(ok && fwrite(&node->leaf, sizeof(int), 1, file) != 1)
        ok = 0;

    if (ok && fwrite(node->keys, sizeof(int), 2 * t - 1, file) != 2 * t - 1)
        ok = 0;
    
    for(int i = 0; ok && i < 2*t; i++){
        int exists = node->children[i] != NULL;

        if(fwrite(&exists, sizeof(int), 1, file) != 1){
            ok = 0;
            break;
        }

        if(exists){
            size_t len = strlen(node->children[i]) + 1;

            if(fwrite(&len, sizeof(size_t), 1, file) != 1 ||
                fwrite(node->children[i], sizeof(char), len, file) != len){
                ok = 0;
            }
        }
    }

    if(fclose(file) != 0)
        ok = 0;

    return ok;
}

Node* diskRead(const char *filename, int t){
    if(filename == NULL) return NULL;

    FILE *file = fopen(filename, "rb");

    if(file == NULL) return NULL;

    Node *node = malloc(sizeof(Node));

    if(node == NULL){
        fclose(file);
        return NULL;
    }

    node->filename = malloc(strlen(filename) + 1);
    node->keys = malloc((2 * t - 1) * sizeof(int));
    node->children = calloc(2 * t, sizeof(char *));

    if(node->filename == NULL || node->keys == NULL || node->children == NULL){

        free(node->filename);
        free(node->keys);
        free(node->children);
        free(node);
        fclose(file);

        return NULL;
    }

    strcpy(node->filename, filename);

    int ok = 1;

    if(fread(&node->n, sizeof(int), 1, file) != 1)
        ok = 0;

    if(ok && fread(&node->leaf, sizeof(int), 1, file) != 1)
        ok = 0;

    if(ok && fread(node->keys, sizeof(int), 2*t-1, file) != 2*t-1)
        ok = 0;

    for(int i = 0; ok && i < 2*t; i++){
        int exists;
        size_t len;

        if(fread(&exists, sizeof(int), 1, file) != 1){
            ok = 0;
            break;
        }

        if(exists){
            if(fread(&len, sizeof(size_t), 1, file) != 1 || len == 0 || len > 4096){
                ok = 0;
                break;
            }

            node->children[i] = malloc(len);

            if(node->children[i] == NULL){
                ok = 0;
                break;
            }

            if(fread(node->children[i], sizeof(char), len, file) != len){
                ok = 0;
                break;
            }

            if(node->children[i][len - 1] != '\0'){
                ok = 0;
                break;
            }
        }
    }

    if(fclose(file) != 0)
        ok = 0;

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

/**
 * Funções de inserção da estrutura
 */

int splitNode(BTree *bt, Node *dad, int i){
    if(bt == NULL || dad == NULL || i < 0 || i > dad->n || dad->n >= 2 * bt->t - 1) return 0;

    int t = bt->t;

    Node *child = diskRead(dad->children[i], t);
    if(child == NULL) return 0;

    if(child->n != 2 * t - 1){
        freeLoadedNode(child, t);
        return 0;
    }

    Node *newn = createNode(t, child->leaf);
    if(newn == NULL){
        freeLoadedNode(child, t);
        return 0;
    }

    int promoted = child->keys[t - 1];

    for(int j = 0; j < t - 1; j++)
        newn->keys[j] = child->keys[j + t];

    newn->n = t - 1;

    if(!child->leaf){
        for(int j = 0; j < t; j++){
            newn->children[j] = child->children[j + t];
            child->children[j + t] = NULL;
        }
    }

    child->n = t - 1;

    for(int j = dad->n; j > i; j--)
        dad->children[j + 1] = dad->children[j];

    dad->children[i + 1] = copyString(newn->filename);

    if(dad->children[i + 1] == NULL){
        freeLoadedNode(child, t);
        freeLoadedNode(newn, t);
        return 0;
    }

    for(int j = dad->n; j > i; j--)
        dad->keys[j] = dad->keys[j - 1];

    dad->keys[i] = promoted;
    dad->n++;

    int ok = diskWrite(child, t);
    if(ok) ok = diskWrite(newn, t);
    if(ok) ok = diskWrite(dad, t);

    freeLoadedNode(child, t);
    freeLoadedNode(newn, t);

    return ok;
}

Node* createNode(int t, int leaf){
	Node *newn = malloc(sizeof(Node));

    if(newn == NULL) return NULL;

	newn->n = 0;
	newn->leaf = leaf;
	
	newn->keys = calloc(2 * t - 1, sizeof(int));
	newn->children = calloc(2 * t, sizeof(char*));

    newn->filename = nominate();

    if(newn->keys == NULL || newn->children == NULL || newn->filename == NULL){
        free(newn->keys);
        free(newn->children);
        free(newn->filename);
        free(newn);

        return NULL;
    }
	
	return newn;
}

int insertNonFull(BTree *bt, const char *filename, int key) {
    if(bt == NULL || filename == NULL) return 0;

    int t = bt->t;
    Node *node = diskRead(filename, t);

    if(node == NULL) return 0;

    int i = node->n - 1;

    if(node->leaf){
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

    i = 0;
    while(i < node->n && key > node->keys[i])
        i++;

    if(node->children[i] == NULL){
        freeLoadedNode(node, t);
        return 0;
    }

    Node *child = diskRead(node->children[i], t);

    if(child == NULL){
        freeLoadedNode(node, t);
        return 0;
    }

    int childFull = (child->n == 2 * t - 1);
    freeLoadedNode(child, t);

    if(childFull){
        if(!splitNode(bt, node, i)){
            freeLoadedNode(node, t);
            return 0;
        }

        if(key > node->keys[i])
            i++;
    }

    char *childFilename = copyString(node->children[i]);

    freeLoadedNode(node, t);

    if(childFilename == NULL) return 0;

    int ok = insertNonFull(bt, childFilename, key);
    free(childFilename);

    return ok;
}

int insert(BTree *bt, int key) {
    if(bt == NULL || bt->t < 2) return 0;

    int t = bt->t;

    if(bt->root == NULL){
        Node *root = createNode(t, 1);
        if(root == NULL) return 0;

        root->keys[0] = key;
        root->n = 1;

        int ok = diskWrite(root, t);

        if(ok){
            bt->root = copyString(root->filename);

            if(bt->root == NULL)
                ok = 0;
        }

        freeLoadedNode(root, t);
        return ok;
    }

    Node *root = diskRead(bt->root, t);
    if(root == NULL) return 0;

    int rootFull = (root->n == 2 * t - 1);
    freeLoadedNode(root, t);

    if(!rootFull)
        return insertNonFull(bt, bt->root, key);

    
    Node *newRoot = createNode(t, 0);
    if(newRoot == NULL) return 0;

    newRoot->children[0] = copyString(bt->root);

    if(newRoot->children[0] == NULL){
        freeLoadedNode(newRoot, t);
        return 0;
    }

    if(!splitNode(bt, newRoot, 0)){
        freeLoadedNode(newRoot, t);
        return 0;
    }

    char *newRootFilename = copyString(newRoot->filename);

    if(newRootFilename == NULL){
        freeLoadedNode(newRoot, t);
        return 0;
    }

    free(bt->root);
    bt->root = newRootFilename;

    int i = (key > newRoot->keys[0]) ? 1 : 0;

    char *childFilename = copyString(newRoot->children[i]);

    freeLoadedNode(newRoot, t);

    if (childFilename == NULL) return 0;

    int ok = insertNonFull(bt, childFilename, key);
    free(childFilename);

    return ok;
}

/**
 * Funções de remoção da estrutura
 */

int findKeyIndex(Node *node, int key){
	int idx = 0;
	while(idx < node->n && node->keys[idx] < key)
		idx++;

	return idx;
}

int getPred(BTree *bt, const char *filename, int *pred){
	if(bt == NULL || filename == NULL || pred == NULL) return 0;

	int t = bt->t;
	Node *cur = diskRead(filename, t);

	while(cur != NULL && !cur->leaf){
		char *next = copyString(cur->children[cur->n]);
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

	*pred = cur->keys[cur->n - 1];
	freeLoadedNode(cur, t);

	return 1;
}

int getSucc(BTree *bt, const char *filename, int *succ){
	if(bt == NULL || filename == NULL || succ == NULL) return 0;

	int t = bt->t;
	Node *cur = diskRead(filename, t);

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

	*succ = cur->keys[0];
	freeLoadedNode(cur, t);

	return 1;
}

int borrowFromPrev(BTree *bt, Node *node, int idx){
	if(bt == NULL || node == NULL || idx < 1 || idx > node->n) return 0;

	int t = bt->t;

	Node *child = diskRead(node->children[idx], t);
	Node *sibling = diskRead(node->children[idx - 1], t);

	if(child == NULL || sibling == NULL){
		freeLoadedNode(child, t);
		freeLoadedNode(sibling, t);
		return 0;
	}

	for(int j = child->n - 1; j >= 0; j--)
		child->keys[j + 1] = child->keys[j];

	if(!child->leaf){
		for(int j = child->n; j >= 0; j--)
			child->children[j + 1] = child->children[j];

		/* a string passa do irmão para o filho: anula a origem */
		child->children[0] = sibling->children[sibling->n];
		sibling->children[sibling->n] = NULL;
	}

	child->keys[0] = node->keys[idx - 1];
	node->keys[idx - 1] = sibling->keys[sibling->n - 1];

	child->n++;
	sibling->n--;

	int ok = diskWrite(child, t);
	if(ok) ok = diskWrite(sibling, t);

	freeLoadedNode(child, t);
	freeLoadedNode(sibling, t);

	return ok;
}

int borrowFromNext(BTree *bt, Node *node, int idx){
	if(bt == NULL || node == NULL || idx < 0 || idx >= node->n) return 0;

	int t = bt->t;

	Node *child = diskRead(node->children[idx], t);
	Node *sibling = diskRead(node->children[idx + 1], t);

	if(child == NULL || sibling == NULL){
		freeLoadedNode(child, t);
		freeLoadedNode(sibling, t);
		return 0;
	}

	child->keys[child->n] = node->keys[idx];

	if(!child->leaf){
		/* a string passa do irmão para o filho */
		child->children[child->n + 1] = sibling->children[0];
	}

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

	int ok = diskWrite(child, t);
	if(ok) ok = diskWrite(sibling, t);

	freeLoadedNode(child, t);
	freeLoadedNode(sibling, t);

	return ok;
}

int merge(BTree *bt, Node *node, int idx){
	if(bt == NULL || node == NULL || idx < 0 || idx >= node->n) return 0;

	int t = bt->t;

	Node *child = diskRead(node->children[idx], t);
	Node *sibling = diskRead(node->children[idx + 1], t);

	if(child == NULL || sibling == NULL){
		freeLoadedNode(child, t);
		freeLoadedNode(sibling, t);
		return 0;
	}

	child->keys[t - 1] = node->keys[idx];

	for(int j = 0; j < sibling->n; j++)
		child->keys[j + t] = sibling->keys[j];

	if(!child->leaf){
		for(int j = 0; j <= sibling->n; j++){
			child->children[j + t] = sibling->children[j];
			sibling->children[j] = NULL; /* evita double free */
		}
	}

	free(node->children[idx + 1]);
	node->children[idx + 1] = NULL;

	for(int j = idx + 1; j < node->n; j++)
		node->keys[j - 1] = node->keys[j];

	for(int j = idx + 2; j <= node->n; j++)
		node->children[j - 1] = node->children[j];

	node->children[node->n] = NULL;

	child->n += sibling->n + 1;
	node->n--;

	int ok = diskWrite(child, t);

	if(ok && remove(sibling->filename) != 0)
		ok = 0;

	freeLoadedNode(child, t);
	freeLoadedNode(sibling, t);

	return ok;
}

int fill(BTree *bt, Node *node, int idx){
	if(bt == NULL || node == NULL || idx < 0 || idx > node->n) return 0;

	int t = bt->t;

	if(idx != 0){
		Node *prev = diskRead(node->children[idx - 1], t);
		if(prev == NULL) return 0;

		int enough = (prev->n >= t);
		freeLoadedNode(prev, t);

		if(enough) return borrowFromPrev(bt, node, idx);
	}

	if(idx != node->n){
		Node *next = diskRead(node->children[idx + 1], t);
		if(next == NULL) return 0;

		int enough = (next->n >= t);
		freeLoadedNode(next, t);

		if(enough) return borrowFromNext(bt, node, idx);
	}

	if(idx != node->n)
		return merge(bt, node, idx);

	return merge(bt, node, idx - 1);
}

void removeFromLeaf(Node *node, int idx){
	for(int j = idx + 1; j < node->n; j++)
		node->keys[j - 1] = node->keys[j];

	node->n--;
}

int removeFromInternal(BTree *bt, Node *node, int idx){
	if(bt == NULL || node == NULL || node->leaf || idx < 0 || idx >= node->n) return 0;

	int t = bt->t;
	int k = node->keys[idx];

	Node *left = diskRead(node->children[idx], t);
	if(left == NULL) return 0;

	int leftEnough = (left->n >= t);
	freeLoadedNode(left, t);

	if(leftEnough){
		int pred;

		if(!getPred(bt, node->children[idx], &pred)) return 0;

		node->keys[idx] = pred;

		if(!diskWrite(node, t)) return 0;

		return removeFromNode(bt, node->children[idx], pred);
	}

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

	/* os dois filhos têm t-1 chaves: funde e remove k do nó resultante */
	if(!merge(bt, node, idx)) return 0;

	if(!diskWrite(node, t)) return 0;

	return removeFromNode(bt, node->children[idx], k);
}

int removeFromNode(BTree *bt, const char *filename, int key){
	if(bt == NULL || filename == NULL) return 0;

	int t = bt->t;
	Node *node = diskRead(filename, t);

	if(node == NULL) return 0;

	int idx = findKeyIndex(node, key);
	int ok;

	if(idx < node->n && node->keys[idx] == key){
		if(node->leaf){
			removeFromLeaf(node, idx);
			ok = diskWrite(node, t);
		} else{
			/* grava o próprio nó internamente, antes de descer */
			ok = removeFromInternal(bt, node, idx);
		}

		freeLoadedNode(node, t);
		return ok;
	}

	/* chave não está neste nó */
	if(node->leaf){
		freeLoadedNode(node, t);
		return 1; /* chave inexistente: nada a fazer, não é erro */
	}

	Node *child = diskRead(node->children[idx], t);

	if(child == NULL){
		freeLoadedNode(node, t);
		return 0;
	}

	int needFill = (child->n < t);
	freeLoadedNode(child, t);

	if(needFill){
		if(!fill(bt, node, idx) || !diskWrite(node, t)){
			freeLoadedNode(node, t);
			return 0;
		}

		/* se houve merge com o irmão da esquerda, o filho passou a ser idx - 1 */
		if(idx > node->n)
			idx--;
	}

	char *childFilename = copyString(node->children[idx]);

	freeLoadedNode(node, t);

	if(childFilename == NULL) return 0;

	ok = removeFromNode(bt, childFilename, key);
	free(childFilename);

	return ok;
}

int removeKey(BTree *bt, int key){
	if(bt == NULL) return 0;
	if(bt->root == NULL) return 1;

	int t = bt->t;

	if(!removeFromNode(bt, bt->root, key)) return 0;

	Node *root = diskRead(bt->root, t);
	if(root == NULL) return 0;

	if(root->n > 0){
		freeLoadedNode(root, t);
		return 1;
	}

	char *newRoot = NULL;

	if(!root->leaf){
		newRoot = copyString(root->children[0]);

		if(newRoot == NULL){
			freeLoadedNode(root, t);
			return 0;
		}
	}

	int ok = (remove(root->filename) == 0);

	freeLoadedNode(root, t);
	free(bt->root);
	bt->root = newRoot; /* NULL se a raiz era folha vazia */

	return ok;
}

/**
 * Funções de busca da estrutura
 */

Node* search(BTree *bt, const char *filename, int k){
    if(bt == NULL || filename == NULL) return NULL;

    int t = bt->t;
    Node *node = diskRead(filename, t);

    if(node == NULL) return NULL;

    int i = 0;

    while(i < node->n && k > node->keys[i])
        i++;

    if(i < node->n && node->keys[i] == k) return node;

    if(node->leaf){
        freeLoadedNode(node, t);
        return NULL;
    }

    char *childFilename = copyString(node->children[i]);

    freeLoadedNode(node, t);

    if(childFilename == NULL) return NULL;

    Node *result = search(bt, childFilename, k);
    free(childFilename);

    return result;
}

/**
 * Funções de impressão da estrutura
 */

#define GAP 2   /* espaços mínimos entre subárvores irmãs */

void nodeString(Node *node, char *buffer)
{
    buffer[0] = '\0';
    strcat(buffer, "| ");
    for(int i = 0; i < node->n; i++){
        char temp[16];
        sprintf(temp, "%d ", node->keys[i]);
        strcat(buffer, temp);
    }
    strcat(buffer, "|");
}

void printSpaces(int n)
{
    for(int i = 0; i < n; i++) printf(" ");
}

/* Informações de layout de um nó (estrutura paralela à árvore) */
typedef struct LNode
{
    char *str;              /* texto "| a b c |" */
    int w;                  /* largura do texto */
    int span;               /* largura ocupada pelos filhos (com gaps) */
    int slot;               /* faixa horizontal da subárvore: max(w, span) */
    int x;                  /* coluna inicial do nó */
    int level;              /* nível (1 = raiz) */
    int nk;                 /* quantidade de filhos */
    struct LNode **kids;
} LNode;

static void freeLayout(LNode *ln);

/* Fase 1: monta a árvore de layout e calcula larguras (de baixo para cima) */
static LNode* buildLayout(BTree *bt, const char *filename, int level, int *maxLevel)
{
    int t = bt->t;
    Node *node = diskRead(filename, t);

    if(node == NULL) return NULL;

    LNode *ln = (LNode*) malloc(sizeof(LNode));

    if(ln == NULL){
        freeLoadedNode(node, t);
        return NULL;
    }

    ln->level = level;
    ln->nk = 0;
    ln->kids = NULL;
    ln->span = 0;
    ln->str = NULL;

    if(level > *maxLevel) *maxLevel = level;

    ln->str = (char*) malloc(node->n * 12 + 8);

    if(ln->str == NULL){
        freeLayout(ln);
        freeLoadedNode(node, t);
        return NULL;
    }

    nodeString(node, ln->str);
    ln->w = (int) strlen(ln->str);

    if(!node->leaf){
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

                ln->kids[ln->nk++] = kid;
                ln->span += kid->slot;
            }
        }

        if(ln->nk > 1) ln->span += GAP * (ln->nk - 1);
    }

    /* todo o texto já foi copiado para ln->str, o nó em memória não é mais necessário */
    freeLoadedNode(node, t);

    ln->slot = (ln->w > ln->span) ? ln->w : ln->span;
    return ln;
}

/* Fase 2: define a coluna de cada nó (de cima para baixo) */
static void placeLayout(LNode *ln, int left)
{
    ln->x = left + (ln->slot - ln->w) / 2;

    int cursor = left + (ln->slot - ln->span) / 2;
    for(int i = 0; i < ln->nk; i++){
        placeLayout(ln->kids[i], cursor);
        cursor += ln->kids[i]->slot + GAP;
    }
}

/* Fase 3: desenha nós e conectores no canvas */
static void drawLayout(LNode *ln, char **grid)
{
    int row = 2 * (ln->level - 1);
    memcpy(grid[row] + ln->x, ln->str, ln->w);

    int pc = ln->x + ln->w / 2;                 /* centro do pai */
    for(int i = 0; i < ln->nk; i++){
        LNode *kid = ln->kids[i];
        int cc = kid->x + kid->w / 2;           /* centro do filho */
        char ch;
        int col;

        if(cc < pc){ ch = '/';  col = cc + 1; }
        else if(cc > pc){ ch = '\\'; col = cc - 1; }
        else { ch = '|'; col = cc; }

        grid[row + 1][col] = ch;
        drawLayout(kid, grid);
    }
}

static void freeLayout(LNode *ln)
{
    for(int i = 0; i < ln->nk; i++) freeLayout(ln->kids[i]);
    free(ln->kids);
    free(ln->str);
    free(ln);
}

void printBTree(BTree *bt)
{
    if(bt == NULL || bt->root == NULL) return;

    int maxLevel = 0;
    LNode *root = buildLayout(bt, bt->root, 1, &maxLevel);

    if(root == NULL) return;   /* falha de leitura ou de alocação */

    placeLayout(root, 0);

    int width = root->slot;
    int rows = 2 * maxLevel - 1;   /* uma linha de nós + uma de conectores por nível */

    char **grid = (char**) malloc(rows * sizeof(char*));

    if(grid == NULL){
        freeLayout(root);
        return;
    }

    for(int r = 0; r < rows; r++){
        grid[r] = (char*) malloc(width + 1);

        if(grid[r] == NULL){
            for(int k = 0; k < r; k++) free(grid[k]);
            free(grid);
            freeLayout(root);
            return;
        }

        memset(grid[r], ' ', width);
        grid[r][width] = '\0';
    }

    drawLayout(root, grid);

    /* largura do número do nível, para alinhar o prefixo mesmo com 10+ níveis */
    int dw = 1;
    for(int m = maxLevel; m >= 10; m /= 10) dw++;

    for(int r = 0; r < rows; r++){
        if(r % 2 == 0) printf("Nível %*d: ", dw, r / 2 + 1);
        else printSpaces(8 + dw);
        printf("%s\n", grid[r]);
    }

    for(int r = 0; r < rows; r++) free(grid[r]);
    free(grid);
    freeLayout(root);
}

/**
 * Funções de liberação de memória
 */

void freeBTree(BTree *bt){
    if (bt == NULL) return;

    free(bt->root);
    bt->root = NULL;
    bt->t = 0;
}

/**
 * Função main e menus
 */

void menuInsert(BTree *bt) {
    int op = 0;

    printf("1. Insert N values\n");
    printf("2. Insert one value\n");
    printf("\nSelect option: ");
    if (scanf("%d", &op) != 1) {
        printf("Invalid input\n");
        while (getchar() != '\n');
        return;
    }

    if (op == 1) {
        int n, max;
        printf("Value of N: ");
        if (scanf("%d", &n) != 1) {
            printf("Invalid input\n");
            while (getchar() != '\n');
            return;
        }

        printf("Max value of N: ");
        if (scanf("%d", &max) != 1) {
            printf("Invalid input\n");
            while (getchar() != '\n');
            return;
        }

        if (n <= 0 || max <= 0) {
            printf("Invalid values\n");
            return;
        }

        printf("Entered values: ");
        for (int k = 0; k < n; k++) {
            int num = rand() % max + 1;
            printf("%d ", num);

            if (!insert(bt, num)) {
                printf("\nFailed to insert value %d (disk or memory error)\n", num);
                break;
            }
        }
        printf("\n");

    } else if (op == 2) {
        int value;
        printf("Value to insert: ");
        if (scanf("%d", &value) != 1) {
            printf("Invalid input\n");
            while (getchar() != '\n');
            return;
        }

        if (!insert(bt, value))
            printf("Failed to insert value %d (disk or memory error)\n", value);

    } else {
        printf("\nInvalid option\n");
    }

    printf("\nPress ENTER to continue ");
    getchar();
    getchar();
}

void menuRemove(BTree *bt) {
    int value;

    while (1) {
        printBTree(bt);

        printf("Value to remove (-1 to exit): ");
        if (scanf("%d", &value) != 1) {
            printf("Invalid input\n");
            while (getchar() != '\n');
            continue;
        }

        if (value == -1) {
            break;
        }

        Node *found = search(bt, bt->root, value);
        if (found == NULL) {
            printf("Value %d not found in the tree\n", value);
        } else {
            /* o nó é uma cópia lida do disco: libera antes de remover */
            freeLoadedNode(found, bt->t);

            if (removeKey(bt, value))
                printf("Value %d removed successfully\n", value);
            else
                printf("Failed to remove value %d (disk or memory error)\n", value);
        }

        printf("\nPress ENTER to continue ");
        getchar();
        getchar();
    }
}

void menuSearch(BTree *bt) {
    int value;
    printf("Value to search: ");
    if (scanf("%d", &value) != 1) {
        printf("Invalid input\n");
        while (getchar() != '\n');
        return;
    }

    Node *result = search(bt, bt->root, value);
    if (result != NULL) {
        printf("Value %d FOUND in node: ", value);
        for (int i = 0; i < result->n; i++)
            printf("%d ", result->keys[i]);
        printf("\n");

        freeLoadedNode(result, bt->t);
    } else {
        printf("Value %d not found\n", value);
    }

    printf("\nPress ENTER to continue ");
    getchar();
    getchar();
}

int main(void) {
    BTree bt;
    int t = 0;

    srand((unsigned) time(NULL));

    /* os nós ficam em bin/node_<id>.bin: o diretório precisa existir */
    if (mkdir("bin", 0755) != 0 && errno != EEXIST) {
        printf("Could not create the bin/ directory\n");
        return 1;
    }

    while (1) {
        printf("Define value t of B Tree: ");
        if (scanf("%d", &t) != 1) {
            printf("Invalid input\n");
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }

        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        if (!initialize(&bt, t)) {
            printf("Value of t must be between 2 and 1000\n\n");
            continue;
        }
        break;
    }

    printf("B Tree initialized with t = %d.\n", bt.t);
    printf("\nPress ENTER to continue ");
    getchar();

    int op = 0;
    do {
        system("clear");
        printf("----- Menu -----\n");
        printf("1. Insert value\n");
        printf("2. Remove value\n");
        printf("3. Search value\n");
        printf("4. Print B Tree\n");
        printf("0. Exit\n");
        printf("\nSelect option: ");

        if (scanf("%d", &op) != 1) {
            printf("Invalid input\n");
            break;
        }

        switch (op) {
            case 1:
                system("clear");
                menuInsert(&bt);
                break;
            case 2:
                system("clear");
                menuRemove(&bt);
                break;
            case 3:
                system("clear");
                menuSearch(&bt);
                break;
            case 4:
                system("clear");
                printBTree(&bt);
                printf("\nPress ENTER to continue ");
                getchar();
                getchar();
                break;
            case 0:
                system("clear");
                break;
            default:
                system("clear");
                printf("Invalid option\n");
        }

        printf("\n");
    } while (op != 0);

    freeBTree(&bt);
    return 0;
}