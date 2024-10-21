#! /usr/bin/env python3
import sys
import signal
import struct
import socket
import tempfile
import subprocess
import logging
import threading

# Configuro il logging
logging.basicConfig(filename='server.log', level=logging.INFO, 
                    format='%(asctime)s - %(levelname)s - %(message)s')

# Specifica da dove accettare le connessioni
HOST = "127.0.0.1"
PORT = 54783

# Variabili globali per tenere traccia degli archi
archi_validi = 0
archi_scartati = 0
threads = []
procedi = True
s = None  # Socket globale per poterlo chiudere
lock = threading.Lock()


def gestisci_connessione(conn, addr):
    global archi_validi, archi_scartati
    
    try:
        with conn:
            data = recv_all(conn, 8)
            assert len(data) == 8, "Errore ricezione interi"

            num_nodi = struct.unpack("!i", data[:4])[0]
            num_archi = struct.unpack("!i", data[4:])[0]

            buffer_scrittura = []

            with tempfile.NamedTemporaryFile(delete=False, mode='w') as temp_file:
                temp_file.write(f"{num_nodi} {num_nodi} {num_archi}\n")

                archi_validi = 0
                archi_scartati = 0
                
                for _ in range(num_archi):
                    arco = recv_all(conn, 8)
                    assert len(arco) == 8
                    i = struct.unpack("!i", arco[:4])[0]
                    j = struct.unpack("!i", arco[4:])[0]
                    
                    if 0 <= i < num_nodi and 0 <= j < num_nodi:
                        buffer_scrittura.append(i)
                        buffer_scrittura.append(j)
                        archi_validi += 1
                        
                        # Faccio gruppi da 10 o piu' prima di scrivere sul file tmp
                        if len(buffer_scrittura) >= 10:
                            for k in range(0, 10, 2):
                                temp_file.write(f"{buffer_scrittura[k]} {buffer_scrittura[k + 1]}\n")
                            buffer_scrittura = buffer_scrittura[10:]
                    else:
                        archi_scartati += 1
                
                # Scrivo gli archi rimanenti
                for k in range(0, len(buffer_scrittura), 2):
                    temp_file.write(f"{buffer_scrittura[k]} {buffer_scrittura[k + 1]}\n")

            # Esegui il programma pagerank
            risultato = subprocess.run(["./pagerank", temp_file.name], capture_output=True, text=True)
            if risultato.returncode == 0:
                conn.sendall((0).to_bytes(4, 'big'))
                conn.sendall(risultato.stdout.encode())
            else:
                exit_code = max(0, risultato.returncode)
                conn.sendall(exit_code.to_bytes(4, 'big'))
                conn.sendall(risultato.stderr.encode())

            # Log delle informazioni
            logging.info(f"\nNodi totali: {num_nodi}\nNome file tmp: {temp_file.name}\n"
                         f"Archi scartati: {archi_scartati}\nArchi validi: {archi_validi}\n"
                         f"EXIT CODE di PageRank: {risultato.returncode}\n")

    finally:
        with lock:
            threads.remove(threading.current_thread())
        conn.close()

def recv_all(conn, n):
    chunks = b''
    bytes_recd = 0
    while bytes_recd < n:
        chunk = conn.recv(min(n - bytes_recd, 800))
        if len(chunk) == 0:
            raise RuntimeError("socket connection broken")
        chunks += chunk
        bytes_recd += len(chunk)
    return chunks



def avvio_server():
    global s
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()

        # faccio una lista di threads accettati fino a che procedi non diventa false
        while procedi:
            try:
                conn, addr = s.accept()
                if procedi == False:
                    conn.close()
                    break
                t = threading.Thread(target=gestisci_connessione, args=(conn, addr))
                with lock:
                    threads.append(t)  # aggiungo il taux alla lista
                t.start()
            except OSError:
                break
    print("Bye dal server")

        


def handler_signal_sigint(signum, frame):
    global procedi
    procedi = False
    for t in threads:
        t.join()
    s.close()

if __name__ == "__main__":
    # gestisco il segnale sigint ad hoc
    signal.signal(signal.SIGINT, handler_signal_sigint)
    avvio_server()

    
