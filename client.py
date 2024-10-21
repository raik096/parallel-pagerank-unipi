#! /usr/bin/env python3

import socket
import argparse
import struct
import threading
import os

# valori di default verso cui connettersi 
HOST = "127.0.0.1"  # The server's hostname or IP address
PORT = 54783        # The port used by the server

def main(host=HOST, port=PORT, nome_file=None):
    
    # creazione del socket per la connessione al server
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))

        # apro il file
        with open(nome_file, 'r') as f:

            # ciclo per arrivare al numero delle righe, colonne, archi
            while True: 
                linea = f.readline().strip()
                if linea.startswith('%'):
                    continue
                else:
                    break
            
            # linea ha i valori significativi
            valori = linea.split()
            if len(valori) < 3:
                raise ValueError("Formato della matrice errato: meno di 3 valori trovati.")
                
            nodi = int(valori[0])
            archi = int(valori[2])

            # invio preliminarmente il numero di nodi e archi
            s.sendall(struct.pack("!2i", nodi, archi))
            
            for _ in range(archi):
                linea = f.readline().strip()
                if not linea:
                    break
                nodo_da, nodo_a = linea.split()
                nodo_da = int(nodo_da)
                nodo_a = int(nodo_a)
                # spedisco al server tutti gli archi nel file
                s.sendall(struct.pack('!2i', nodo_da, nodo_a))
        
        # Ricezione risposta dal server
        exit_code = struct.unpack('!I', s.recv(4))[0]
        output = s.recv(1024).decode()  # Aumentato il buffer per ricevere più dati

        print(f"{nome_file} Exit code: {exit_code}")
        for line in output.splitlines():  # Suddivido l'output in righe
            print(f"{nome_file} {line}")         
        print(f"{nome_file} Bye")


def avvio_client(files):
    threads = []
    # avvio un thread per ogni file
    for nome_file in files:
        t = threading.Thread(target=main, args=(HOST, PORT, nome_file))
        # avvio il thread
        t.start()
        # inserisco in lista per il join
        threads.append(t)

    for t in threads:
        t.join()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Specificare i file .mtx da inviare al server')
    parser.add_argument('files', metavar='F', type=str, nargs='+', help='Elenco di file mtx da inviare al server')

    args = parser.parse_args()
    avvio_client(args.files)
