import socket
import threading
import tkinter as tk
from tkinter import scrolledtext, ttk
import os
import time
import re

# --- Configuration ---
SERVER_IP = "192.168.1.14"  # IP du PC portable
SERVER_PORT = 5000

BIN_PATHS = {
    "MainPCB_Image": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\MainPCB\VanManagement\build\VanManagement.bin",
    "MainPCB_Bootloader": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\MainPCB\VanManagement\build\bootloader\bootloader.bin",
    "MainPCB_Partition": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\MainPCB\VanManagement\build\partition_table\partition-table.bin",
    "SlavePCB_Image": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\SlavePCB\WaterManagement\build\WaterManagement.bin",
    "SlavePCB_Bootloader": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\SlavePCB\WaterManagement\build\bootloader\bootloader.bin",
    "SlavePCB_Partition": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\SlavePCB\WaterManagement\build\partition_table\partition-table.bin"
}

client = None
connected = False
upload_client = None  # Socket dédié pour l'upload

# --- ANSI colors ---
ANSI_COLORS = {
    '30': 'black', '31': 'red', '32': 'green', '33': 'yellow',
    '34': 'blue', '35': 'magenta', '36': 'cyan', '37': 'white'
}

# --- GUI setup ---
root = tk.Tk()
root.title("ESP32 Remote Manager")

frames = {}
textboxes = {}
progressbars = {}

# --- Fonction pour gérer les couleurs ANSI ---
def insert_ansi_text(txt_widget, text):
    pattern = re.compile(r'\x1b\[(\d+;)?(\d+)m')
    last_end = 0
    current_color = 'black'
    for m in pattern.finditer(text):
        txt_widget.insert(tk.END, text[last_end:m.start()], ('fg_'+current_color,))
        color_code = m.group(2)
        current_color = ANSI_COLORS.get(color_code, 'black')
        last_end = m.end()
    txt_widget.insert(tk.END, text[last_end:], ('fg_'+current_color,))
    txt_widget.see(tk.END)

def append_log(name, msg):
    txt = textboxes[name]
    insert_ansi_text(txt, msg + "\n")

def clear_log(name):
    textboxes[name].delete(1.0, tk.END)

def reset_esp(name):
    if connected:
        client.sendall(f"RESET_{name}\n".encode())
        append_log(name, "[INFO] Reset envoyé")
    else:
        append_log(name, "[ERREUR] Pas connecté au serveur")

# --- Fonction pour recevoir exactement N bytes ---
def recv_exact(sock, n):
    data = b""
    while len(data) < n:
        packet = sock.recv(n - len(data))
        if not packet:
            raise ConnectionError("Connexion fermée pendant la réception")
        data += packet
    return data

# --- Fonction pour recevoir jusqu'à un délimiteur ---
def recv_until(sock, delimiter=b'\n', timeout=5.0):
    sock.settimeout(timeout)
    data = b""
    try:
        while delimiter not in data:
            chunk = sock.recv(1)
            if not chunk:
                raise ConnectionError("Connexion fermée")
            data += chunk
        return data.decode().strip()
    finally:
        sock.settimeout(None)

# --- Envoi fichier fiable (CORRIGÉ) ---
def send_file(sock, temp_path):
    size = os.path.getsize(temp_path)
    print(f"[DEBUG] Envoi taille: {size} bytes pour {temp_path}")
    
    # Envoi de la taille (16 bytes)
    sock.sendall(str(size).encode().ljust(16))
    
    # Réception de l'ACK (exactement 8 bytes: "SIZE_OK\n")
    ack = recv_exact(sock, 8).decode().strip()
    print(f"[DEBUG] ACK reçu: {ack}")
    
    if ack != "SIZE_OK":
        raise RuntimeError(f"Serveur non prêt pour le fichier (reçu: {ack})")
    
    # Envoi du fichier
    with open(temp_path, "rb") as f:
        total_sent = 0
        while total_sent < size:
            chunk = f.read(4096)
            if not chunk:
                break
            sock.sendall(chunk)
            total_sent += len(chunk)
    
    print(f"[DEBUG] Fichier envoyé: {total_sent}/{size} bytes")

# --- Upload dans un thread séparé (CORRIGÉ) ---
def upload_esp_thread(name):
    global upload_client
    
    append_log(name, "[INFO] Connexion au serveur pour upload...")
    
    try:
        # Créer une nouvelle socket dédiée à l'upload
        upload_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        upload_client.connect((SERVER_IP, SERVER_PORT))
        
        files = [
            BIN_PATHS[f"{name}_Bootloader"],
            BIN_PATHS[f"{name}_Partition"],
            BIN_PATHS[f"{name}_Image"]
        ]
        
        # Vérification que tous les fichiers existent
        for f in files:
            if not os.path.exists(f):
                append_log(name, f"[ERREUR] Fichier introuvable: {f}")
                return
        
        total_size = sum(os.path.getsize(f) for f in files)
        sent = 0

        append_log(name, "[INFO] Envoi de la commande UPLOAD...")
        upload_client.sendall(f"UPLOAD_{name}\n".encode())
        
        # Attendre la réponse READY
        append_log(name, "[INFO] Attente de READY...")
        ready = recv_until(upload_client, b'\n', timeout=10.0)
        print(f"[DEBUG] Réponse serveur: {ready}")
        
        if ready != "READY":
            append_log(name, f"[ERREUR] Serveur pas prêt (reçu: {ready})")
            return
        
        append_log(name, "[INFO] Serveur prêt, début de l'upload...")
        
        # Envoi des 3 fichiers
        file_names = ["Bootloader", "Partition Table", "Firmware"]
        for idx, (f, fname) in enumerate(zip(files, file_names)):
            file_size = os.path.getsize(f)
            append_log(name, f"[INFO] Envoi {fname} ({file_size} bytes)...")
            send_file(upload_client, f)
            sent += file_size
            progressbars[name]["value"] = (sent / total_size) * 100
            root.update_idletasks()
        
        append_log(name, "[INFO] Upload terminé! En attente du flash...")
        progressbars[name]["value"] = 0  # Reset avant le flash
        
        # Attendre les retours du flash (avec timeout long)
        upload_client.settimeout(120.0)  # 2 minutes max pour le flash
        while True:
            try:
                response = recv_until(upload_client, b'\n', timeout=120.0)
                if response.startswith("OK:"):
                    append_log(name, "[SUCCESS] Flash terminé avec succès!")
                    progressbars[name]["value"] = 100
                    break
                elif response.startswith("ERROR:"):
                    append_log(name, f"[ERREUR] {response}")
                    break
                elif response.startswith("FLASH_LOG:"):
                    # On ignore le préfixe et le target
                    continue
            except Exception as e:
                append_log(name, f"[ERREUR] Timeout ou erreur: {e}")
                break
        
    except Exception as e:
        append_log(name, f"[ERREUR] {e}")
        import traceback
        traceback.print_exc()
        progressbars[name]["value"] = 0
    finally:
        if upload_client:
            upload_client.close()
            upload_client = None

def make_column(name, col):
    frame = tk.LabelFrame(root, text=name, padx=10, pady=10)
    frame.grid(row=0, column=col, padx=10, pady=10, sticky="nsew")

    txt = scrolledtext.ScrolledText(frame, width=60, height=20)
    txt.grid(row=1, column=0, columnspan=3, pady=(5, 10))
    for c in ANSI_COLORS.values():
        txt.tag_configure('fg_'+c, foreground=c)

    progress = ttk.Progressbar(frame, orient="horizontal", mode="determinate", length=300)
    progress.grid(row=2, column=0, columnspan=3, pady=5)

    tk.Button(frame, text="Upload", command=lambda: threading.Thread(target=upload_esp_thread, args=(name,), daemon=True).start()).grid(row=0, column=0)
    tk.Button(frame, text="Reset", command=lambda: reset_esp(name)).grid(row=0, column=1)
    tk.Button(frame, text="Clear", command=lambda: clear_log(name)).grid(row=0, column=2)

    frames[name] = frame
    textboxes[name] = txt
    progressbars[name] = progress

make_column("MainPCB", 0)
make_column("SlavePCB", 1)

# --- Réception des logs ---
def listen_server():
    global connected
    while True:
        if not connected:
            try:
                connect_server()
            except:
                time.sleep(2)
                continue
        try:
            data = client.recv(4096)
            if not data:
                connected = False
                time.sleep(2)
                continue
            
            # Traiter plusieurs lignes dans un seul paquet
            lines = data.decode(errors="ignore").strip().split('\n')
            for line in lines:
                if not line:
                    continue
                    
                if line.startswith("LOG:"):
                    _, target, msg = line.split(":", 2)
                    append_log(target, msg)
                    
                elif line.startswith("OK:"):
                    _, action = line.split(":", 1)
                    root.title(f"✅ Action terminée : {action}")
                    
                elif line.startswith("FLASHING:"):
                    _, target = line.split(":")
                    append_log(target, "[INFO] Flash en cours...")
                    progressbars[target]["value"] = 0
                    
                elif line.startswith("FLASH_LOG:"):
                    # Format: FLASH_LOG:target:message
                    parts = line.split(":", 2)
                    if len(parts) >= 3:
                        target = parts[1]
                        flash_msg = parts[2]
                        
                        # Parser la progression d'esptool
                        # Format: "Writing at 0x... [ ====> ] 91.6% 344064/375575 bytes..."
                        if "%" in flash_msg and ("Writing at" in flash_msg or "]" in flash_msg):
                            # Chercher un pourcentage
                            import re
                            match = re.search(r'(\d+(?:\.\d+)?)\s*%', flash_msg)
                            if match:
                                percent = float(match.group(1))
                                progressbars[target]["value"] = percent
                                
                                # Forcer la mise à jour immédiate de l'interface
                                root.update()
                                
                                # Afficher seulement tous les 20% pour pas spam les logs
                                if int(percent) % 20 == 0 or int(percent) == 100:
                                    append_log(target, f"Flash: {int(percent)}%")
                        else:
                            # Pour les autres messages importants
                            if any(keyword in flash_msg for keyword in ["Configuring", "erased", "Compressed", "Wrote", "Hash", "resetting"]):
                                append_log(target, flash_msg.strip())
                        
        except Exception as e:
            print(f"[ERREUR LISTEN] {e}")
            connected = False
            time.sleep(2)

# --- Connexion automatique ---
def connect_server():
    global client, connected
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect((SERVER_IP, SERVER_PORT))
    connected = True
    root.title("Connecté au serveur")
    print("[CLIENT] Connecté au serveur")

threading.Thread(target=listen_server, daemon=True).start()

root.mainloop()