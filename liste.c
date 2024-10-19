#include <stdio.h>
#include <stdlib.h>



typedef struct inmap {
    int id; // Identificatore del nodo
    struct inmap *next; // Puntatore al nodo collegato
} inmap;


void insert_inmap(inmap* head, int new_id) {
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

/*
void free_inmap(inmap* head) {
    inmap* temp = head;
    inmap* prox;
    
    while (temp != NULL) {
        prox = current->next; // Salva il puntatore al prossimo nodo
        free(temp);              // Liberiamo il nodo corrente
        temp = next_node;        // Passa al prossimo nodo
    }
}
*/
void free_inmap(inmap* node) {
    while (node != NULL) {
        inmap* temp = node; 
        node = node->next;  
        free(temp);         
    }
}

//io voglio passargli una cella che contiene una struct
//in modo che lui verifichi l'id e nel caso usi il next
int search(inmap* head, int val) {

    if (head->id == -1) 
        return 1;       //allora e' vuoto e inserisco
    
    inmap* temp = head;
    
    while (temp != NULL) {
        if (temp->id == val) {
            return -1;  //riscontrato e non inserisco
        }
        temp = temp->next;
    }
    return 1;           //non trovato e inserisco
}

/*
void free_inmap(inmap* head) {
    inmap* temp = NULL;

    while (head != NULL && head->id != -1) {
        temp = head;
        head = head->next;
        free(temp);
    }
}
*/

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
    if (head->id == -1) return 0;
    int count = 0;
    inmap* current = head;
    while (current->next != NULL) {
        count++;
        current = current->next;
    }

    return count;

}

/*
int cmp(const void *a, const void *b) {
    double *da = *(double **)a;
    double *db = *(double **)b;
    
    if (*da < *db) return 1;
    if (*da > *db) return -1;
    return 0;

int cmp(const void *a, const void *b) {
    double da = **(double **)a; // Dereferenzia a e poi dereferenzia il puntatore a double
    double db = **(double **)b; // Dereferenzia b e poi dereferenzia il puntatore a double

    if (da < db) return 1;     // Se da è minore di db, ritorna -1 per l'ordinamento crescente
    if (da > db) return -1;      // Se da è maggiore di db, ritorna 1
    return 0;                   // Se sono uguali, ritorna 0
}
}*/


int cmp(const void *a, const void *b) {
    double diff = **(double**)a - **(double**)b;
    return (diff < 0) - (diff > 0); // Ritorna 1, -1, o 0 per stabilire l'ordine
}
