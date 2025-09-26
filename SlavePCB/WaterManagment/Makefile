# Makefile pour ESP32 WaterManagement avec OTA
# Usage: make build, make flash, make ota IP=192.168.1.100

# Configuration par défaut
PORT ?= /dev/ttyUSB0
BAUD ?= 921600
IP ?= 192.168.1.100

# Couleurs pour l'affichage
GREEN = \033[0;32m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m # No Color

.PHONY: help build flash monitor clean ota config

help:
	@echo "$(GREEN)ESP32 WaterManagement - Commandes disponibles:$(NC)"
	@echo "  $(YELLOW)make build$(NC)        - Compiler le projet"
	@echo "  $(YELLOW)make flash$(NC)        - Flasher via USB (PORT=$(PORT))"
	@echo "  $(YELLOW)make monitor$(NC)      - Monitorer via USB"
	@echo "  $(YELLOW)make clean$(NC)        - Nettoyer le build"
	@echo "  $(YELLOW)make config$(NC)       - Ouvrir menuconfig"
	@echo "  $(YELLOW)make ota IP=x.x.x.x$(NC) - Upload OTA sans fil"
	@echo ""
	@echo "$(GREEN)Exemples:$(NC)"
	@echo "  make ota IP=192.168.1.100"
	@echo "  make flash PORT=/dev/ttyUSB1"

build:
	@echo "$(GREEN)🔨 Compilation du projet...$(NC)"
	idf.py build

flash: build
	@echo "$(GREEN)⚡ Flash via USB sur $(PORT)...$(NC)"
	idf.py -p $(PORT) -b $(BAUD) flash

monitor:
	@echo "$(GREEN)📺 Monitoring sur $(PORT)...$(NC)"
	idf.py -p $(PORT) monitor

flash-monitor: build
	@echo "$(GREEN)⚡📺 Flash + Monitor sur $(PORT)...$(NC)"
	idf.py -p $(PORT) -b $(BAUD) flash monitor

clean:
	@echo "$(GREEN)🧹 Nettoyage...$(NC)"
	idf.py clean

config:
	@echo "$(GREEN)⚙️  Configuration menuconfig...$(NC)"
	idf.py menuconfig

ota: build
	@echo "$(GREEN)📡 Upload OTA vers $(IP)...$(NC)"
	@if [ -f "build/WaterManagment.bin" ]; then \
		python3 ota_upload.py $(IP) build/WaterManagment.bin; \
	else \
		echo "$(RED)❌ Fichier build/WaterManagment.bin introuvable$(NC)"; \
		echo "$(YELLOW)Lancez d'abord 'make build'$(NC)"; \
		exit 1; \
	fi

# Commandes pour le développement
size: build
	@echo "$(GREEN)📊 Taille du firmware:$(NC)"
	idf.py size

erase:
	@echo "$(YELLOW)⚠️  Effacement complet de la flash...$(NC)"
	idf.py -p $(PORT) erase_flash

# Affichage des informations système
info:
	@echo "$(GREEN)ℹ️  Informations système:$(NC)"
	@echo "  ESP-IDF: $$(idf.py --version)"
	@echo "  Python: $$(python3 --version)"
	@echo "  Port: $(PORT)"
	@echo "  IP OTA: $(IP)"

# Installation des dépendances Python pour OTA
install-deps:
	@echo "$(GREEN)📦 Installation des dépendances Python...$(NC)"
	pip3 install requests

# Test de connectivité OTA
ping-esp:
	@echo "$(GREEN)🏓 Test de connectivité vers $(IP)...$(NC)"
	@ping -c 3 $(IP) || (echo "$(RED)❌ ESP32 non accessible$(NC)" && exit 1)
	@echo "$(GREEN)✅ ESP32 accessible$(NC)"
