#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>  // Include per fabs
#include "xerrori.h"
#include "liste.h"  // Assumendo che contenga la definizione di `inmap`

#define Buf_size 10

typedef struct grafo
{
    int n;           // Numero nodi appartenenti a G
    int pilaindici;
    int n_archi;
    int *out;        // Puntatore ad array con il numero di archi uscenti dal nodo indice
    int archi_validi;
    inmap *in;       // Array con la lista di nodi corrispondenti agli archi entranti nel nodo indice
    double *x;
    double *xnext;
    double d;
    double s_t;
    double k;
    double scarto;
    int taux;
    int iterazione;

} grafo;


//struttura su cui c'e' la roba che serve ai threads
typedef struct dati
{
    char **buffer; // il buffer che viene condiviso dal produttore/consumatorI 
    //pthread_mutex_t *mu;
    int *pcindex; //forse l'indice a cui si rifa' il consumatore
    char *stringa;
    int *temp;
    //int fase;
    grafo *ptrG; //puntatore al grafo
    sem_t *sem_free_slots;
    sem_t *sem_data_items;
    int startindex;
    int endindex;


} dati;

typedef struct dati_sig
{
    int *iter_attuale;
    double *x;
} dati_sig;

//___ GLOBAL VAR ___

//int fase = 0;

int thread_in_attesa = 0;
bool stop = false;
volatile bool continua = true;
pthread_mutex_t mu;
pthread_cond_t cond;
pthread_cond_t up;
pthread_barrier_t barriera;
#define SIGINT = 1;

void *tbody(void *arg); //quello che fa il thread lettura
void *sighandler(void *arg);
double *pagerank(grafo *g, double d, double eps, int maxiter, int thaux, int *numiter); //funzione che mi calcola il page ranking

//----------------------------- INIZIO MAIN --------------------------------------

int main(int argc, char *argv[]) {
    //for (int z = 0; z < 1; z++)
    //{
    
    int opt; 
    int taux = 3;              //thread ausiliari
    int maxiter = 100;         //il numero di iterazioni massime
    int shownodes = 3;         //i primi numero di nodi che voglio vedere in classifica
    double d = 0.9;            //dumping factor che mi serve per saltare da un posto all'altro
    double eps = 0.0000001;    //eps quindi l'errore entro il quale non mi e' sufficiente fermarmi

    //getopt mi prende come argmento argc quindi il numero totale di stringhe sulla linea di comando
    //poi mi prende argv, puntatore alla prima stringa,     
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

    //questo significa che ci sono solo istruzioni opzionali senza il file
    if (optind >= argc) {
        fprintf(stderr, "Inserire almeno un file. Uso %s file [file...] [-k shownodes] [-m maxiter] [-d damping factor] [-e eps] [-t taux]\n", argv[0]);
        exit(1);
    } else {
        //fprintf(stderr, "Chiamata da linea di comando corretta, avvio programma con parametri:\n  shownodes: %d\n  maxiter: %d\n  damping factor: %f\n  eps: %.10g\n  taux: %d\n...\n",
        //    shownodes, maxiter, d, eps, taux);
    }

    grafo *g = (grafo *)malloc(sizeof(grafo));
    if (g == NULL) {
        fprintf(stderr, "Errore nell'allocazione della memoria per il grafo\n");
        //fclose(file);
        exit(1);
    }
    
    
    dati *a = (dati *)malloc(sizeof(dati) * taux);
    if (a == NULL) {
        fprintf(stderr, "Errore nell'allocazione array struct per i threads");
        exit(1);
    }
    struct sigaction sa;
    pthread_t srHandler;
    sigset_t maschera;                              //struttura dove rappresento il segnale
    sa.sa_handler = &srHandler;
    sigemptyset(&maschera);                         //inizializzo maschera come insieme vuoto
    sigaddset(&maschera, SIGUSR1);                  //aggiungo all'insieme il segna SIGUSR1
    pthread_sigmask(SIG_BLOCK, &maschera, NULL);    //blocco il segnale al mainthread
    sigaction(SIGINT, &sa, NULL);

    g->archi_validi = 0;
    g->iterazione = 0;
    g->taux = taux;
    g->d = d;
    g->scarto = 0.0;
    int pindex = 0;
    int cindex = 0;
    char *buffer[Buf_size];
    //pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    pthread_t t[taux];
    sem_t sem_free_slots, sem_data_items;

    pthread_cond_init(&up, NULL);
    pthread_cond_init(&cond, NULL);
    xpthread_mutex_init(&mu, NULL, __LINE__, __FILE__);
    pthread_barrier_init(&barriera, NULL, taux);
    xsem_init(&sem_free_slots, 0, Buf_size,__LINE__,__FILE__);
    xsem_init(&sem_data_items, 0, 0, __LINE__,__FILE__);

    //dati_sig *a2 = (dati_sig*)malloc(sizeof(dati_sig));
    //a2->x = g->x;
    //a2->iter_attuale = &(g->iterazione); 
    xpthread_create(&srHandler, NULL, &sighandler, NULL, __LINE__, __FILE__);
    fprintf(stderr, "IL SIGNAL THREAD E' PARTITO");
    

    for (int i = 0; i < taux; i++) {
        a[i].temp = calloc(2,sizeof(int));
        a[i].buffer = buffer;
        a[i].ptrG = g;
        a[i].pcindex = &cindex;
        //a[i].fase = 0;
        //a[i].mu = &mu;
        a[i].sem_data_items = &sem_data_items;
        a[i].sem_free_slots = &sem_free_slots;
        xpthread_create( &t[i], NULL, &tbody, &a[i], __LINE__, __FILE__ );
    }   


    FILE *file = fopen(argv[optind], "r");
    if (file == NULL) {
        fprintf(stderr, "Non ho trovato il file\n");
        exit(1);
    } else {
        //fprintf(stderr, "Il file è stato aperto correttamente...\n");
    }

    int *datiG = malloc(sizeof(int) * 3);
    if (datiG == NULL) {
        fprintf(stderr, "Errore nell'allocazione dell'array\n");
            exit(1);
    } else { 
        //fprintf(stderr, "Accensione memoria di primo livello...\n");
    }

    size_t size = 0;
    //int contatorepuntatoridebug = 0;
    bool partenza = true;
    char *linea = NULL;
    int i = 0;    
    //finche' ho roba da leggere leggo
    while (getline(&linea, &size, file) != -1) {

        if (partenza) {
    
            char *token = strtok(linea, " ");
            if (*token == '%') continue;

            datiG[0] = atoi(token);    // # RIGHE
            token = strtok(NULL, " "); 
            datiG[1] = atoi(token);    // # COLONNE
            token = strtok(NULL, " ");
            datiG[2] = atoi(token);    // # ARCHI

            //controllo matrice
            if (datiG[0] != datiG[1]) {
                    fprintf(stderr, "Errore: il file MatrixMarket non descrive una matrice di adiacenza\n");
                    free(datiG);
                    free(linea);
                    fclose(file);
                    exit(1);
            } else { 
                    //fprintf(stderr, "Il FILE è corretto, proseguo...\n");
                }

            g->n = datiG[0];                            //inizializzo g->n: #nodi
            
            // approfitto subito a splittare l'array X sui threads
            int range = floor((double)g->n/(double)taux);    
            for(int i = 0; i < taux; i++) {

                a[i].startindex = range * i;
                //se eccede l'array arriva fino all'ultimo indice, senno' tranqui
                if (i == taux-1 ) {
                    a[i].endindex = (g->n);
            
                } else {
                    a[i].endindex = range * (i+1); 
                }
                assert(a[i].startindex >= 0);
                assert(a[i].endindex <= g->n);
                assert(a[i].startindex <= a[i].endindex);
            }

            g->pilaindici = g->n-1;
            g->n_archi = datiG[2]; 
            g->s_t = 0;                     //inizilizzo g->n_archi
            g->out = (int*)malloc(sizeof(int) * g->n);        //alloco memoria vettore g->out
            g->in = (inmap*)malloc(sizeof(inmap) * g->n);       //alloco memoria vettore g->in

            if (g->out == NULL || g->in == NULL) {
                    fprintf(stderr, "Errore nell'allocazione memoria IN/OUT del grafo\n");
                    free(datiG);
                    free(linea);
                    if (g->out) free(g->out);
                    if (g->in) free(g->in);
                    fclose(file);
                    exit(1);
            } else {
                    //fprintf(stderr, "Accensione memoria terzo e quarto livello...\n");
            }
                
                //inizilizzo vettori g->in/ g->out
            for (i = 0; i < g->n; i++) {
                g->out[i] = 0;              
                g->in[i].id = -1;
                g->in[i].next = NULL;
            }
            partenza = false;
            //fprintf(stderr, "Ho messo la partenza a false\n");

        
        } else { 
                
            xsem_wait(&sem_free_slots, __LINE__, __FILE__);
            buffer[pindex++ % Buf_size] = strdup(linea);
            //fprintf(stderr, "Ho scritto %d puntatori nel buffer\n", contatorepuntatoridebug++);
            xsem_post(&sem_data_items, __LINE__,__FILE__);
        }
        linea = NULL;
    }
    free(linea);
        
    fputs("Tutte le linee lette con successo...\n",stderr);

    for(int i = 0; i < taux; i++) {

        //fprintf(stderr, "il tread indice %d parte da %d a %d escluso\n", i, a[i].startindex, a[i].endindex);

        xsem_wait(&sem_free_slots,__LINE__,__FILE__);
        buffer[pindex++ % Buf_size]= "-1";
        xsem_post(&sem_data_items,__LINE__,__FILE__);
    }

    fputs("-------- FASE LETTURA IN COMPLETAMENTO --------\n",stderr);

    // Calcolo del PageRank
    int numiter = 0;
    //pthread_barrier_init(&barriera, NULL, taux);
    double *x = pagerank(g, d, eps, maxiter, taux, &numiter);
    
    //aspetto che tutti i thread abbiano finito prima di fare il calcolo del page ranking
    for (int i = 0; i < taux; i++) {
        xpthread_join(t[i], NULL, __LINE__, __FILE__);
    }
    pthread_kill(srHandler, SIGUSR1);
    xpthread_join(srHandler, NULL, __LINE__, __FILE__);

    fputs("-------- FASE CALCOLO IN COMPLETAMENTO --------\n",stderr);


    int deadnodes = 0;
    double sum_of_ranks = 0.0;
    double *ptr_x[g->n];

    // Poi, nel ciclo:
    for (int de = 0; de < g->n; de++) {
        if (g->out[de] == 0) deadnodes++;
        sum_of_ranks += x[de];
        ptr_x[de] = &x[de];
    }


    qsort(ptr_x, (g->n), sizeof(double *), cmp); //ordina in un classifica

    fprintf(stdout, "Number of nodes: %d\nNumber of dead-end nodes: %d\nNumber of valid arcs: %d\n", g->n, deadnodes, g->archi_validi);
    fprintf(stdout, "Converged after %d iterations\n", numiter);
    fprintf(stdout, "Sum of ranks: %f\n", sum_of_ranks);

    int k = 0;
    fprintf(stdout, "Top %d nodes:\n", shownodes); 
    while (shownodes != k)
    {
        fprintf(stdout, "%ld %f\n", (ptr_x[k]-x), *ptr_x[k]);    
        k++;
    }
    
    free(g->out);
    free(g->in);
    free(g);

    return 0;
}

//----------------------------- FINE MAIN ------------------------------------------------

//----------------------------- INIZIO THREADS LETTORI ----------------------------------
void *tbody(void *arg) 
{
    dati *a = (dati *)arg;      //in a ho i dati necessari al funzionamento del thread
    grafo *g = a->ptrG;
    //pthread_barrier_t* barriera = (pthread_barrier_t*)arg;
    //a->stringa;

    fprintf(stderr,"Consumatore %d partito\n",gettid());
    while (true) 
    
    {

        
        xsem_wait(a->sem_data_items, __LINE__, __FILE__);//aspetto che ci sono elementi salvati nel buffer buttati dal produttore
        xpthread_mutex_lock(&mu, __LINE__, __FILE__);//se li trovo blocco il mutex
        
        a->stringa = a->buffer[*(a->pcindex) % Buf_size];
        assert(a->stringa!=NULL);
        *(a->pcindex) += 1;
        if (strcmp(a->stringa, "-1") == 0) {
            xpthread_mutex_unlock(&mu,__LINE__,__FILE__);
            break;
        }
        xpthread_mutex_unlock(&mu,__LINE__,__FILE__);

        //una volta consumata la linea
        //int ]temp[2] = {0};
        char *save;

        char *token = strtok_r(a->stringa, " ", &save);
        assert(token!=NULL);
        a->temp[0] = atoi(token) - 1; // Nodo di partenza
        token = strtok_r(NULL, " ", &save); 
        assert(token!=NULL);
        a->temp[1] = atoi(token) - 1; // Nodo di arrivo
        //fprintf(stderr, "Il consumatore %d ha letto: %d %d\n", gettid(), temp[0] + 1, temp[1] + 1);


        if (search(&(g->in[a->temp[1]]), a->temp[0]) == NULL && a->temp[0] != a->temp[1]) {

            //fprintf(stderr, "Sto acquisendo %d --> %d scrivendoli nella cella: %d\n", temp[0] + 1, temp[1] + 1, temp[0]);
            xpthread_mutex_lock(&mu, __LINE__, __FILE__);//se li trovo blocco il mutex
            insertNode(&g->in[a->temp[1]], a->temp[0]);
            fprintf(stderr, "Ho acquisito %d --> %d\n", a->temp[0] + 1, a->temp[1] + 1);
            g->out[a->temp[0]]++;
            g->archi_validi++;
            xpthread_mutex_unlock(&mu,__LINE__,__FILE__);

        }
        xsem_post(a->sem_free_slots,__LINE__,__FILE__);
    
    }; //io fermo tutto quando il main thread imposta il buffer a -1


    pthread_barrier_wait(&barriera);

    //prima iterazione, inizializzazione del vettore x
    double val = 1.0/(double)g->n;
    for (int i = a->startindex; i < a->endindex; i++) {
        
        //fprintf(stderr, "\t\tendindex: %d\n", a->endindex);
        g->x[i] = val;

        //fprintf(stderr, "~~~~~~~~ Ho scritto %f in %d ~~~~~~~~~~~~~~~~~\n", val, i);
    }
    
    //pthread_barrier_wait(&barriera);

    while (!stop)
    {
        //fprintf(stderr,"Consumatore %d e' entrato nel while\n",gettid());

        //----calcolo del valore s_t

        double tot1 = 0.0;
        pthread_barrier_wait(&barriera);


        for (int i = a->startindex; i < a->endindex; i++) {
            
            if (g->out[i] == 0) {
                tot1 += g->x[i];
            }
            //fprintf(stderr,"Consumatore %d ha calcolato tot1 sull'indice %d\n",gettid(), i);

        }
        tot1 = tot1 * (g->d/ (double)(g->n));
        pthread_mutex_lock(&mu);
        g->s_t += tot1;
        pthread_mutex_unlock(&mu);

        //fprintf(stderr,"-------------> s_t e' uguale a %f\n", g->s_t);

        pthread_barrier_wait(&barriera);
        //////////////////////////////////////////////////////////////// <-- BARRIERA

        while (true) {
            

            xpthread_mutex_lock(&mu, __LINE__, __FILE__); //se li trovo blocco il mutex
            int j = g->pilaindici;    
            if (j == -1) {
                xpthread_mutex_unlock(&mu, __LINE__,__FILE__);
                //fprintf(stderr,"il thread %d si ferma\n",gettid());
                break;

            }               //il thread si prende in carico la pagina j-esima
            g->pilaindici--;
            //fprintf(stderr,"il thread ha decrementato la pila, adesso e' a: %d\n",g->pilaindici);
            xpthread_mutex_unlock(&mu, __LINE__,__FILE__);
            
            double tot2 = 0.0;
            inmap *ptrNodo = &g->in[j]; //l'indirizzo della cella j-esima
            while (ptrNodo != NULL && ptrNodo->id != -1) {
                
                tot2 += g->x[ptrNodo->id] / g->out[ptrNodo->id];
                ptrNodo = ptrNodo->next;
                //fprintf(stderr, "Il thread %d sta contribuendo e ha calcolato %f\n", gettid(), tot2);
            }
            tot2 = tot2 * g->d;
            g->xnext[j] = g->k + g->s_t + tot2;
    
        }

        pthread_barrier_wait(&barriera);

        for (int i = a->startindex; i < a->endindex; i++) {
            g->scarto += fabs(g->xnext[i] - g->x[i]);
            //fprintf(stderr,"\n______ %f ____-_____ %f______ = %f\n",g->xnext[i], g->x[i], g->scarto);
            g->x[i] = g->xnext[i];
        }

        
        pthread_mutex_lock(&mu);
        //devo sincronizzare con il mainthred
        thread_in_attesa++;

        pthread_cond_signal(&up);  // oppure pthread_cond_broadcast, se necessario
        
        //fprintf(stderr,"il thread %d SI E' MESSO IN ATTESA, in totale sono %d e deve essere minore o uguale a %d\n",gettid(), thread_in_attesa, g->taux);
        pthread_cond_wait(&cond, &mu);

        // Rilasciamo il mutex
        pthread_mutex_unlock(&mu);
        
    }

    fprintf(stderr,"Consumatore %d sta per terminare\n",gettid());
    pthread_exit(NULL);

}

//----------------------------- FINE THREADS LETTORI ----------------------------------


//---------------------- INIZIO CALCOLO PAGERANK --------------------

double *pagerank(grafo *g, double d, double eps, int maxiter, int taux, int *numiter) {

    g->k = (1.0 - d) / g->n;
    g->x = (double*)malloc(sizeof(double)*g->n);         //vettore contenente il pagerank di tutti i nodi
    g->xnext = (double*)malloc(sizeof(double)*g->n);     //vettore contenente il pagerank t+1 di tutti i nodi 
    //double *y = malloc(sizeof(double) * g->n);         //vettore contenente il pagerank dei nodi dead-end
    if (g->x == NULL || g->xnext == NULL) {
        fprintf(stderr, "Errore nell'allocazione della memoria per il calcolo del PageRank\n");
        return NULL;
    }

    do {
        g->iterazione++;

        pthread_mutex_lock(&mu);
        while (thread_in_attesa != taux) {
            
            pthread_cond_wait(&up, &mu);
            //fprintf(stderr, "\n\nmi hannos svegliato e il thread in attesa e' %d\n\n", thread_in_attesa);
        }
        thread_in_attesa = 0;
        //fprintf(stderr, "\n\nsono alla %d iterazione\n\n", g->iterazione);

        //fprintf(stderr,"\n\n Se %d supera o e' uguale a %d finisce\n\n Se %f che e' lo scarto supera %f finisce\n",g->iterazione, maxiter, eps, g->scarto);

        if (g->iterazione >= maxiter) break;
        if (eps > g->scarto) break;
        
        g->scarto = 0.0;
        g->s_t = 0;
        g->pilaindici = (g->n)-1; //reimposto la pilaindici per la prox iterazione

        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mu);

    } while (continua);
    stop = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mu);

    *numiter = g->iterazione;
    return g->x;
}

//----------------------------- FINE CALCOLO PAGE RANKING ----------------------------------

void *sighandler(void *arg) 
{   
    //dati_sig *a2 = (dati_sig *)arg;      //in a ho i dati necessari al funzionamento del thread
    
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    
    while(true) {
        sigwait(&set, &sig);

        
        fprintf(stderr, "\nIteration actually: %d\nTop Node: %d\nPageRank: %f\n", 0, 1, 1.0);
        continua = false;
        break;
    }
    
    kill(getpid(), SIGUSR1); //se ho finito tutto mi uccido da solo
}