
# Progetto PageRank – Laboratorio II (2023/2024)

## 📖 Introduzione

Questo repository contiene la mia implementazione dell'algoritmo **PageRank** sviluppata in C un ambiente Linux per il corso di Laboratorio II all'Università di Pisa eseguita dal Prof. Giovanni Manzini. Il progetto prevede una parallelizzazione tramite thread POSIX e un'infrastruttura client-server in Python per gestire l'invio e l'elaborazione di grafi.

---

## 🛠️ Caratteristiche principali

### Parallelizzazione tramite Thread
Sono stato utilizzati i thread POSIX per velocizzare il calcolo del PageRank, scegliendo due strategie principali:
- **Suddivisione statica dei nodi** tra i thread per ridurre la necessità di sincronizzazione.
- **Allocazione dinamica dei task** tramite una pila condivisa protetta da mutex, così da bilanciare meglio il carico di lavoro.

### Gestione Memoria
- E' stata implementata una struttura dati efficiente per il grafo (`typdef struct grafo`) per contenere solo le informazioni essenziali degli archi entranti e uscenti.
- Durante la lettura iniziale del file, vengono scartati gli archi duplicati o auto-referenziali, riducendo il consumo di memoria.

### Sincronizzazione dei Thread
- Vengono utilizzati mutex e condition variables per gestire le risorse condivise.
- I thread rimangono attivi durante tutto il processo, evitando overhead continui di creazione e distruzione.

---

## 🔍 Dettagli Implementativi

### Algoritmo e Convergenza
- Viene eseguita la formula standard del PageRank, pre-calcolando il contributo dei nodi dead-end.
- La convergenza dell'algoritmo viene controllata tramite una soglia configurabile di errore assoluto tra iterazioni consecutive.

### Gestione Segnali
- Un thread dedicato mostra informazioni in tempo reale sul calcolo (iterazione corrente e nodo con massimo rank) quando riceve il segnale `SIGUSR1`.

### Linea di Comando
L'eseguibile `pagerank` accetta diverse opzioni configurabili:
```bash
pagerank [-k K] [-m M] [-d D] [-e E] [-t T] infile
```

---

## 🌐 Parte Client-Server (Python)

### Server (`graph_server.py`)
- Server multi-threaded Python in ascolto su una porta locale.
- Gestisce in maniera incrementale la ricezione e il salvataggio del grafo.
- Utilizza `subprocess.run` per chiamare il programma `pagerank` e logga informazioni utili in un file dedicato.

### Client (`graph_client.py`)
- Client Python che invia simultaneamente più grafi al server tramite thread paralleli.
- Output identificato chiaramente per ciascun grafo elaborato.

---

## 🧪 Test e validazione
Ho eseguito diversi test su vari grafi benchmark:

- Nessun problema riscontrato con Valgrind (assenza di memory leak).
- I risultati ottenuti sono consistenti con i risultati attesi forniti.
- L’algoritmo rispetta pienamente le condizioni di convergenza previste.

Esempio di test eseguito:
```bash
valgrind ./pagerank web-Stanford.mtx -k8 1> web-Stanford.rk 2> web-Stanford.log
```

---

## 📁 Struttura Repository
```
progetto/
├── main.c
├── pagerank.c
├── pagerank.h
├── graph_server.py
├── graph_client.py
├── 9nodi.mtx
├── Makefile
└── README.md
```

---

## 📦 Come compilare ed eseguire il progetto

```bash
git clone git@github.com:user/progetto.git testdir
cd testdir
make
./pagerank 9nodi.mtx
```

---
