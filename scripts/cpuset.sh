#!/bin/bash

# Verifica se o script está sendo executado como root
if [ "$(id -u)" != "0" ]; then
    echo "Este script precisa ser executado com privilégios de superusuário."
    echo "Por favor, execute-o usando 'sudo'."
    exit 1
fi

echo "Cria o cpuset para tarefas do tipo deadline"
sudo mkdir cpuset
sudo mount -t cpuset cpuset /sys/fs/cgroup/cpuset
sudo mkdir /sys/fs/cgroup/cpuset/deadline
sudo echo 0 | sudo tee /sys/fs/cgroup/cpuset/deadline/cpus
sudo echo 0 | sudo tee /sys/fs/cgroup/cpuset/deadline/mems
# sudo ksh -c "echo 0 > /sys/fs/cgroup/cpuset/deadline/cpuset.cpus"
# sudo ksh -c "echo 0 > /sys/fs/cgroup/cpuset/deadline/cpuset.mems"

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
            echo $tid | sudo tee /sys/fs/cgroup/cpuset/deadline/tasks
        fi
    done
done
