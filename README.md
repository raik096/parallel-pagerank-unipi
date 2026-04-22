# PageRank Project – Laboratorio II (2023/2024)

## 📖 Introduction

This repository contains my implementation of the **PageRank** algorithm, developed in C within a Linux environment for the Laboratorio II course at the University of Pisa, taught by Prof. Giovanni Manzini. The project features parallelization using POSIX threads and a Python client-server infrastructure to manage the transmission and processing of graphs. Further details can be found in the **progetto.pdf** file.

---

## 🛠️ Main Features

### Thread Parallelization
POSIX threads were used to speed up the PageRank calculation, employing two main strategies:
- **Static node division** among threads to reduce the need for synchronization.
- **Dynamic task allocation** via a shared stack protected by mutexes, to better balance the workload.

### Memory Management
- Implemented an efficient data structure for the graph (`typedef struct grafo`) to contain only the essential information about incoming and outgoing edges.
- During the initial file reading, duplicate or self-referencing edges are discarded, reducing memory consumption.

### Thread Synchronization
- Mutexes and condition variables are used to manage shared resources.
- Threads remain active throughout the entire process, avoiding the continuous overhead of creation and destruction.

---

## 🔍 Implementation Details

### Algorithm and Convergence
- Executes the standard PageRank formula, pre-calculating the contribution of dead-end nodes.
- The algorithm's convergence is checked via a configurable absolute error threshold between consecutive iterations.

### Signal Handling
- A dedicated thread displays real-time computation info (current iteration and the node with the highest rank) upon receiving the `SIGUSR1` signal.

### Command Line
The `pagerank` executable accepts several configurable options:
```bash
pagerank [-k K] [-m M] [-d D] [-e E] [-t T] infile
