# PageRank---LAB-II
Progetto finale completo anno 2023/2024

######### MAIN.C #########

Il programma main.c usa sostanzialmente due meccanismi di suddivisione del lavoro con i thread:

1) Dividera' il numero dei nodi con il numero dei threads in modo da suddividere il carico di lavoro equamente, lavorando direttamente sull'array coinvolto senza meccanismi di sincronizzazione.

2) Usera' una "pila" di indici, dove ogni thread sotto mutex, cercherà di accedere all'intero (quando parte o quando finisce il calcolo della i'esima pagina). Se la variabile di mutex è sbloccata allora la bloccherà, legge l'indice, lo decrementa di 1 (l'indice max == # nodi - 1) e il thread, dopo aver sbloccato la variabile mutex, si prenderà in carico il calcolo dell'iterazione t+1, sulla pagina di indice trovato. una volta finito, come tutti gli altri thread, re interrogherà il mux per trovare l'indice su cui lavorare. Tutto ciò finché l'intero è -1.

Nella fase di:
- lettura dal grafo viene usato il metodo 1)
- nella fase di calcolo del pageranking vengono usati 1), 2), 1) in modo da lavorare dove possibile direttamente sull'array. Una volta che tutti i thread hanno calcolato il vettore xnext, restituiranno il risultato al main thread, si metteranno in attesa, quest'ultimo controllerà lo scarto e se viene superato (oppure si raggione l'iter max) allora i threads verranno fermati e si restituira' alla funzione main il vettore x.

######### SERVER_GRAPH.PY #########

Il programma server_graph.py invece tiene una lista globale di tutti i thread attivi, ogni thread quando termina si toglie dalla lista (dietro protezione di un mutex). Avvia un loop dal quale puo' uscire solo con SIGINT, all'interno di questo loop appena avviene una connessione viene dedicato un thread a quella connessione specifica, e il thread viene aggiunto alla lista globale. La chiusura del socket avviene dopo il join di tutti i thread rimasti attivi da parte dell'signale handler.

######### SERVER_CLIENT.PY #########

Nel programma client_graph.py viene creato un thread per ogni file, ad ogni thread viene incaricato la gestione del file







