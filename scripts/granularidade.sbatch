#!/bin/bash
# Granularidade e balanceamento com P = 32 (16 por no), como pede o enunciado:
#  (a) ida e volta com as mensagens do mestre/escravo, dentro do no e entre nos
#      (o tempo de uma tarefa sai da sequencial e das linhas ESCRAVO);
#  (b) varredura de g de 1 a 2048: g pequeno paga mais mensagens, g grande deixa
#      escravos ociosos no fim do saco; a de menor mediana vira G em comum.sh;
#  (c) tarefas e tempo ocupado por escravo nas linhas ESCRAVO.
#SBATCH --job-name=t2-gran
#SBATCH --nodes=2
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --output=resultados/granularidade_%j.out
cd "$SLURM_SUBMIT_DIR"
source scripts/comum.sh
G_LISTA=${G_LISTA:-"1 2 4 8 16 32 64 128 256 512 1024 2048"}
registra_ambiente
for r in $(seq 1 "$REPS"); do
    echo "== pingpong dentro do no ($r)"
    lancar 2 1 ./pingpong $G_LISTA
    echo "== pingpong entre nos ($r)"
    lancar 2 2 ./pingpong $G_LISTA
done
for r in $(seq 1 "$REPS"); do
    for g in $(na_ordem "$r" $G_LISTA); do
        echo "RUN,granularidade,32,16,$r"
        lancar 32 2 ./mestre_escravo "$g" "$TAREFAS" "$AMOSTRAS" "$EPOCAS"
    done
done
