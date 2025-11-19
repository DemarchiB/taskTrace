#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Uso: $0 <PID>"
    exit 1
fi

PID=$1
OUT="trace_${PID}.dat"

echo "[*] Gravando trace para o PID $PID (CTRL+C para parar)..."

# Remover arquivos antigos
rm -f ${OUT} ${OUT}.meta ${OUT}.cpu* 2>/dev/null

trace-cmd record \
    -o $OUT \
    -e sched:sched_switch \
    -e sched:sched_wakeup \
    -P $PID &

REC_PID=$!

# Importante: ENCERRAR COM SIGINT, NUNCA SIGTERM!
trap "echo; echo '[*] Finalizando captura...'; sudo kill -INT $REC_PID" INT
wait $REC_PID

echo "[*] Arquivos gerados:"
ls -lh ${OUT}*

echo
echo "[*] Verificando conteúdo..."
sudo trace-cmd report ${OUT} | head -20

echo
echo "[*] Abra no KernelShark com:"
echo "    kernelshark ${OUT}"
