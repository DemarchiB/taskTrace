#!/bin/bash

# Verifica se o script está sendo executado como root
if [ "$(id -u)" != "0" ]; then
    echo "Este script precisa ser executado com privilégios de superusuário."
    echo "Por favor, execute-o usando 'sudo'."
    exit 1
fi

echo "Run supervisor: Executando processo do supervisor"
./Supervisor/Supervisor