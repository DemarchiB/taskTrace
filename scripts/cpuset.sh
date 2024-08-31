#!/bin/bash

# Verifica se o script está sendo executado como root
if [ "$(id -u)" != "0" ]; then
    echo "Este script precisa ser executado com privilégios de superusuário."
    echo "Por favor, execute-o usando 'sudo'."
    exit 1
fi

echo "Cria o cpuset para tarefas do tipo deadline"
if ! mountpoint -q "/sys/fs/cgroup/cpuset"; then
    echo "O cpuset não está montado. Montando..."
    mkdir /sys/fs/cgroup/cpuset
    mount -t cpuset cpuset /sys/fs/cgroup/cpuset
else
    echo "O cpuset já está montado."
fi

cd /sys/fs/cgroup/cpuset/

if [ ! -d "deadline" ]; then
    echo "Criando cpuset 'deadline'"
    mkdir deadline
else
    echo "cpuset 'deadline' já existe"
fi

echo "Atualizando propriedades do cpuset deadline"
echo 3-5 > deadline/cpus
echo 0 > deadline/mems
echo 1 > cpu_exclusive
echo 0 > sched_load_balance


file="deadline/cpu_exclusive"
current_value=$(cat "$file")
if [ "$current_value" != "1" ]; then
    echo 1 > "$file"
fi

# file="deadline/mem_exclusive"
# current_value=$(cat "$file")
# if [ "$current_value" != "1" ]; then
#     echo 1 > "$file"
# fi

echo "Aloca todas as tarefas deadline no cpuset"
process_name="userTest"

# Obtém o PID do processo com o nome especificado
pids=$(pgrep -x $process_name)

if [ -z "$pids" ]; then
    echo "Nenhum processo encontrado com o nome $process_name."
    exit 1
fi

echo "Processos e threads com SCHED_DEADLINE para $process_name:"

# Itera sobre cada PID encontrado
for pid in $pids; do
    echo "Processo PID: $pid"
    # Obtém todas as threads (TIDs) do processo
    tids=$(ls /proc/$pid/task)
    for tid in $tids; do
        # Verifica a política de agendamento da thread
        policy=$(chrt -p $tid | grep 'SCHED_DEADLINE')
        if [ ! -z "$policy" ]; then
            echo "    Thread TID: $tid está usando SCHED_DEADLINE"
            echo $tid > deadline/tasks
            #echo $tid | sudo tee /sys/fs/cgroup/cpuset/deadline/tasks
        fi
    done
done
