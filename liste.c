#include <stdio.h>
#include <stdlib.h>
//#include "liste.h"


typedef struct inmap {

    int id; //l'identificatore dell'attuale nodo
    struct inmap *next; //il puntatore al nodo collegato

} inmap;

void insertNode(inmap* head, int new_id) {
    if (head->id == -1) {
        // Se il nodo è vuoto, inserisci i dati direttamente in questo nodo
        head->id = new_id;
        head->next = NULL;
        return;
    }

    // Altrimenti, crea un nuovo nodo e inseriscilo all'inizio della lista
    inmap* newNode = (inmap *)malloc(sizeof(inmap));
    if (newNode == NULL) {
        fprintf(stderr, "Errore nell'allocazione della memoria\n");
        exit(1);
    }

    newNode->id = new_id;
    newNode->next = head->next;
    head->next = newNode;
}


//io voglio passargli una cella che contiene una struct
//in modo che lui verifichi l'id e nel caso usi il next
struct inmap* search(inmap* head, int key) {

    inmap* current = head;
    if (head->id == -1) return NULL;
    
    while (current != NULL) {
        if (current->id == key) {
            return current;
        }
        current = current->next;
    }
    return NULL;  // Se non trovato
}


void freeList(inmap* head) {
    inmap* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}


/*
void printNode(inmap* head) {
    if (head == NULL || head->id == -1) {
        fprintf(stderr, "Il nodo non ha archi entranti\n");
        return;
    }

    inmap* current = head;
    while (current != NULL) {
        //fprintf(stderr, "%d --> ", current->id + 1);
        current = current->next;
    }
    fprintf(stderr, "NULL\n");
}
*/

int sizeLista(inmap *head) 
{ 
    if (head == NULL) return 0;
    int count = 0;
    inmap* current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;

}


int cmp(const void *a, const void *b) {
    double *da = *(double **)a;
    double *db = *(double **)b;
    
    if (*da < *db) return 1;
    if (*da > *db) return -1;
    return 0;
}