#!/bin/bash
# Escalabilidade forte: entrada de referencia fixa, P = 2..64, 10 repeticoes.
# Extra: P = 16 num so no, para comparar com P = 16 dividido em 2 nos
# (comunicacao dentro do no x entre nos).
#SBATCH --job-name=t2-forte
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=32
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --output=resultados/forte_%j.out
cd "$SLURM_SUBMIT_DIR"
source scripts/comum.sh
registra_ambiente
for r in $(seq 1 "$REPS"); do
    for P in ${P_LISTA:-2 4 8 16 32 64}; do
        echo "RUN,forte,$P,$(( P / 2 )),$r"
        lancar "$P" 2 ./mestre_escravo "$G" "$TAREFAS" "$AMOSTRAS" "$EPOCAS"
    done
    echo "RUN,forte_1no,16,16,$r"
    lancar 16 1 ./mestre_escravo "$G" "$TAREFAS" "$AMOSTRAS" "$EPOCAS"
done
