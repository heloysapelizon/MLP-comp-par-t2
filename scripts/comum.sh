# Parametros e funcoes compartilhados pelos scripts sbatch (source scripts/comum.sh).

# ---- Entrada de referencia (ajuste ate a sequencial ficar entre 30 e 60 s) ----
TAREFAS=${TAREFAS:-6000}  # tarefas no saco (cada uma leva alguns ms)
AMOSTRAS=${AMOSTRAS:-200} # imagens de treino por tarefa
EPOCAS=${EPOCAS:-2}       # epocas por tarefa
# Granularidade escolhida (tarefas por mensagem). E um compromisso entre dois
# custos que granularidade.sbatch mede com P = 32:
#  - g pequeno: cada tarefa paga uma ida e volta e um atendimento do mestre,
#    que atende 31 escravos em serie;
#  - g grande: menos blocos que escravos * algumas rodadas, e no fim do saco
#    escravos ficam ociosos esperando o ultimo bloco (ate g * tempo da tarefa).
# Fixe aqui o g de menor mediana em granularidade.sbatch antes de rodar a
# forte e a fraca.
G=${G:-4}
REPS=${REPS:-10}

# Registra o ambiente no inicio de cada saida do Slurm.
registra_ambiente() {
    echo "== job $SLURM_JOB_ID em $(date -Is)"
    echo "== nos: $(scontrol show hostnames "$SLURM_JOB_NODELIST" | tr '\n' ' ')"
    echo "== git: $(git rev-parse --short HEAD 2>/dev/null)"
    echo "== entrada: TAREFAS=$TAREFAS AMOSTRAS=$AMOSTRAS EPOCAS=$EPOCAS G=$G REPS=$REPS"
    srun --version
    ${MPICXX:-mpicxx} --version | head -1
    lscpu | grep -E '^(Model name|Socket|Core|Thread|CPU\(s\))'
}

# lancar P NOS programa args...
# P processos divididos igualmente entre NOS nos, em bloco (ranks 0..P/NOS-1 no
# primeiro no, o mestre incluido), com srun como no Lab 3. Ate 16 por no, um
# processo por nucleo fisico; acima disso (P = 64) os processos ocupam as
# threads de HT. Sem --hint=nomultithread o Slurm pode colocar dois ranks nas
# duas threads do mesmo nucleo ja com 16 por no, e a curva cairia antes da hora.
# "verbose" escreve na saida a mascara de CPUs de cada rank: confira no primeiro
# job que, com P <= 32, cada rank ficou num nucleo fisico diferente.
# Se o Slurm da Atlantica contar so os 16 nucleos fisicos por no, o sbatch com
# --ntasks-per-node=32 sera recusado: nesse caso acrescente --overcommit aqui
# e no cabecalho dos scripts.
lancar() {
    local P=$1 NOS=$2
    shift 2
    local PPN=$(( (P + NOS - 1) / NOS ))
    local HINT=nomultithread
    [ "$PPN" -gt 16 ] && HINT=multithread
    srun -N "$NOS" -n "$P" --ntasks-per-node="$PPN" --distribution=block --hint="$HINT" --cpu-bind=verbose "$@"
}
