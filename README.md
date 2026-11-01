# T2 — Mestre/escravo com MPI: busca de hiperparâmetros de uma MLP

Versão paralela, no modelo mestre/escravo com MPI, de uma busca de hiperparâmetros
de uma rede MLP treinada no MNIST. É executada e medida no cluster Atlântica (LAD).

**Origem do código.** A rede neural (`NeuralNetwork.h/.cpp`, `load_mnist.h`, `loadcsv.h`,
`main.cpp`) vem de [muchlakshay/MLP-From-Scratch](https://github.com/muchlakshay/MLP-From-Scratch)
(licença MIT, arquivo `LICENSE`). No T2 ela mudou em três pontos: semente fixa opcional,
`learn()` silencioso quando `verbose=false` e leitura parcial do MNIST. O resto é deste trabalho.

## O problema

Uma **tarefa** (a unidade de trabalho) treina uma MLP `784-h-h-10` e conta os acertos nas
200 primeiras imagens de teste do MNIST. A configuração sai do índice da tarefa
(`config_da_tarefa` em `tarefa.cpp`):
- `h` ∈ {16, 32} e taxa ∈ {0,05; 0,1; 0,2};
- uma das 50 fatias de `amostras` imagens do treino (100 na entrada de referência);
- 2 épocas, lote 8, semente = índice.

Com 100 imagens, o acerto médio é de cerca de 30%, contra 10% do acaso. As tarefas são
independentes. Cada mensagem leva um bloco de **g** tarefas consecutivas, descrito por 2
inteiros `{inicio, qtd}`, porque cada escravo lê o MNIST antes da região medida.

**Granularidade.** A tarefa foi reduzida ao mínimo que ainda treina (10,5 ms na Atlântica),
para dar a g pequeno a maior chance de pesar. O `granularidade.sbatch` varre g de 1 a 512
com P = 32, e o g de menor mediana vai para `G` em `scripts/comum.sh` antes da forte e da
fraca. O escolhido foi **g = 1**:
- a ida e volta entre nós (~190 µs) custa 0,6% de uma tarefa com P = 32, então agrupar não
  tem o que economizar;
- com g grande, os escravos ficam ociosos no fim do saco; com g = 256 e 512 há menos blocos
  que escravos, e o tempo vai de 4 s para 10 s e 20 s.

Checksum impresso em toda execução: `soma` = Σ acertos[i] e `ordem` = Σ (i+1)·acertos[i].
O segundo só coincide com o da sequencial se cada resultado voltou para a sua posição no saco.

| Arquivo | Conteúdo |
| --- | --- |
| `tarefa.h/.cpp` | Unidade de trabalho e checksum, usados pelas duas versões |
| `sequencial.cpp` | Versão sequencial de referência (sem MPI) |
| `mestre_escravo.cpp` | Versão MPI (tags `TAG_PEDIDO`, `TAG_TAREFA`, `TAG_RESULTADO`, `TAG_FIM`) |
| `pingpong.cpp` | Ida e volta com os mesmos tamanhos de mensagem do mestre/escravo |
| `scripts/preparar.sh` | Baixa o Eigen 3.4.0 e o MNIST |
| `scripts/comum.sh` | Entrada de referência e a forma de lançar os processos nos nós |
| `scripts/*.sbatch` | Um script por experimento |
| `scripts/analise.py` | Tabelas (mediana, mín., máx.), speed-up, eficiência e gráficos |
| `resultados/` | Saídas brutas do Slurm usadas nas tabelas e o que `analise.py` gera |
| `resultados/ordem_fixa/` | 1ª granularidade (job 41640), com os g sempre na mesma ordem; refeita com a ordem alternada e mantida como registro |
| `resultados/extra/` | Granularidade submetida em duplicata (job 41672) e cancelada no meio |

## Preparar e compilar (na Atlântica)

```bash
git clone https://github.com/heloysapelizon/MLP-comp-par-t2.git && cd MLP-comp-par-t2
bash scripts/preparar.sh          # Eigen em eigen/, MNIST em MNIST/
module load <openmpi>             # se o cluster exigir; confira com `module avail`
make                              # sequencial, mestre_escravo, pingpong
```

Flags: `-std=c++17 -O3 -Wall -DNDEBUG`, iguais nas duas versões, e as duas são compiladas
pelo mesmo `mpicxx`; a sequencial não chama MPI. Sem `-fopenmp`, o Eigen roda
com uma thread por processo. Sem `-march=native`, para o ponto flutuante (e portanto o
checksum) não depender do nó.

Teste rápido de equivalência (os três checksums devem ser iguais):

```bash
./sequencial 13
srun -N 1 -n 2 ./mestre_escravo 1 13
srun -N 1 -n 7 ./mestre_escravo 4 13
```

Uso dos programas:

```
./sequencial [tarefas=4000] [amostras=100] [epocas=2] [passo=0]    # passo: checksums dos prefixos passo*2^k
srun -n P ./mestre_escravo g [tarefas=4000] [amostras=100] [epocas=2]   # P >= 2, g = tarefas por mensagem
srun -n 2 ./pingpong g1 [g2 ...]
```

## Experimentos

A entrada de referência e a granularidade ficam em `scripts/comum.sh` (`TAREFAS`, `AMOSTRAS`,
`EPOCAS`, `G`, `REPS`). Ajuste `TAREFAS` até a mediana da sequencial ficar entre 30 e 60 s.
Os nós são passados na linha de comando, sempre os mesmos dois:

```bash
NOS=<no1>,<no2>
# 1. referência e escolha de g
sbatch --nodelist=${NOS%%,*} scripts/sequencial.sbatch      # T1, 10 repetições, 1 nó       (~7 min)
sbatch --nodelist=$NOS scripts/granularidade.sbatch         # P = 32, g = 1..512, pingpong  (~15 min)
python3 scripts/analise.py resultados                       # escolha g e fixe G em scripts/comum.sh
# 2. escalabilidade, já com o G escolhido
sbatch --nodelist=$NOS scripts/forte.sbatch                 # P = 2..64, mais P = 8 em 1 nó  (~15 min na Atlântica)
sbatch --nodelist=$NOS scripts/fraca.sbatch                 # tarefas = TAREFAS * P         (~1h50)
sbatch --nodelist=${NOS%%,*} scripts/checksum_fraca.sbatch  # checksums de referência da fraca (~45 min)
```

Se o tempo máximo da partição não comportar a fraca, divida o job pelos valores de P:
`sbatch --nodelist=$NOS --export=ALL,P_LISTA="2 4 8" scripts/fraca.sbatch` e depois
`P_LISTA="16 32 64"`. Isso também vale para a forte.

A fraca não tem uma sequencial de mesmo tamanho para comparar o checksum. Por isso o
`checksum_fraca.sbatch` roda a sequencial uma vez com 64 × `TAREFAS` tarefas e imprime o
checksum de cada prefixo `TAREFAS` × 2^k. O checksum de N tarefas é o do prefixo de tamanho N,
então essa execução única serve de referência para todos os tamanhos da fraca.

Todos usam `--exclusive`. Os processos se dividem igualmente entre os 2 nós, em bloco: ranks
`0..P/2-1` no primeiro nó, com o mestre, e o resto no segundo
(`srun --distribution=block`, como no Lab 3).

**Topologia medida.** Os nós da Atlântica são Xeon E5520, com 2 × 4 núcleos com HT: 8 núcleos
físicos e 16 threads por nó, e não os 2 × 8 do enunciado. A ida e volta medida é de ~0,9 µs
dentro do nó e ~200 µs entre nós. Os scripts leem a topologia com `lscpu` e escolhem o
regime pelo número de processos por nó:

| P | Processos por nó | Regime |
| --- | --- | --- |
| 2 a 16 | até 8 | um processo por núcleo físico (`--hint=nomultithread`) |
| 32 | 16 | HT (`--hint=multithread`) |
| 64 | 32 | mais processos que threads (`--overcommit`) |

A "forte em 1 nó" usa P = 8, um processo por núcleo de um único nó, para comparar com P = 8
dividido entre os 2 nós.

`--cpu-bind=verbose` registra na saída onde cada rank ficou. **No primeiro job, confira**
que com P = 16 cada rank caiu num núcleo físico diferente, sem dois ranks nas duas threads
do mesmo núcleo. Para mudar a distribuição, altere só a função `lancar` em
`scripts/comum.sh`.

## Análise

```bash
python3 scripts/analise.py resultados    # precisa de matplotlib
```

Gera `resultados/tabelas.md`, com mediana, mínimo e máximo em todas as tabelas:
- equivalência de checksums;
- sequencial;
- forte, com E = S/P, E por escravo = S/(P−1) e a decomposição E = (P−1)/P × t_seq/t_tarefa × ocupação;
- fraca, com o trabalho relativo à referência;
- granularidade;
- carga por escravo no g escolhido;
- pingpong, com a razão computação/comunicação.

O `analise.py` só lê os `.out` que estão diretamente em `resultados/`; os das subpastas ficam
guardados sem entrar nas tabelas. Gera também `resultados/escravos.csv`, com todas as linhas por escravo, e os gráficos
`speedup.pdf` e `eficiencia.pdf`.

A entrada de referência é a da sequencial. Execuções da forte, da fraca ou da granularidade
com outra entrada, ou da forte e da fraca com outro g, ficam de fora e aparecem em "Avisos"
no topo de `tabelas.md`. Mesmo assim, faça as calibragens fora de `resultados/` (por exemplo,
`srun -N 1 ./sequencial 2000`), para que a pasta guarde só as execuções do relatório.

Linhas que os programas imprimem e o `analise.py` lê:

```
RUN,experimento,P,processos_por_no,repeticao                  (scripts sbatch)
CSV,versao,P,tarefas,g,amostras,epocas,tempo_s,soma,ordem     (sequencial e mestre_escravo)
MESTRE,host
ESCRAVO,P,tarefas,g,rank,host,tarefas_proc,mensagens,ocupado_s,ultimo_resultado_s
PINGPONG,g,bytes_ida,bytes_volta,host0,host1,ida_e_volta_us
```

O programa original de treino completo no MNIST continua disponível com `make demo && ./demo`.
