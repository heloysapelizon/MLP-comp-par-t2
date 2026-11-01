#!/bin/bash
# Escalabilidade forte: entrada de referencia fixa, P = 2..64, 10 repeticoes.
# Extra: P = nucleos de um no (8 na Atlantica) num so no, para comparar com o
# mesmo P dividido em 2 nos (comunicacao dentro do no x entre nos).
#SBATCH --job-name=t2-forte
#SBATCH --nodes=2
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --output=resultados/forte_%j.out
cd "$SLURM_SUBMIT_DIR"
source scripts/comum.sh
registra_ambiente
for r in $(seq 1 "$REPS"); do
    for P in $(na_ordem "$r" ${P_LISTA:-2 4 8 16 32 64}); do
        echo "RUN,forte,$P,$(( P / 2 )),$r"
        lancar "$P" 2 ./mestre_escravo "$G" "$TAREFAS" "$AMOSTRAS" "$EPOCAS"
    done
    echo "RUN,forte_1no,$NUCLEOS_POR_NO,$NUCLEOS_POR_NO,$r"
    lancar "$NUCLEOS_POR_NO" 1 ./mestre_escravo "$G" "$TAREFAS" "$AMOSTRAS" "$EPOCAS"
done
