#!/bin/bash
# Equivalencia da fraca: uma execucao da sequencial com o maior tamanho da
# fraca (TAREFAS * 64) imprime o checksum de cada prefixo TAREFAS * 2^k, que e
# a referencia de cada tamanho da fraca. Nao e medicao de tempo (cerca de 64 x T1).
#SBATCH --job-name=t2-ckfraca
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --time=02:00:00
#SBATCH --output=resultados/checksum_fraca_%j.out
cd "$SLURM_SUBMIT_DIR"
source scripts/comum.sh
registra_ambiente
echo "RUN,checksum_fraca,1,1,1"
./sequencial $(( TAREFAS * 64 )) "$AMOSTRAS" "$EPOCAS" "$TAREFAS"
