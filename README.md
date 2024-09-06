# PageRank---LAB-II
Progetto finale completo anno 2023/2024

!! NON ANCORA PRONTO !!

bozza calcolo pageranking parallelizzato:

La soluzione proposta sarebbe: creare una struct { d = 1-d/N; double *x; double *xnext; double *scarto; double *s_t; } nella prima fase della gestione creo p + 1 threads dove nella fase iniziale, dividerò l'array N-1 in N-1/p porzioni a cui ad ogni porzione incaricherò uno specifico thread al calcolo di una porzione di s_t e una porzione di scarto che manderò in una porzione di memoria condivisa con il mainthread che una volta atteso della stessa operazione sugli altri thread, farà il merge dei risultati parziali. A questo punto il lavoro del calcolo della nuova iterazione t+1 verrà gestita tramite un solo intero e un solo mutex. Ogni thread cercherà di accedere all'intero quando inizia la prima volta o quando finisce il calcolo della i'esima pagina, quello che farà è una lock sul mutex, se la variabile di mutex è sbloccata allora la bloccherà, ne prenderà possesso, incrementerà l'intero di 1 (facciamo che inizia da 0) e il thread, dopo aver sbloccato la variabile mutex, si prenderà in carico il calcolo dell'iterazione t+1, sulla pagina 0 (in questo caso). una volta finito, come tutti gli altri thread, re interrogherà il mux per trovare l'indice su cui lavorare. Tutto ciò finché l'intero è <= N. Una volta che tutti i thread hanno calcolato il vettore xnext, restituiscono il risultato al main thread, e si mettono in attesa, quest'ultimo controllerà lo scarto e se viene superato allora killa i threads e restituisce alla funzione main il vettore xnext.

