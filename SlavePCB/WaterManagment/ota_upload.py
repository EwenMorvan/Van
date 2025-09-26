#!/usr/bin/env python3
"""
Script pour upload OTA sans fil vers ESP32
Usage: python3 ota_upload.py <esp32_ip> [firmware.bin]
"""

import sys
import requests
import os
from pathlib import Path

def upload_ota(esp32_ip, firmware_path):
    """Upload firmware via OTA to ESP32"""
    
    if not os.path.exists(firmware_path):
        print(f"Erreur: Le fichier {firmware_path} n'existe pas")
        return False
    
    url = f"http://{esp32_ip}:8070/upload"
    
    print(f"Upload de {firmware_path} vers {esp32_ip}...")
    print(f"URL: {url}")
    
    try:
        with open(firmware_path, 'rb') as f:
            firmware_data = f.read()
            
        # Envoyer le fichier binaire directement (pas en multipart)
        headers = {
            'Content-Type': 'application/octet-stream',
            'Content-Length': str(len(firmware_data))
        }
        
        response = requests.post(url, data=firmware_data, headers=headers, timeout=120)
        
        if response.status_code == 200:
            print("✅ Upload OTA réussi!")
            print("L'ESP32 va redémarrer avec le nouveau firmware.")
            return True
        else:
            print(f"❌ Erreur HTTP: {response.status_code}")
            print(f"Réponse: {response.text}")
            return False
            
    except requests.exceptions.Timeout:
        print("❌ Timeout - L'upload a pris trop de temps")
        return False
    except requests.exceptions.ConnectionError:
        print(f"❌ Impossible de se connecter à {esp32_ip}")
        print("Vérifiez que l'ESP32 est connecté au WiFi et accessible")
        return False
    except Exception as e:
        print(f"❌ Erreur: {e}")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 ota_upload.py <esp32_ip> [firmware.bin]")
        print("Exemple: python3 ota_upload.py 192.168.1.100")
        print("         python3 ota_upload.py 192.168.1.100 build/WaterManagment.bin")
        sys.exit(1)
    
    esp32_ip = sys.argv[1]
    
    # Déterminer le chemin du firmware
    if len(sys.argv) >= 3:
        firmware_path = sys.argv[2]
    else:
        # Chemin par défaut
        firmware_path = "build/WaterManagment.bin"
    
    # Vérifier que le fichier existe
    if not os.path.exists(firmware_path):
        print(f"Fichier {firmware_path} introuvable.")
        
        # Essayer de trouver automatiquement
        possible_paths = [
            "build/WaterManagment.bin",
            "WaterManagment.bin",
            "../build/WaterManagment.bin"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                firmware_path = path
                print(f"Firmware trouvé: {firmware_path}")
                break
        else:
            print("❌ Aucun firmware trouvé. Compilez d'abord avec 'idf.py build'")
            sys.exit(1)
    
    # Afficher les informations du fichier
    file_size = os.path.getsize(firmware_path)
    print(f"Fichier: {firmware_path}")
    print(f"Taille: {file_size} bytes ({file_size/1024:.1f} KB)")
    
    # Faire l'upload
    success = upload_ota(esp32_ip, firmware_path)
    
    if success:
        print("\n🎉 Mise à jour OTA terminée avec succès!")
        print("L'ESP32 devrait redémarrer dans quelques secondes.")
    else:
        print("\n💥 Échec de la mise à jour OTA")
        sys.exit(1)

if __name__ == "__main__":
    main()
