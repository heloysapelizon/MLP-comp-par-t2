# Parametros e funcoes compartilhados pelos scripts sbatch (source scripts/comum.sh).

# ---- Entrada de referencia (ajuste ate a sequencial ficar entre 30 e 60 s) ----
# Tarefa curta (ver tarefa.h): estimada em ~1,7 ms na Atlantica (Xeon E5520),
# entao ~25000 tarefas dao T1 de ~42 s. CALIBRAR na Atlantica.
TAREFAS=${TAREFAS:-20000} # tarefas no saco
AMOSTRAS=${AMOSTRAS:-20}  # imagens de treino por tarefa
EPOCAS=${EPOCAS:-1}       # epocas por tarefa
# Granularidade escolhida (tarefas por mensagem). E um compromisso entre dois
# custos que granularidade.sbatch mede com P = 32:
#  - g pequeno: cada tarefa paga uma ida e volta e um atendimento do mestre,
#    que atende 31 escravos em serie. Entre nos a ida e volta medida e de
#    ~190 us (1 us dentro do no): ~11% de uma tarefa de 1,7 ms com g = 1;
#  - g grande: poucos blocos por escravo, e no fim do saco escravos ficam
#    ociosos esperando o ultimo bloco (ate g * tempo da tarefa).
# PROVISORIO: fixe aqui o g de menor mediana em granularidade.sbatch (e
# atualize este comentario com os numeros) antes de rodar a forte e a fraca.
G=${G:-16}
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
    # lido por analise.py para marcar nos graficos onde comecam HT e sobrecarga
    echo "TOPOLOGIA,$NUCLEOS_POR_NO,$THREADS_POR_NO"
}

# na_ordem REP lista...: a lista em ordem crescente nas repeticoes impares e
# decrescente nas pares. Sem isso, cada configuracao rodaria sempre depois da
# mesma vizinha (g = 1 logo apos g = 512, que deixa o no quase ocioso), e um
# efeito de ordem (temperatura, turbo) se confundiria com o da configuracao.
na_ordem() {
    local rep=$1
    shift
    local lista=("$@") i
    if [ $(( rep % 2 )) -eq 1 ]; then
        echo "${lista[@]}"
    else
        for (( i = ${#lista[@]} - 1; i >= 0; i-- )); do printf '%s ' "${lista[i]}"; done
        echo
    fi
}

# Topologia lida do proprio no. O enunciado fala em 2 x 8 nucleos com HT, mas
# os nos da Atlantica medidos tem 2 x 4 nucleos com HT (8 fisicos, 16 threads).
NUCLEOS_POR_NO=$(lscpu | awk -F: '/^Socket\(s\)/{s=$2} /^Core\(s\) per socket/{c=$2} END{print s*c}')
THREADS_POR_NO=$(lscpu | awk -F: '/^CPU\(s\)/{print $2+0}')

# lancar P NOS programa args...
# P processos divididos igualmente entre NOS nos, em bloco (ranks 0..P/NOS-1 no
# primeiro no, o mestre incluido), com srun como no Lab 3. Tres regimes,
# conforme os processos por no (PPN):
#  - PPN <= nucleos fisicos: um processo por nucleo (--hint=nomultithread; sem
#    isso o Slurm pode por dois ranks nas duas threads do mesmo nucleo);
#  - PPN <= threads: os processos ocupam as threads de HT;
#  - PPN > threads: mais processos que threads (--overcommit), o SO reveza.
# Nos nos de 8 nucleos/16 threads: P <= 16, P = 32 e P = 64, respectivamente.
# "verbose" escreve na saida a mascara de CPUs de cada rank.
lancar() {
    local P=$1 NOS=$2
    shift 2
    local PPN=$(( (P + NOS - 1) / NOS ))
    local EXTRA=(--hint=nomultithread)
    if [ "$PPN" -gt "$THREADS_POR_NO" ]; then
        EXTRA=(--hint=multithread --overcommit)
    elif [ "$PPN" -gt "$NUCLEOS_POR_NO" ]; then
        EXTRA=(--hint=multithread)
    fi
    srun -N "$NOS" -n "$P" --ntasks-per-node="$PPN" --distribution=block "${EXTRA[@]}" --cpu-bind=verbose "$@"
}
