#!/usr/bin/env python3
import subprocess
import sys
import re
import csv
import matplotlib.pyplot as plt

if len(sys.argv) != 3:
    print(f"Uso: {sys.argv[0]} <arquivo.dat> <PID>")
    sys.exit(1)

dat_file = sys.argv[1]
TARGET_PID = int(sys.argv[2])

print(f"[*] Executando trace-cmd report em {dat_file} ...")
txt = subprocess.check_output(["trace-cmd", "report", dat_file], text=True)

# Regex para timestamp e sched_switch
ts_re = re.compile(r"\s(\d+\.\d+):\s+sched_switch:")
pid_pattern = f":{TARGET_PID} "

# Armazenamento
runtimes_us = []
periods_us = []
start_times = []   # timestamps onde o PID aparece como next

running_since = None

print("[*] Processando eventos sched_switch...")

for line in txt.splitlines():

    if "sched_switch:" not in line:
        continue

    # extrair timestamp
    mts = ts_re.search(line)
    if not mts:
        continue
    ts = float(mts.group(1))

    # Pegar texto após "sched_switch:"
    details = line.split("sched_switch:")[1] if "sched_switch:" in line else ""
    if "==>" not in details:
        continue

    prev_part, next_part = details.split("==>")

    is_prev = pid_pattern in prev_part
    is_next = pid_pattern in next_part

    # ----------------------------------------------
    # 1) Começo de execução da tarefa
    # ----------------------------------------------
    if is_next:
        # guardar timestamp do início
        start_times.append(ts)
        running_since = ts

    # ----------------------------------------------
    # 2) Fim de execução da tarefa
    # ----------------------------------------------
    if is_prev and running_since is not None:
        runtime = (ts - running_since) * 1_000_000.0
        runtimes_us.append(runtime)
        running_since = None

# ========================================
# Calcular períodos
# ========================================
for i in range(1, len(start_times)):
    dt_us = (start_times[i] - start_times[i-1]) * 1_000_000.0
    periods_us.append(dt_us)

# ========================================
# Exibir resultados
# ========================================
print("\n========== RESULTADOS ==========")

if runtimes_us:
    print(f"Execuções:             {len(runtimes_us)}")
    print(f"Runtime médio (us):    {sum(runtimes_us)/len(runtimes_us):.2f}")
    print(f"Runtime mínimo (us):   {min(runtimes_us):.2f}")
    print(f"Runtime máximo (us):   {max(runtimes_us):.2f}")
else:
    print("[AVISO] Nenhum runtime encontrado!")

if periods_us:
    print("\nAtivações:             ", len(periods_us) + 1)
    print(f"Período médio (us):    {sum(periods_us)/len(periods_us):.2f}")
    print(f"Período mínimo (us):   {min(periods_us):.2f}")
    print(f"Período máximo (us):   {max(periods_us):.2f}")
else:
    print("\n[AVISO] Não foi possível calcular períodos.")

print("================================\n")

# ========================================
# Salvar CSV
# ========================================
with open("runtime_period_results.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["runtime_us", "period_us"])
    max_len = max(len(runtimes_us), len(periods_us))
    for i in range(max_len):
        rt = runtimes_us[i] if i < len(runtimes_us) else ""
        pt = periods_us[i] if i < len(periods_us) else ""
        w.writerow([rt, pt])

print("[*] CSV salvo como runtime_period_results.csv")

# ========================================
# Histograma do runtime
# ========================================
if runtimes_us:
    plt.figure(figsize=(10,5))
    plt.hist(runtimes_us, bins=50)
    plt.title(f"Histograma do Runtime (PID={TARGET_PID})")
    plt.xlabel("Runtime (µs)")
    plt.ylabel("Frequência")
    plt.grid()
    plt.tight_layout()
    plt.savefig("runtime_hist.png")
    print("[*] Histograma salvo em runtime_hist.png")
    plt.show()

# ========================================
# Histograma do período
# ========================================
if periods_us:
    plt.figure(figsize=(10,5))
    plt.hist(periods_us, bins=50)
    plt.title(f"Histograma do Período Real (PID={TARGET_PID})")
    plt.xlabel("Período (µs)")
    plt.ylabel("Frequência")
    plt.grid()
    plt.tight_layout()
    plt.savefig("period_hist.png")
    print("[*] Histograma salvo em period_hist.png")
    plt.show()
