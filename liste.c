#include <stdio.h>
#include <stdlib.h>



typedef struct inmap {
    int id; // identificatore del nodo
    struct inmap *next; // puntatore al nodo collegato
} inmap;


void insert_inmap(inmap* head, int new_id) {
    if (head->id == -1) {
        // se il nodo è vuoto, inserisci i dati direttamente in questo nodo
        head->id = new_id;
        head->next = NULL;
        return;
    }

    // altrimenti, crea un nuovo nodo e inseriscilo all'inizio della lista
    inmap* newNode = (inmap *)malloc(sizeof(inmap));
    if (newNode == NULL) {
        fprintf(stderr, "Errore nell'allocazione della memoria\n");
        exit(1);
    }

    newNode->id = new_id;
    newNode->next = head->next;
    head->next = newNode;
}

void free_inmap(inmap* node) {
    while (node != NULL) {
        inmap* temp = node; 
        node = node->next;  
        free(temp);         
    }
}

// io voglio passargli una cella che contiene una struct
// in modo che lui verifichi l'id e nel caso usi il next
int search(inmap* head, int val) {

    if (head->id == -1) 
        return 1;       // allora e' vuoto e inserisco
    
    inmap* temp = head;
    
    while (temp != NULL) {
        if (temp->id == val) {
            return -1;  // riscontrato e non inserisco
        }
        temp = temp->next;
    }
    return 1;           // non trovato e inserisco
}


int sizeLista(inmap *head) 
{ 
    if (head->id == -1) return 0;
    int count = 0;
    inmap* current = head;
    while (current->next != NULL) {
        count++;
        current = current->next;
    }

    return count;

}


int cmp(const void *a, const void *b) {
    double diff = **(double**)a - **(double**)b;
    return (diff < 0) - (diff > 0); // ritorna 1, -1, o 0 per stabilire l'ordine
}
