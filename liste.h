#include <stdio.h>
#include <stdlib.h>



typedef struct inmap {

    int id; //l'identificatore dell'attuale nodo
    struct inmap *next; //il puntatore al nodo collegato

} inmap;

void insert_inmap(inmap* head_ptr, int new_id);

int search(inmap* head, int key);

void free_inmap(inmap* head);

void printNode(inmap* head);

int sizeLista(inmap *head);

int cmp(const void *a, const void *b);
