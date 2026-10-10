#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>

/**
 * Estruturas da Arvore B
 */

typedef struct Node{
	int *keys, n, leaf;
	struct Node **children;                
} Node;

typedef struct {
    Node *root;
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

void splitNode(BTree *bt, Node *dad, int i);
Node* createNode(int t, int leaf);
void insertNonFull(BTree *bt, Node *node, int key);
void insert(BTree *bt, int key);

int findKeyIndex(Node *node, int key);
int getPred(Node *node);
int getSucc(Node *node);
void borrowFromPrev(Node *node, int idx);
void borrowFromNext(Node *node, int idx);
void merge(BTree *bt, Node *node, int idx);
void fill(BTree *bt, Node *node, int idx);
void removeFromLeaf(Node *node, int idx);
void removeFromInternal(BTree *bt, Node *node, int idx);
void removeFromNode(BTree *bt, Node *node, int key);
void removeKey(BTree *bt, int key);

void freeNode(Node *node);
void freeBTree(BTree *bt);


/**
 * Funções de manipulação dos arquivos
 */



/**
 * Funções de inserção da estrutura
 */

void splitNode(BTree *bt, Node *dad, int i){
	int t = bt->t;
	Node *child = dad->children[i];
	Node *newn = createNode(t, child->leaf);
	
	int half = t - 1;
	int prom = child->keys[half];
	
	for(int j = half + 1; j < child->n; j++){
		newn->keys[newn->n] = child->keys[j];
		newn->n++;
	}
	
	if(!child->leaf){
		for(int j = half + 1; j <= child->n; j++){
			newn->children[j - (half + 1)] = child->children[j];
			child->children[j] = NULL;
		}
	}
	
	child->n = half;
	
	for(int j = dad->n; j > i; j--)
		dad->children[j + 1] = dad->children[j];
	
	dad->children[i + 1] = newn;
	
	for(int j = dad->n; j > i; j--)
		dad->keys[j] = dad->keys[j - 1];
		
	dad->keys[i] = prom;
	dad->n++;
}

Node* createNode(int t, int leaf){
	Node *newn = (Node*) malloc(sizeof(Node));
	newn->n = 0;
	newn->leaf = leaf;
	
	newn->keys = (int*) malloc((2*t - 1) * sizeof(int));
	newn->children = (Node**) malloc((2*t) * sizeof(Node*));
	
	for(int i = 0; i < 2*t; i++)
		newn->children[i] = NULL;
	
	return newn;
}

void insertNonFull(BTree *bt, Node *node, int key){
	int t = bt->t;
	int i = node->n - 1;
	
	if(node->leaf){
		while(i >= 0 && key < node->keys[i]){
			node->keys[i + 1] = node->keys[i];
			i--;
		}
		
		node->keys[i + 1] = key;
		node->n++;
		return;
	}
	
	i = 0;
	while( i < node->n && key > node->keys[i])
		i++;
	
	if(node->children[i]->n == 2*t - 1){
		splitNode(bt, node, i);
		if(key > node->keys[i])
			i++;
	}
	
	insertNonFull(bt, node->children[i], key);
}

void insert(BTree *bt, int key){
	int t = bt->t;
	
	if(bt->root == NULL){
		bt->root = createNode(t, 1);
		bt->root->keys[0] = key;
		bt->root->n = 1;
		return;
	}
	
	Node *root = bt->root;
	if(root->n == 2*t - 1){
		Node *newn = createNode(t, 0);
		newn->children[0] = root;
		bt->root = newn;
		
		splitNode(bt, newn, 0);
		
		int i = (key > newn->keys[0]) ? 1 : 0;
		insertNonFull(bt, newn->children[i], key);
	} else{
		insertNonFull(bt, root, key);
	}
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

int getPred(Node *node){
	while(!node->leaf)
		node = node->children[node->n];
	
	return node->keys[node->n - 1];
}

int getSucc(Node *node){
	while(!node->leaf)
		node = node->children[0];
	
	return node->keys[0];
}

void borrowFromPrev(Node *node, int idx){
	Node *child = node->children[idx];
	Node *sibling = node->children[idx - 1];
	
	for(int j = child->n - 1; j >= 0; j--)
		child->keys[j + 1] = child->keys[j];
	
	if(!child->leaf){
		for(int j = child->n; j >= 0; j--)
			child->children[j + 1] = child->children[j];
	}
	
	child->keys[0] = node->keys[idx - 1];
	
	if(!child->leaf){
		child->children[0] = sibling->children[sibling->n];
		sibling->children[sibling->n] = NULL;
	}
	
	node->keys[idx - 1] = sibling->keys[sibling->n - 1];
	
	child->n++;
	sibling->n--;
}

void borrowFromNext(Node *node, int idx){
	Node *child = node->children[idx];
	Node *sibling = node->children[idx + 1];
	
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
}

void merge(BTree *bt, Node *node, int idx){
	int t = bt->t;
	Node *child = node->children[idx];
	Node *sibling = node->children[idx + 1];
	
	child->keys[t - 1] = node->keys[idx];
	
	for(int j = 0; j < sibling->n; j++)
		child->keys[j + t] = sibling->keys[j];
	
	if(!child->leaf){
		for(int j = 0; j <= sibling->n; j++)
			child->children[j + t] = sibling->children[j];
	}
	
	for(int j = idx + 1; j < node->n; j++)
		node->keys[j - 1] = node->keys[j];
	
	for(int j = idx + 2; j <= node->n; j++)
		node->children[j-1] = node->children[j];
	
	node->children[node->n] = NULL;
	child->n += sibling->n + 1;
	node->n--;
	
	free(sibling->keys);
	free(sibling->children);
	free(sibling);
}

void fill(BTree *bt, Node *node, int idx){
	int t = bt->t;
	
	if(idx != 0 && node->children[idx - 1]->n >= t)
		borrowFromPrev(node, idx);
	else if(idx != node->n && node->children[idx + 1]->n >= t)
		borrowFromNext(node, idx);
	else{
		if(idx != node->n)
			merge(bt, node, idx);
		else
			merge(bt, node, idx - 1);
	}
}

void removeFromLeaf(Node *node, int idx){
	for(int j = idx + 1; j < node->n; j++)
		node->keys[j - 1] = node->keys[j];
	
	node->n--;
}

void removeFromInternal(BTree *bt, Node *node, int idx){
	int t = bt->t;
	int k = node->keys[idx];
	
	if(node->children[idx]->n >= t){
		int pred = getPred(node->children[idx]);
		node->keys[idx] = pred;
		removeFromNode(bt, node->children[idx], pred);
	} else if(node->children[idx + 1]->n >= t){
		int succ = getSucc(node->children[idx + 1]);
		node->keys[idx] = succ;
		removeFromNode(bt, node->children[idx + 1], succ);
	} else{
		merge(bt, node, idx);
		removeFromNode(bt, node->children[idx], k);
	}
}

void removeFromNode(BTree *bt, Node *node, int key){
	int t = bt->t;
	int idx = findKeyIndex(node, key);
	
	if(idx < node->n && node->keys[idx] == key){
		if(node->leaf)
			removeFromLeaf(node, idx);
		else
			removeFromInternal(bt, node, idx);
	} else{
		if(node->leaf)
			return;
		
		if(node->children[idx]->n < t)
			fill(bt, node, idx);
		
		if(idx > node->n)
			removeFromNode(bt, node->children[idx - 1], key);
		else
			removeFromNode(bt, node->children[idx], key);
	}
}

void removeKey(BTree *bt, int key){
	if(bt->root == NULL) return;
	
	removeFromNode(bt, bt->root, key);
	
	if(bt->root->n == 0){
		Node *old = bt->root;
		
		if(old->leaf)
			bt->root = NULL;
		else
			bt->root = old->children[0];
		
		free(old->keys);
		free(old->children);
		free(old);
	}
}

/**
 * Funções de busca da estrutura
 */

Node* search(Node *root, int k){
    if(root == NULL) return NULL;

    int i = 0;

    while(i < root->n && k > root->keys[i]) 
        i++;

    if(i < root->n && root->keys[i] == k) return root;
    if(root->leaf) return NULL;

    return search(root->children[i], k);
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

/* Fase 1: monta a árvore de layout e calcula larguras (de baixo para cima) */
static LNode* buildLayout(Node *node, int level, int *maxLevel)
{
    LNode *ln = (LNode*) malloc(sizeof(LNode));
    ln->level = level;
    ln->nk = 0;
    ln->kids = NULL;
    ln->span = 0;

    if(level > *maxLevel) *maxLevel = level;

    ln->str = (char*) malloc(node->n * 12 + 8);
    nodeString(node, ln->str);
    ln->w = (int) strlen(ln->str);

    if(!node->leaf){
        ln->kids = (LNode**) malloc((node->n + 1) * sizeof(LNode*));
        for(int j = 0; j <= node->n; j++){
            if(node->children[j] != NULL){
                LNode *kid = buildLayout(node->children[j], level + 1, maxLevel);
                ln->kids[ln->nk++] = kid;
                ln->span += kid->slot;
            }
        }
        if(ln->nk > 1) ln->span += GAP * (ln->nk - 1);
    }

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
    LNode *root = buildLayout(bt->root, 1, &maxLevel);
    placeLayout(root, 0);

    int width = root->slot;
    int rows = 2 * maxLevel - 1;   /* uma linha de nós + uma de conectores por nível */

    char **grid = (char**) malloc(rows * sizeof(char*));
    for(int r = 0; r < rows; r++){
        grid[r] = (char*) malloc(width + 1);
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

void freeNode(Node *node){
    if (node == NULL) return;

    if (!node->leaf) {
        for (int i = 0; i <= node->n; i++)
            freeNode(node->children[i]);
    }

    free(node->keys);
    free(node->children);
    free(node);
}

void freeBTree(BTree *bt){
    if (bt == NULL) return;
    freeNode(bt->root);
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
        return;
    }

    if (op == 1) {
        int n, max;
        printf("Value of N: ");
        scanf("%d", &n);
        printf("Max value of N: ");
        scanf("%d", &max);

        if (n <= 0 || max <= 0) {
            printf("Invalid values\n");
            return;
        }

        printf("Entered values: ");
        for (int k = 0; k < n; k++) {
            int num = rand() % max + 1;
            printf("%d ", num);
            insert(bt, num);
        }
        printf("\n");

    } else if (op == 2) {
        int value;
        printf("Value to insert: ");
        if (scanf("%d", &value) != 1) {
            printf("Invalid input\n");
            return;
        }
        insert(bt, value);

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

        Node *found = search(bt->root, value);
        if (found == NULL) {
            printf("Value %d not found in the tree\n", value);
        } else {
            removeKey(bt, value);
            printf("Value %d removed successfully\n", value);
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
        return;
    }

    Node *result = search(bt->root, value);
    if (result != NULL) {
        printf("Value %d FOUND in node: ", value);
        for (int i = 0; i < result->n; i++)
            printf("%d ", result->keys[i]);
        printf("\n");
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