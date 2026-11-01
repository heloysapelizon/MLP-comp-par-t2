#!/bin/bash
# T1: versao sequencial, entrada de referencia, 10 repeticoes em 1 no.
#SBATCH --job-name=t2-seq
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --output=resultados/sequencial_%j.out
cd "$SLURM_SUBMIT_DIR"
source scripts/comum.sh
registra_ambiente
lscpu
for r in $(seq 1 "$REPS"); do
    echo "RUN,sequencial,1,1,$r"
    ./sequencial "$TAREFAS" "$AMOSTRAS" "$EPOCAS"
done
