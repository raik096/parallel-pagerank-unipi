#define _GNU_SOURCE
#include <stdio.h>    
#include <stdlib.h>   
#include <stdbool.h>    
#include <string.h>    
#include <pthread.h>  
#include <errno.h> 
#include <math.h>    
#include "xerrori.h"
#include "liste.h"

#define Buf_size 10 //La size del buffer circolare

typedef struct {        //la struct del mio grafo condiviso
    int N;              //# nodi che appartengono al grafo letto
    int n_archi;        //# archi, capibile dal file mtx
    int n_archi_validi; //# archi validi ovvero non rindondanti ne doppi
    int pilaindici;     //usata per smistare il lavoro tra i threads dinamicamente

    int *out;           //array di interi descrittore del # nodi in uscita dalla cella out[i]
    inmap *in;         //array di puntatori a struttura inmap, inizializzato come puntatore a NULL

    double *x;          //vettore contenente il pageranking alla iterazione "t" dove ogni cella coincide con un nodo = pagina
    double *xnext;      //come sopra ma all'iterazione "t+1"
    double d;           //valore di dumping utile all'algoritmo di pageranking
    double s_t;         //valore s_t utile all'algoritmo PK
    double k;           // " "
    double scarto;      // " "
    int taux;           //# threads ausiliari
    bool apparecchiato; //variabile che coordina il passaggio (dei taux) dalla fase di lettura alla fase di calcolo del PRK
    int thread_in_attesa;

    pthread_cond_t *down;
    pthread_cond_t *up;
    pthread_mutex_t *mu3;
} grafo;


typedef struct {        //la struct dei dati in supporto ai threads

    grafo *ptrG;
    char **buffer;              //buffer tra produttore e consumatore, ed e' un array di puntatori a stringa restituiti da strdup
    int *cindex;                //consumer index

    sem_t *sem_free_slots;      //ptr al semaforo condiviso tra threads cons e prod
    sem_t *sem_data_items;      // " "
    pthread_mutex_t *mu;         //ptr al mutex condiviso tra i threads
    pthread_mutex_t *mu2;        //ptr al secondo mutex condiviso tra i threads
    pthread_barrier_t *barriera;

    int startindex;             //ogni thread inizia da un pt specifico
    int endindex;               //ogni threads finisce ad un pt specifico
    int *size_in;               //l'allocazione dinamica per in condivisa tra i threads
} dati;

volatile bool continua = true;   //queste variabile servono al coordinamento tra il main e i treads
volatile bool avanza = false;    //sono volatile per fermare il compilatore dal ottimizzare il codice

void *tbody(void *arg);
double *pagerank(grafo *g, double d, double eps, int maxiter, int taux, int *numiter);
void free_g_in(grafo* g, int size_in);

int main(int argc, char *argv[])
{
    int opt;                //var in support alla funzione getopt
    int taux = 3;           //val default thread ausiliari
    int maxiter = 100;      //val default max iterazioni possibili
    int shownodes = 3;      //val default nodi mostrati in stdout
    double d = 0.9;         //val default dumping factor
    double eps = 0.0000001; //val default delta tra x e xnext

    while ((opt = getopt(argc, argv, "k:m:d:e:t:")) != -1) {
        switch (opt) {
            case 'k':
                shownodes = atoi(optarg);
                if (shownodes < 1) shownodes = 3;
                break;


            case 'm':
                maxiter = atoi(optarg);
                if (maxiter < 1) maxiter = 100;
                break;

            case 'd':
                d = atof(optarg);
                if (d < 1) d = 0.9;
                break;

            case 'e':
                eps = atof(optarg);
                if (eps < 0) eps = 0.0000001;
                break;

            case 't':
                taux = atoi(optarg);
                if (taux < 1) taux = 3;
                break;

            default:
                fprintf(stderr, "Parametro opzionale errato. Uso %s file [file...] [-k shownodes] [-m maxiter] [-d damping factor] [-e eps] [-t taux]\n", argv[0]);
                exit(1);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Inserire almeno un file. Uso %s file [file...] [-k shownodes] [-m maxiter] [-d damping factor] [-e eps] [-t taux]\n", argv[0]);
        exit(1);
    }

    grafo *g = malloc(sizeof(grafo)); //alloco una porzione di memoria per il grafo
    if (g == NULL) {
        perror("Errore nell'allocazione della memoria per il grafo\n");
        exit(EXIT_FAILURE);
    }

    dati *a = malloc(sizeof(dati) * taux); //alloco un'array di dati da dedicare ad ogni taux                         //alloco la memoria necessaria a contenere la strytt
    if (a == NULL) {
        perror("Errore nell'allocazione della memoria per la struct in supporto ai threads\n");
        free(g);
        exit(EXIT_FAILURE);
    }
    g->n_archi_validi = 0;
    g->taux           = taux;
    g->d              = d;
    g->s_t            = 0;
    g->scarto         = 0.0;
    g->apparecchiato  = false;   //per "apparecchiato" intendo il momento in cui non sono ancora allocati i vettori necessari al calcolo del pagerankin ma i thread hanno finito di leggere
    g->thread_in_attesa = 0;

    pthread_t t[taux]; //creazione statica dell'array contente i threads, ad ogni cella avro' un tid
    sem_t sem_free_slots, sem_data_items;
    pthread_barrier_t barriera;

    pthread_cond_t up = PTHREAD_COND_INITIALIZER;    //la variabile che serve ai taux per svegliare il main thread
    pthread_cond_t down = PTHREAD_COND_INITIALIZER;  //la variaible che serve al main per fare il broadcast sui threads

    pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;  //primo mutex
    pthread_mutex_t mu2 = PTHREAD_MUTEX_INITIALIZER; //secondo mutex
    pthread_mutex_t mu3 = PTHREAD_MUTEX_INITIALIZER; //terzo mutex

    xsem_init(&sem_free_slots, 0, Buf_size,__LINE__,__FILE__);   //creo semaforo prod/cons
    xsem_init(&sem_data_items, 0, 0, __LINE__,__FILE__);         //creo semaforo prod/cons
    pthread_barrier_init(&barriera, NULL, taux);                   //creo l'unica barriera

    //---------------- inizio fase lettura ---------------------

    FILE *file = fopen(argv[optind], "r");
    if (file == NULL) {
        perror("Errore nell'aprire il file");
        free(g);
        free(a);
        exit(EXIT_FAILURE);
    }

    int datiG[3];       //in 0: #RIGHE, 1:#COLONNE, 2:#ARCHI
    size_t size = 0;
    char *linea = NULL; //in modo che la getline allochi la memoria autonomamente
    char *token = NULL;

    while(getline(&linea, &size, file) != -1)  //la getline legge un intera linea dallo stream, salvando il ptr in linea
    {
        token = strtok(linea, " "); //riceve la linea dalla getline
        if (*token == '%') {
            continue;
        } else {
            break;
        }
    }

    datiG[0] = atoi(token);     //# RIGHE

    token = strtok(NULL, " ");
    datiG[1] = atoi(token);    // # COLONNE

    token = strtok(NULL, " ");
    datiG[2] = atoi(token);    // # ARCHI
    free(linea);

    if (datiG[0] != datiG[1]) {
        fprintf(stderr, "Errore: il file MatrixMarket non descrive una matrice di adiacenza\n");
        free(g);
        free(a);
        fclose(file);
        exit(1);
    }
    g->N       = datiG[0];  //questo campo conterra' gli N nodi del grafo relativo al file passato
    g->n_archi = datiG[2];  //numero archi

    int size_in  = 10;         //parto con una size bassa sull'allocazione della memoria dedicata a g->in
    g->out       = calloc(g->N, sizeof(int));
    g->in        = malloc(sizeof(inmap) * size_in);
    g->pilaindici = (g->N)-1; //necessario per la suddivisione di carico dinamica tra i threads
    g->mu3       = &mu3;
    g->up        = &up;
    g->down      = &down;
    
    if (g->out == NULL || g->in == NULL) {
        perror("Errore nell'allocazione memoria IN/OUT del grafo\n");
        free(g->out);
        free(g->in);
        free(g);
        free(a);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    for (int k = 0; k < 10; k++) {
        g->in[k].id  = -1;     //inizializzo g->in
        g->in[k].next = NULL;
    }

    linea = NULL;             //per scongiurare possibili errori di getline alla prossima chiamata
    char *buffer[Buf_size];   //verranno contenuti tutti i puntatori
    int pindex = 0;           //indice del produttore
    int cindex = 0;           //indice del consumatore

    double chunk = floor((double)g->N/(double)taux); //suddivido il carico tra taux

    for (int i = 0; i < taux; i++) {                   //creazione sottothreads
        a[i].startindex = chunk * i;                   //l'indice di partenza del thread i-esimo
        if (i < taux-1) {
            a[i].endindex = chunk * (i+1);             //l'indice di arrivo nel caso non sia l'ultimo
        } else {
                a[i].endindex = g->N;                  //l'indice di arrivo nel caso sia l'ultimo
        }
        a[i].ptrG           = g;
        a[i].buffer         = buffer;                  //il buffer verra' popolato da puntatori a carattere, restituiti dal thread main con la funzione strdup
        a[i].cindex         = &cindex;                 //tutti i sotto threads fanno riferimento ad un indice consumatore
        a[i].size_in        = &size_in;
        a[i].mu             = &mu;
        a[i].mu2            = &mu2;
        a[i].barriera       = &barriera;
        a[i].sem_data_items = &sem_data_items;          //il primo semaforo condiviso tra tutti i threads
        a[i].sem_free_slots = &sem_free_slots;          //il secondo semaforo condiviso tra tutti i threads

        xpthread_create( &t[i], NULL, &tbody, &a[i], __LINE__, __FILE__ );  //da qui iniziano tutti le funzioni TSAFE ecc...
    }

    while (getline(&linea, &size, file) != -1)
    {
        xsem_wait(&sem_free_slots, __LINE__,__FILE__);      //aspetta che ci siano dei posti liberi
        buffer[pindex++ % Buf_size] = strdup(linea);        //aggiorno senza mutex
        xsem_post(&sem_data_items, __LINE__, __FILE__);     //dice che ci sono dei dati sul buffer
        //linea = NULL;
    }
    free(linea);
    fclose(file);

    for (int i = 0; i < taux; i++) {                        //inserisco valore di terminazione ai consumatori
        xsem_wait(&sem_free_slots, __LINE__, __FILE__);
        buffer[pindex ++ % Buf_size] = "-1";
        xsem_post(&sem_data_items, __LINE__, __FILE__);
    }

    //---------------- fine fase lettura ---------------------

    //--------------- inizio fase calcolo --------------------
    int numiter = 0;
    double *x = pagerank(g, d, eps, maxiter, taux, &numiter);
    for (int i = 0; i < taux; i++)
        xpthread_join(t[i], NULL, __LINE__, __FILE__);

    int deadnodes = 0;
    double sum_of_ranks = 0.0;
    double **ptr_x = malloc(sizeof(double *) * g->N);        //creo un array di puntatori
    if (ptr_x == NULL) {
        perror("Errore nell'allocazione array finale per la classifica");
        free(g->out);
        free(g->x);
        free_g_in(g, *a->size_in);
        free(g);
        free(a);
        exit(EXIT_FAILURE);
    }

    for (int de = 0; de < g->N; de++)
    {
        if (g->out[de] == 0) deadnodes++;
        sum_of_ranks += x[de];
        ptr_x[de] = &x[de];
        fprintf(stderr, "ptr_x[%d]: %p, &x[%d]: %p, x[%d]: %lf\n", de, (void*)ptr_x[de], de, (void*)&x[de], de, x[de]);
    }
    qsort(ptr_x, (g->N), sizeof(double *), cmp); //ordina in un classifica

    fprintf(stdout, "Number of nodes: %d\nNumber of dead-end nodes: %d\nNumber of valid arcs: %d\n", g->N, deadnodes, g->n_archi_validi);
    fprintf(stdout, "Converged after %d iterations\n", numiter);
    fprintf(stdout, "Sum of ranks: %f\n", sum_of_ranks);

    int k = 0;
    fprintf(stdout, "Top %d nodes:\n", shownodes);
    while (shownodes != k)
    {
        fprintf(stdout, " %ld %f\n", (ptr_x[k]-x), *ptr_x[k]);
        k++;
    }
    
    free(ptr_x);
    free(g->out);
    free(g->x);
    free_g_in(g, *a->size_in);
    free(g);
    free(a);

    return 0;
}

void *tbody(void *arg)
{
    dati *a = (dati *)arg;
    grafo *g = a->ptrG;

    int temp[2];

    while(true)
    {
        xsem_wait(a->sem_data_items, __LINE__, __FILE__); //aspettano che ci siano dei dati sul buffer
        xpthread_mutex_lock(a->mu, __LINE__,__FILE__);    //un thread prende l'esclusiva

        char *stringa = a->buffer[*(a->cindex) % Buf_size];     //il thread legge dal buffer e aggiorna inializza stringa
        *(a->cindex) += 1;                                //il thread aggiorna la variabile
        xpthread_mutex_unlock(a->mu, __LINE__, __FILE__); //il thread rilascia il mutex
        xsem_post(a->sem_free_slots, __LINE__,__FILE__);

        if (strcmp(stringa, "-1") == 0)
            break;

        char *var_r;
        char *token = strtok_r(stringa, " ", &var_r); // Spezza la stringa e salva il primo valore
        temp[0] = atoi(token) - 1;                   // Salva il nodo di partenza in temp[0]
        token = strtok_r(NULL, " ", &var_r);         // Ottiene il secondo pezzo della stringa
        temp[1] = atoi(token) - 1;                   // Salva il nodo di arrivo in temp[1]

        free(stringa);

        if (temp[0] != temp[1]) { // Evita l'arco rindondante

            xpthread_mutex_lock(a->mu2, __LINE__, __FILE__); // Lock per sincronizzazione
            // Se l'indice di IN è maggiore della dimensione attuale, rialloca la memoria
            if (temp[1] >= *a->size_in) {

                // Rialloca la memoria per g->in
                int new_size_in = temp[1] + 1;
                inmap *new_in = realloc(g->in, new_size_in * sizeof(inmap));
                if (new_in == NULL) {
                    perror("Errore durante la riallocazione della memoria g->in");
                    exit(EXIT_FAILURE);
                }
                g->in = new_in;

                // Inizializza le nuove celle allocate
                for (int k = *a->size_in; k < new_size_in; k++) {
                    g->in[k].id = -1;
                    g->in[k].next = NULL;
                }
                *a->size_in = new_size_in;
            }
            // Inserisce il nodo solo se l'arco non è già presente
            if (search(&g->in[temp[1]], temp[0]) == 1) {
                insert_inmap(&g->in[temp[1]], temp[0]);
                g->out[temp[0]]++;
                g->n_archi_validi++;
            }
            xpthread_mutex_unlock(a->mu2, __LINE__, __FILE__); // Rilascia il lock
        }
    }

    xpthread_mutex_lock(g->mu3, __LINE__, __FILE__);
    while(g->apparecchiato == false)
        xpthread_cond_wait(g->down, g->mu3, __LINE__,__FILE__);
    xpthread_mutex_unlock(g->mu3, __LINE__, __FILE__);

    //posso calcolare il primo fattore di x^(t+1) una sola volta, distribuisco il carico tra i threads
    double val = 1.0/(double)g->N; //prima iterazione, inizializzazione del vettore x
    for (int i = a->startindex; i < a->endindex; i++)
        g->x[i] = val;

    while (continua)
    {
        double tot1 = 0.0;
        //calcolo valore s_t riazzerandolo ad ogni nuova iterazione
        pthread_barrier_wait(a->barriera);  
        //all'alba della nuova iterazione c'e' bisogno che il vettore xnext sia diventato x

        //calcolo il secondo fattore del vettore x^(t+1) distribuendo il carico 
        for (int i = a->startindex; i < a->endindex; i++) {
            if (g->out[i] == 0) 
                tot1 += g->x[i];
        }

        tot1 = (g->d/(double)(g->N)) * tot1;
        xpthread_mutex_lock(a->mu, __LINE__, __FILE__);    //la barriera mi garantisce che il mutex: "mu", sia libero da racecondition
        g->s_t += tot1;
        xpthread_mutex_unlock(a->mu, __LINE__, __FILE__);

        //nel momento della somma del vettore c'e' bisogno che g->s_t sia calcolato da tutti
        pthread_barrier_wait(a->barriera); //new
        //calcolo il terzo fattore del vettore x^(t+1)
        while (true)
        {   
            //non piu' distribuzione di carico ma meccanismo a pila di indici dinamico a consumo
            xpthread_mutex_lock(a->mu, __LINE__, __FILE__);
            int j = g->pilaindici; //il thread si prende in carico l'indice j
            if (j == -1) {         //sono arrivato alla fine dei nodi da calcolare, mi metto in attesa dei check della funzione
                xpthread_mutex_unlock(a->mu, __LINE__, __FILE__);
                break;
            }
            g->pilaindici--;       //decremento la pila

            xpthread_mutex_unlock(a->mu, __LINE__, __FILE__);

            double tot2 = 0.0;
            inmap *ptrNodo = &g->in[j]; //g->in[j] a questa fase e' safe da scritture
            while (ptrNodo != NULL && ptrNodo->id != -1) //scorre il ptr, facendo un doppio check che esiste e che non sia gia' stato inizializzato
            {
                tot2 += g->x[ptrNodo->id]/(double)g->out[ptrNodo->id];
                ptrNodo = ptrNodo->next;
            }
            tot2 = tot2 * g->d;
            g->xnext[j] = g->k + g->s_t + tot2;
        }
        //tutti i thread avranno aggiornato xnext su tutti i j indici
        pthread_barrier_wait(a->barriera);

        double scarto = 0.0;
        for (int i = a->startindex; i < a->endindex; i++)
        {
            scarto += fabs(g->xnext[i] - g->x[i]);
            g->x[i] = g-> xnext[i];
        }
        xpthread_mutex_lock(a->mu2, __LINE__, __FILE__);
        g->scarto += scarto;
        xpthread_mutex_unlock(a->mu2, __LINE__, __FILE__);

        xpthread_mutex_lock(g->mu3, __LINE__, __FILE__);
        g->thread_in_attesa++;
        xpthread_cond_signal(g->up, __LINE__, __FILE__);
        //while (g->pilaindici != (g->N)-1)
        xpthread_cond_wait(g->down, g->mu3, __LINE__, __FILE__);
        xpthread_mutex_unlock(g->mu3, __LINE__, __FILE__);
    }
    pthread_exit(NULL);
}

double *pagerank(grafo *g, double d, double eps, int maxiter, int taux, int *numiter)
{
    g->k = (1.0 - d) / g->N;                    //questo lo posso calcolare solo una volta prima di tutte le iterazioni
    g->x = malloc(sizeof(double)*g->N);         //vettore contenente il pagerank di tutti i nodi
    g->xnext = malloc(sizeof(double)*g->N);     //vettore contenente il pagerank t+1 di tutti i nodi
    if (g->x == NULL || g->xnext == NULL) {
        perror("Errore nell'allocazione della memoria per il calcolo del PageRank\n");
        exit(EXIT_FAILURE);
    }

    xpthread_mutex_lock(g->mu3, __LINE__, __FILE__);
    g->apparecchiato = true;
    xpthread_cond_broadcast(g->down,__LINE__,__FILE__);
    xpthread_mutex_unlock(g->mu3, __LINE__, __FILE__);

    do {
        
        (*numiter)++;

        xpthread_mutex_lock(g->mu3, __LINE__, __FILE__);
        while(g->thread_in_attesa != taux)
            xpthread_cond_wait(g->up, g->mu3, __LINE__, __FILE__);
        g->thread_in_attesa = 0;
        
        if ((*numiter) >= maxiter) break;
        if (eps > g->scarto) break;

        g->scarto     = 0.0;
        g->s_t        = 0.0;
        g->pilaindici = (g->N)-1;

        xpthread_cond_broadcast(g->down, __LINE__, __FILE__);
        xpthread_mutex_unlock(g->mu3, __LINE__, __FILE__);

    } while(true);

    continua = false;
    xpthread_cond_broadcast(g->down, __LINE__, __FILE__);
    xpthread_mutex_unlock(g->mu3, __LINE__, __FILE__);

    free(g->xnext);
    return g->x;
}

void free_g_in(grafo* g, int size_in) {
    for (int i = 0; i < size_in; i++) {
        if (g->in[i].id != -1) {
            free_inmap(g->in[i].next); //libero la lista collegata da g->in[i]
        }
    }
    free(g->in); 
}