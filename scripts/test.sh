#!/bin/bash

# Verifica se o script está sendo executado como root
if [ "$(id -u)" != "0" ]; then
    echo "Este script precisa ser executado com privilégios de superusuário."
    echo "Por favor, execute-o usando 'sudo'."
    exit 1
fi

echo "Test script: Executando processo do supervisor"
./Supervisor/Supervisor --sys_latency 500 &

echo "Test script: Esperando 1s"
sleep 1

echo "Test script: Executando processo de simulação de tarefas do usuário"
./userTest/userTest &

echo "Test script: Pressione qualquer tecla para encerrar os testes"
read -n 1 -s -r -p ""
echo

killall Supervisor &
killall userTest &