import socket
import threading
import tkinter as tk
from tkinter import scrolledtext, ttk
import os
import time
import re
from PIL import Image, ImageTk  # Ajout pour affichage image
import io
import websocket

# --- Configuration ---
SERVER_IP = "192.168.1.16"  # IP du PC portable
SERVER_PORT = 5000

BIN_PATHS = {
    "MainPCB_Image": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\MainPCB\VanManagement\build\VanManagement.bin",
    "MainPCB_Bootloader": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\MainPCB\VanManagement\build\bootloader\bootloader.bin",
    "MainPCB_Partition": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\MainPCB\VanManagement\build\partition_table\partition-table.bin",
    "SlavePCB_Image": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\SlavePCB\WaterManagment\build\WaterManagment.bin",
    "SlavePCB_Bootloader": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\SlavePCB\WaterManagment\build\bootloader\bootloader.bin",
    "SlavePCB_Partition": r"C:\Users\Ewen\Documents\Projets\VAN\code\Van\SlavePCB\WaterManagment\build\partition_table\partition-table.bin"
}

client = None
connected = False
upload_client = None  # Socket dédié pour l'upload

# --- ANSI colors ---
ANSI_COLORS = {
    '30': 'black',
    '31': 'red',
    '32': '#00FF00',  # vert flashy
    '33': 'yellow',
    '34': 'blue',
    '35': 'magenta',
    '36': 'cyan',
    '37': 'white'
}

# --- GUI setup ---
root = tk.Tk()
root.title("ESP32 Remote Manager")

frames = {}
textboxes = {}
progressbars = {}

# --- Vue caméra permanente ---
cam_frame = tk.Frame(root, bg="#888")
cam_frame.grid(row=0, column=0, columnspan=2, pady=(10, 0), sticky="ew")
cam_label = tk.Label(cam_frame, bg="#888")
cam_label.pack(padx=10, pady=10)

# Placeholder "No image"
def show_placeholder():
    img = Image.new("RGB", (400, 300), "#888")
    from PIL import ImageDraw, ImageFont
    draw = ImageDraw.Draw(img)
    font = ImageFont.load_default()
    text = "No image"
    bbox = draw.textbbox((0, 0), text, font=font)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text(((400-w)//2, (300-h)//2), text, fill="white", font=font)
    photo = ImageTk.PhotoImage(img)
    cam_label.config(image=photo)
    cam_label.image = photo

show_placeholder()
root.has_image = False

def video_receiver():
    def on_data(ws, message, opcode, fin):
        # opcode 2 = binary frame
        if opcode == websocket.ABNF.OPCODE_BINARY:
            try:
                img = Image.open(io.BytesIO(message))
                img = img.resize((400, 300))
                photo = ImageTk.PhotoImage(img)
                cam_label.config(image=photo)
                cam_label.image = photo
                root.has_image = True
            except Exception:
                show_placeholder()
                root.has_image = False

    def on_error(ws, error):
        show_placeholder()
        root.has_image = False

    def on_close(ws, close_status_code, close_msg):
        show_placeholder()
        root.has_image = False

    ws = websocket.WebSocketApp(
        "ws://192.168.1.16:8765",
        on_data=on_data,
        on_error=on_error,
        on_close=on_close
    )
    ws.run_forever()

# Démarrer le client WebSocket dans un thread séparé
threading.Thread(target=video_receiver, daemon=True).start()

# --- Fonction pour gérer les couleurs ANSI ---
def insert_ansi_text(txt_widget, text):
    pattern = re.compile(r'\x1b\[(\d+;)?(\d+)m')
    last_end = 0
    current_color = 'white'  # couleur par défaut: blanc
    for m in pattern.finditer(text):
        txt_widget.insert(tk.END, text[last_end:m.start()], ('fg_'+current_color,))
        color_code = m.group(2)
        current_color = ANSI_COLORS.get(color_code, 'white')
        last_end = m.end()
    txt_widget.insert(tk.END, text[last_end:], ('fg_'+current_color,))
    txt_widget.see(tk.END)

def append_log(name, msg):
	# Si le terminal est gelé, stocker dans le tampon au lieu d'insérer
	if freeze_flags.get(name, False):
		buf = freeze_buffers.setdefault(name, [])
		buf.append(msg + "\n")
		# Mettre à jour le texte du bouton pour afficher la taille du tampon
		btn = freeze_buttons.get(name)
		if btn:
			btn.config(text=f"Unfreeze ({len(buf)})", bg="#d9534f")
		return
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

def send_command(cmd):
    if connected:
        client.sendall(f"{cmd}\n".encode())
    else:
        append_log("MainPCB", "[ERREUR] Pas connecté au serveur")

# --- Ajout : mécanisme hold pour envoi continu tant que la souris est maintenue ---
hold_flags = {}  # clé = commande, valeur = bool

def _send_repeat(cmd, interval=0.001):
    # envoie immédiatement puis en boucle tant que le flag est True
    try:
        send_command(cmd)
    except Exception:
        pass
    while hold_flags.get(cmd):
        try:
            time.sleep(interval)
            if not hold_flags.get(cmd):
                break
            send_command(cmd)
        except Exception:
            pass

def start_hold(cmd, interval=0.1):
    if hold_flags.get(cmd):
        return
    hold_flags[cmd] = True
    t = threading.Thread(target=_send_repeat, args=(cmd, interval), daemon=True)
    t.start()

def stop_hold(cmd):
    hold_flags[cmd] = False

# --- Ajout global : gestion freeze par terminal ---
freeze_flags = {}     # name -> bool
freeze_buffers = {}   # name -> list[str]
freeze_buttons = {}   # name -> tk.Button (pour mettre à jour l'affichage)

def toggle_freeze(name):
	# bascule l'état et, si on dégel, vide le buffer dans le terminal
	if freeze_flags.get(name):
		# dégel : vider le tampon
		freeze_flags[name] = False
		buf = freeze_buffers.get(name, [])
		if buf:
			txt = textboxes.get(name)
			if txt:
				for m in buf:
					insert_ansi_text(txt, m)
			freeze_buffers[name] = []
		btn = freeze_buttons.get(name)
		if btn:
			btn.config(text="Freeze", bg=None)
	else:
		# gel
		freeze_flags[name] = True
		btn = freeze_buttons.get(name)
		if btn:
			btn.config(text=f"Unfreeze (0)", bg="#d9534f")

def make_column(name, col):
	frame = tk.LabelFrame(root, text=name, padx=10, pady=10)
	frame.grid(row=1, column=col, padx=10, pady=10, sticky="nsew")

	txt = scrolledtext.ScrolledText(frame, width=100, height=20, bg="black")
	txt.grid(row=0, column=0, columnspan=1, pady=(5, 10))
	for c in ANSI_COLORS.values():
		txt.tag_configure('fg_'+c, foreground=c)
	txt.tag_configure('fg_white', foreground='white')

	progress = ttk.Progressbar(frame, orient="horizontal", mode="determinate", length=300)
	progress.grid(row=1, column=0, columnspan=1, pady=5)

	tk.Button(frame, text="Upload", command=lambda: threading.Thread(target=upload_esp_thread, args=(name,), daemon=True).start()).grid(row=2, column=0, sticky="ew")
	tk.Button(frame, text="Reset", command=lambda: reset_esp(name)).grid(row=3, column=0, sticky="ew")
	tk.Button(frame, text="Clear", command=lambda: clear_log(name)).grid(row=4, column=0, sticky="ew")

	# Bouton Freeze / Unfreeze
	fz_btn = tk.Button(frame, text="Freeze", command=lambda n=name: toggle_freeze(n))
	fz_btn.grid(row=6, column=0, sticky="ew", pady=(4,0))
	# stocker le bouton pour mise à jour du label (taille du buffer)
	freeze_buttons[name] = fz_btn
	# initialiser flags/tampons
	freeze_flags[name] = False
	freeze_buffers[name] = []

	# --- Zone boutons ronds ---
	btn_zone = tk.Frame(frame)
	btn_zone.grid(row=5, column=0, pady=15)

	if name == "MainPCB":
		# Bouton rond SW (maintien possible)
		sw_btn = tk.Canvas(btn_zone, width=60, height=60, highlightthickness=0)
		sw_btn.grid(row=0, column=0, padx=10)
		oval = sw_btn.create_oval(5, 5, 55, 55, fill="#e0e0e0", outline="#888", width=2)
		sw_btn.create_text(30, 30, text="SW", font=("Arial", 14, "bold"))
		# Bind press/release pour envoi continu tant que la souris est maintenue
		sw_cmd = "SW_CLICK"
		sw_btn.bind("<ButtonPress-1>", lambda e, c=sw_cmd: start_hold(c))
		sw_btn.bind("<ButtonRelease-1>", lambda e, c=sw_cmd: stop_hold(c))
		sw_btn.bind("<Leave>", lambda e, c=sw_cmd: stop_hold(c))

	if name == "SlavePCB":
		btn_names = ["BE1", "BE2", "BD1", "BD2", "BH", "BV1", "BV2"]
		for i, btn in enumerate(btn_names):
			canvas = tk.Canvas(btn_zone, width=60, height=60, highlightthickness=0)
			canvas.grid(row=0, column=i, padx=10)
			oval = canvas.create_oval(5, 5, 55, 55, fill="#e0e0e0", outline="#888", width=2)
			canvas.create_text(30, 30, text=btn, font=("Arial", 14, "bold"))
			cmd_name = f"{btn}_CLICK"
			# press/release pour envoi continu
			canvas.bind("<ButtonPress-1>", lambda e, c=cmd_name: start_hold(c))
			canvas.bind("<ButtonRelease-1>", lambda e, c=cmd_name: stop_hold(c))
			canvas.bind("<Leave>", lambda e, c=cmd_name: stop_hold(c))

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
                show_placeholder()
                time.sleep(2)
                continue

            # Gestion réception image caméra
            if data.startswith(b"CAM_IMG:"):
                img_bytes = data[len("CAM_IMG:"):]

                if hasattr(root, "camera_update"):
                    root.camera_update(img_bytes)
                continue

            # Si aucune image reçue, affiche le placeholder
            if not root.has_image:
                show_placeholder()

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
    # request_camera()  # SUPPRIMER : le flux vidéo est géré par WebSocket

threading.Thread(target=listen_server, daemon=True).start()

root.mainloop()