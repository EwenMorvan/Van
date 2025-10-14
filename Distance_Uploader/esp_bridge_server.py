import serial
import threading
import socket
import subprocess
import time
import signal
import sys
import os
import cv2  # Ajout pour la webcam
import numpy as np

# ----------------------------
# CONFIGURATION
# ----------------------------
ESP_PORTS = {
    "MainPCB": "COM9",
    "SlavePCB": "COM8"
}
BAUDRATE = 115200
SERVER_HOST = "0.0.0.0"
SERVER_PORT = 5000

# chemins temporaires
TEMP_BOOTLOADER_PATH = "temp_bootloader.bin"
TEMP_PARTITION_TABLE_PATH = "temp_partition.bin"
TEMP_BIN_PATH = "temp_firmware.bin"

# ----------------------------
# VARIABLES GLOBALES
# ----------------------------
clients = []
serials = {}
running = True

# ----------------------------
# SIGNAL HANDLER
# ----------------------------
def signal_handler(sig, frame):
    global running
    print("\n[INFO] Arrêt du serveur...")
    running = False
    try:
        srv.close()
    except:
        pass
    for s in serials.values():
        try:
            s.close()
        except:
            pass
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# ----------------------------
# OUVERTURE DES PORTS COM
# ----------------------------
for name, port in ESP_PORTS.items():
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=1)
        serials[name] = ser
        print(f"[OK] {name} connecté sur {port}")
    except Exception as e:
        print(f"[ERREUR] Impossible d'ouvrir {port} : {e}")

# ----------------------------
# THREAD LECTURE SERIE
# ----------------------------
def serial_reader(name, ser):
    while running:
        try:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                msg = f"LOG:{name}:{line}\n"
                for c in clients:
                    try:
                        c.sendall(msg.encode())
                    except:
                        pass
        except:
            time.sleep(0.1)

for n, s in serials.items():
    threading.Thread(target=serial_reader, args=(n, s), daemon=True).start()

# ----------------------------
# UTILITAIRE RECV EXACT
# ----------------------------
def recv_all(conn, length):
    data = b""
    while len(data) < length:
        packet = conn.recv(length - len(data))
        if not packet:
            raise ConnectionError("Client déconnecté pendant la réception")
        data += packet
    return data

# ----------------------------
# RECEPTION D'UN FICHIER (CORRIGÉ)
# ----------------------------
def receive_file(conn, temp_path):
    print(f"[INFO] Attente de la taille du fichier pour {temp_path}...")
    
    # Lire 16 bytes pour la taille
    size_bytes = recv_all(conn, 16)
    file_size = int(size_bytes.decode().strip())
    print(f"[INFO] Taille annoncée: {file_size} bytes")
    
    # Envoyer l'ACK (exactement 8 bytes)
    conn.sendall(b"SIZE_OK\n")
    
    # Attendre un court instant pour être sûr que l'ACK est parti
    time.sleep(0.01)
    
    # Lire exactement file_size bytes
    print(f"[INFO] Réception de {file_size} bytes...")
    data = recv_all(conn, file_size)
    
    with open(temp_path, "wb") as f:
        f.write(data)
    
    actual_size = os.path.getsize(temp_path)
    print(f"[INFO] Fichier reçu : {temp_path} ({actual_size} bytes)")
    
    if actual_size != file_size:
        raise RuntimeError(f"Taille incorrecte! Attendu: {file_size}, Reçu: {actual_size}")

# ----------------------------
# THREAD GESTION CLIENT TCP
# ----------------------------
def handle_client(conn, addr):
    print(f"[+] Client connecté depuis {addr}")
    clients.append(conn)
    try:
        while running:
            data = conn.recv(1024)
            if not data:
                break
            cmd = data.decode().strip()
            print(f"[CMD] {cmd}")

            # Ajout gestion commande caméra
            if cmd == "GET_CAM":
                try:
                    cap = cv2.VideoCapture(0)
                    ret, frame = cap.read()
                    cap.release()
                    if ret:
                        # Encode en JPEG
                        _, img_encoded = cv2.imencode('.jpg', frame)
                        img_bytes = img_encoded.tobytes()
                        # Envoi au client
                        conn.sendall(b"CAM_IMG:" + img_bytes)
                    else:
                        print("[ERREUR] Capture caméra échouée")
                except Exception as e:
                    print(f"[ERREUR CAM] {e}")
                continue

            if cmd.startswith("UPLOAD_"):
                target = cmd.split("_")[1]
                print(f"[INFO] Début upload pour {target}")
                conn.sendall(b"READY\n")
                
                time.sleep(0.05)  # Petite pause pour synchronisation

                try:
                    # Réception stop-and-wait pour chaque fichier
                    print("[INFO] === Réception Bootloader ===")
                    receive_file(conn, TEMP_BOOTLOADER_PATH)
                    
                    print("[INFO] === Réception Partition Table ===")
                    receive_file(conn, TEMP_PARTITION_TABLE_PATH)
                    
                    print("[INFO] === Réception Firmware ===")
                    receive_file(conn, TEMP_BIN_PATH)
                    
                    print("[INFO] Tous les fichiers reçus avec succès!")

                    if target in serials:
                        port = ESP_PORTS[target]
                        conn.sendall(f"FLASHING:{target}\n".encode())

                        # fermer temporairement le port COM
                        ser = serials[target]
                        if ser.is_open:
                            ser.close()

                        # commande flash complète
                        cmd_flash = f"python -m esptool --chip esp32s3 --port {port} -b 460800 " \
                                    f"--before default_reset --after hard_reset write_flash " \
                                    f"--flash_mode dio --flash_freq 80m --flash_size 8MB " \
                                    f"0x0 {TEMP_BOOTLOADER_PATH} " \
                                    f"0x10000 {TEMP_BIN_PATH} " \
                                    f"0x8000 {TEMP_PARTITION_TABLE_PATH}"
                        
                        print(f"[INFO] Exécution de la commande flash...")
                        
                        # Fonction pour lire avec gestion des \r (retours chariot)
                        def read_with_carriage_return(pipe, conn, target):
                            import sys
                            while True:
                                char = pipe.read(1)
                                if not char:
                                    break
                                
                                # Afficher dans le terminal du serveur (comportement natif)
                                sys.stdout.write(char)
                                sys.stdout.flush()
                                
                                # Pour le client, on envoie caractère par caractère
                                # et il gérera l'affichage
                                try:
                                    conn.sendall(f"FLASH_CHAR:{target}:{char}".encode())
                                except:
                                    pass
                        
                        # Lancer le processus SANS buffer et en mode texte
                        import os
                        env = os.environ.copy()
                        env['PYTHONUNBUFFERED'] = '1'
                        
                        process = subprocess.Popen(
                            cmd_flash,
                            shell=True,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            universal_newlines=True,
                            bufsize=0,
                            env=env
                        )
                        
                        # Lire dans un thread
                        reader_thread = threading.Thread(
                            target=read_with_carriage_return,
                            args=(process.stdout, conn, target),
                            daemon=True
                        )
                        reader_thread.start()
                        
                        result = process.wait()
                        reader_thread.join(timeout=2)
                        
                        if result == 0:
                            print(f"[OK] Flash réussi pour {target}")
                        else:
                            print(f"[ERREUR] Flash échoué pour {target} (code: {result})")

                        # rouvrir le port COM
                        serials[target] = serial.Serial(port, BAUDRATE, timeout=1)
                        threading.Thread(target=serial_reader, args=(target, serials[target]), daemon=True).start()

                        conn.sendall(f"OK:UPLOAD_{target}\n".encode())
                    else:
                        conn.sendall(f"ERROR:Target {target} not found\n".encode())
                        
                except Exception as e:
                    print(f"[ERREUR] Lors de la réception des fichiers: {e}")
                    conn.sendall(f"ERROR:{e}\n".encode())

            elif cmd.startswith("RESET_"):
                target = cmd.split("_")[1]
                if target in serials:
                    ser = serials[target]
                    ser.setDTR(False)
                    ser.setRTS(True)
                    time.sleep(0.1)
                    ser.setRTS(False)
                    conn.sendall(f"OK:RESET_{target}\n".encode())
                else:
                    conn.sendall(f"ERROR:Target {target} not found\n".encode())
            elif cmd.endswith("_CLICK"):
                # Simulation d'un click bouton
                target = None
                if cmd == "SW_CLICK":
                    target = "MainPCB"
                else:
                    # Pour SlavePCB: BE1_CLICK, etc.
                    for btn in ["BE1", "BE2", "BD1", "BD2", "BH"]:
                        if cmd == f"{btn}_CLICK":
                            target = "SlavePCB"
                            break
                if target and target in serials:
                    # Envoyer sur le port série (simuler un événement)
                    serials[target].write((cmd + "\n").encode())
                    conn.sendall(f"OK:{cmd}\n".encode())
                else:
                    conn.sendall(f"ERROR:Target for {cmd} not found\n".encode())
                continue

    except Exception as e:
        print(f"[ERREUR CLIENT] {e}")
    finally:
        if conn in clients:
            clients.remove(conn)
        conn.close()
        print("[-] Client déconnecté")

# ----------------------------
# SERVEUR TCP PRINCIPAL
# ----------------------------
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind((SERVER_HOST, SERVER_PORT))
srv.listen(1)
srv.settimeout(1.0)
print(f"[SERVEUR] En écoute sur {SERVER_HOST}:{SERVER_PORT}")

try:
    while running:
        try:
            conn, addr = srv.accept()
            threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()
        except socket.timeout:
            continue
except KeyboardInterrupt:
    signal_handler(None, None)