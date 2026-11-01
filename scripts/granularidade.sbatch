#!/bin/bash
# Granularidade e balanceamento com P = 32 (16 por no):
#  (a) ida e volta com as mensagens do mestre/escravo, dentro do no e entre nos
#      (o tempo de uma tarefa sai da sequencial e das linhas ESCRAVO);
#  (b) varredura de g de 1 a 512: a escolhida (G em comum.sh) contra as bem
#      menores (sobrecarga de mensagens) e as bem maiores (fim do saco);
#  (c) tarefas e tempo ocupado por escravo nas linhas ESCRAVO.
#SBATCH --job-name=t2-gran
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=32
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --output=resultados/granularidade_%j.out
cd "$SLURM_SUBMIT_DIR"
source scripts/comum.sh
G_LISTA=${G_LISTA:-"1 2 4 8 16 32 64 128 256 512"}
registra_ambiente
for r in $(seq 1 "$REPS"); do
    echo "== pingpong dentro do no ($r)"
    lancar 2 1 ./pingpong $G_LISTA
    echo "== pingpong entre nos ($r)"
    lancar 2 2 ./pingpong $G_LISTA
done
for r in $(seq 1 "$REPS"); do
    for g in $G_LISTA; do
        echo "RUN,granularidade,32,16,$r"
        lancar 32 2 ./mestre_escravo "$g" "$TAREFAS" "$AMOSTRAS" "$EPOCAS"
    done
done
