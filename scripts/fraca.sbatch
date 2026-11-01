#!/bin/bash
# Escalabilidade fraca: P vezes mais tarefas, cada uma com o mesmo custo
# (mesmas amostras e epocas). A linha "amostras treinadas" de cada execucao
# comprova que o trabalho cresceu na proporcao de P.
#SBATCH --job-name=t2-fraca
#SBATCH --nodes=2
#SBATCH --exclusive
#SBATCH --time=04:00:00
#SBATCH --output=resultados/fraca_%j.out
cd "$SLURM_SUBMIT_DIR"
source scripts/comum.sh
registra_ambiente
for r in $(seq 1 "$REPS"); do
    for P in ${P_LISTA:-2 4 8 16 32 64}; do
        echo "RUN,fraca,$P,$(( P / 2 )),$r"
        lancar "$P" 2 ./mestre_escravo "$G" $(( TAREFAS * P )) "$AMOSTRAS" "$EPOCAS"
    done
done
