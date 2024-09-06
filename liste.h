#include <stdio.h>
#include <stdlib.h>



typedef struct inmap {

    int id; //l'identificatore dell'attuale nodo
    struct inmap *next; //il puntatore al nodo collegato

} inmap;

void insertNode(inmap* head_ptr, int new_id);

struct inmap* search(inmap* head, int key);

void freeList(inmap* head);

void printNode(inmap* head);

int sizeLista(inmap *head);

int cmp(const void *a, const void *b);
