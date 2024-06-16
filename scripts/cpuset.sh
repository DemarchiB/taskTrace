#!/bin/bash

# Verifica se o script está sendo executado como root
if [ "$(id -u)" != "0" ]; then
    echo "Este script precisa ser executado com privilégios de superusuário."
    echo "Por favor, execute-o usando 'sudo'."
    exit 1
fi

echo "Cria o cpuset para tarefas do tipo deadline"
mkdir /sys/fs/cgroup/cpuset/deadline
sudo ksh -c "echo 0 > /sys/fs/cgroup/cpuset/deadline/cpuset.cpus"
sudo ksh -c "echo 0 > /sys/fs/cgroup/cpuset/deadline/cpuset.mems"

echo "Aloca todas as tarefas deadline no cpuset"
while true; do
    for pid in $(pgrep -x <nome_do_processo>); do
        policy=$(chrt -p $pid | grep 'SCHED_DEADLINE')
        if [ ! -z "$policy" ]; then
            echo $pid | sudo tee /sys/fs/cgroup/cpuset/deadline/tasks
        fi
    done
    sleep 5
done

