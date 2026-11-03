# T2 — Mestre/escravo com MPI: busca de hiperparâmetros de uma MLP
Heloysa Pelizon | Marina Bon
Repositório: https://github.com/heloysapelizon/MLP-comp-par-t2

Versão paralela, no modelo **mestre/escravo** com MPI, de uma busca de hiperparâmetros de
uma rede MLP treinada no MNIST, executada e medida no cluster Atlântica (LAD). Como pede o
enunciado, o foco é a **análise**: explicar as causas dos números, não repetir em texto o
que o gráfico mostra.

**Origem do código.** A rede neural (`NeuralNetwork.h/.cpp` e `load_mnist.h`) vem de
[muchlakshay/MLP-From-Scratch](https://github.com/muchlakshay/MLP-From-Scratch)
(licença MIT, arquivo `LICENSE`). No T2 ela mudou em três pontos: semente fixa opcional,
`learn()` silencioso quando `verbose=false` e leitura parcial do MNIST. O resto é deste trabalho.

## O problema e a tarefa

Uma **tarefa** (a unidade de trabalho) treina uma MLP `784-h-h-10` e conta os acertos num
conjunto fixo de validação. A configuração sai do índice da tarefa (`config_da_tarefa` em
`tarefa.cpp`):
- `h` ∈ {16, 32} e taxa ∈ {0,05; 0,1; 0,2};
- uma das 500 fatias de `amostras` imagens do treino;
- 1 época, lote 8, semente = índice.

As tarefas são **independentes** e mais numerosas que os escravos, e treinar cada uma custa
muito mais do que enviá-la: é o que torna o problema adequado ao modelo mestre/escravo. Cada
mensagem leva um bloco de **g** tarefas consecutivas, descrito por 2 inteiros
`{início, qtd}` (8 bytes), porque cada escravo lê o MNIST antes da região medida.

A tarefa foi dimensionada curta de propósito: assim g = 1 expõe de fato o custo das
mensagens e dá o que comparar com granularidades maiores (exigido pelo enunciado). A
primeira rodada, com uma tarefa de ~10 ms, está em `resultados/v1_tarefa_10ms/` (ver o
LEIAME da pasta).

**Equivalência.** Toda execução imprime um checksum: `soma` = Σ acertos[i] e
`ordem` = Σ (i+1)·acertos[i]. O segundo só coincide com o da sequencial se cada resultado
voltou para a sua posição no saco.

## Arquivos

| Arquivo | Conteúdo |
| --- | --- |
| `tarefa.h/.cpp` | Unidade de trabalho e checksum, usados pelas duas versões |
| `sequencial.cpp` | Versão sequencial de referência (sem MPI) |
| `mestre_escravo.cpp` | Versão MPI mestre/escravo |
| `pingpong.cpp` | Ida e volta com os mesmos tamanhos de mensagem do mestre/escravo |
| `scripts/preparar.sh` | Baixa o Eigen 3.4.0 e o MNIST |
| `scripts/comum.sh` | Entrada de referência e o lançamento dos processos nos nós |
| `scripts/*.sbatch` | Um script por experimento |
| `scripts/analise.py` | Tabelas (mediana/mín./máx.), speed-up, eficiência e gráficos |
| `scripts/graficos.py` | Só os dois gráficos exigidos (atalho do `analise.py`) |
| `resultados/` | Saídas brutas do Slurm e o que o `analise.py` gera |
| `resultados/v1_tarefa_10ms/` | Primeira rodada, com a tarefa de ~10 ms |

## Preparar e compilar (na Atlântica)

```bash
git clone https://github.com/heloysapelizon/MLP-comp-par-t2.git && cd MLP-comp-par-t2
bash scripts/preparar.sh          # Eigen em eigen/, MNIST em MNIST/
module load <openmpi>             # se o cluster exigir; veja `module avail`
make                              # sequencial, mestre_escravo, pingpong
```

Flags iguais nas duas versões (`-std=c++17 -O3 -Wall -DNDEBUG`), ambas pelo mesmo `mpicxx`;
a sequencial não chama MPI. Sem `-fopenmp` (Eigen com uma thread por processo) e sem
`-march=native`, para o ponto flutuante — e portanto o checksum — não dependerem do nó.

Uso dos programas:

```
./sequencial [tarefas=25000] [amostras=20] [epocas=1] [passo=0]
srun -n P ./mestre_escravo g [tarefas=25000] [amostras=20] [epocas=1]   # P >= 2
srun -n 2 ./pingpong g1 [g2 ...]
```

## Ambiente e regras de medição

- Sempre nos **mesmos 2 nós**, com `--exclusive` e jobs em lote (`sbatch`).
- O enunciado especifica nós de 2 × 8 núcleos físicos com HT (16 físicos, 32 threads por
  nó). **Confirme com `lscpu`**: os scripts leem a topologia e escolhem o regime pelo número
  de processos por nó — um processo por núcleo físico, HT quando passa dos núcleos físicos e
  overcommit se passar das threads. O mestre ocupa um núcleo e conta em P.
- Processos divididos em bloco entre os 2 nós (`srun --distribution=block`): ranks
  `0..P/2-1` no primeiro, com o mestre. `--cpu-bind=verbose` registra onde cada rank ficou.
- **Região medida**: `MPI_Wtime()` no mestre, do início da distribuição até o último
  resultado; a leitura do MNIST fica fora, igual nas duas versões.
- Entrada de referência dimensionada para **T1 entre 30 e 60 s**; **10 repetições** por
  configuração (mediana, mínimo e máximo nas tabelas).

## Reproduzir os resultados

Tudo roda na Atlântica (acesso via sparta, com o usuário do LAD). Os parâmetros ficam em
`scripts/comum.sh` (`TAREFAS`, `AMOSTRAS`, `EPOCAS`, `G`, `REPS`).

1. **Compilar** (acima) e conferir a equivalência — os três checksums devem bater:
   ```bash
   ./sequencial 13 | grep CSV
   srun -N 1 -n 2 ./mestre_escravo 1 13 | grep CSV
   srun -N 1 -n 7 ./mestre_escravo 4 13 | grep CSV
   ```
2. **Calibrar** `TAREFAS` para T1 ∈ [30, 60] s (medição fora de `resultados/`) e pôr o valor
   em `scripts/comum.sh`:
   ```bash
   srun -N 1 --exclusive ./sequencial 5000 | grep tempo   # TAREFAS ≈ 42 / tempo_por_tarefa
   ```
3. **Escolher 2 nós** livres e usar sempre os mesmos: `sinfo -N -o "%N %t"`, depois
   `NOS=no1,no2`.
4. **Rodar, nesta ordem** (a conta aceita no máx. 2 jobs na fila):
   ```bash
   # referência e escolha de g
   sbatch --nodelist=${NOS%%,*} scripts/sequencial.sbatch
   sbatch --nodelist=$NOS       scripts/granularidade.sbatch    # P = 32, g = 1..2048 + pingpong
   python3 scripts/analise.py resultados   # veja "Granularidade, P = 32" e fixe G em comum.sh
   # escalabilidade, já com o G escolhido
   sbatch --nodelist=$NOS       scripts/forte.sbatch            # P = 2,4,8,16,32,64 (+ P=8 em 1 nó)
   sbatch --nodelist=$NOS       scripts/fraca.sbatch            # tarefas = TAREFAS * P
   sbatch --nodelist=${NOS%%,*} scripts/checksum_fraca.sbatch   # checksums de referência da fraca
   ```
   Acompanhe com `squeue -u $USER`. Se a partição não comportar a fraca inteira, divida por
   P: `--export=ALL,P_LISTA="2 4 8"` e depois `"16 32 64"` (vale para a forte também).
5. **Gerar tabelas e gráficos**:
   ```bash
   python3 scripts/analise.py resultados    # tabelas.md, escravos.csv, speedup.*, eficiencia.*
   python3 scripts/graficos.py resultados   # só os 2 gráficos exigidos (precisa de matplotlib)
   ```

O `analise.py` só lê os `.out` **diretamente** em `resultados/` — mantenha calibragens e
testes fora dessa pasta. Execuções com outra entrada, ou com g diferente do escolhido, ficam
de fora e aparecem em "Avisos" no topo de `tabelas.md`.

## Saídas da análise

`analise.py` gera, em `resultados/`:
- `tabelas.md` — equivalência, sequencial (T1), **forte** (S, E = S/P, E por escravo e a
  decomposição E = (P−1)/P × t_seq/t_tarefa × ocupação), **fraca**, **granularidade**, carga
  por escravo e pingpong (razão computação/comunicação);
- `escravos.csv` — todas as linhas por escravo;
- `speedup.pdf/.png` — speed-up da forte com a reta ideal (S = P);
- `eficiencia.pdf/.png` — eficiência da forte (S/P) e da fraca (T1/TP).

`graficos.py` regenera só essas duas figuras, a partir das mesmas saídas.

Linhas que os programas imprimem e a análise lê:

```
RUN,experimento,P,processos_por_no,repeticao                  (scripts sbatch)
CSV,versao,P,tarefas,g,amostras,epocas,tempo_s,soma,ordem     (sequencial e mestre_escravo)
MESTRE,host
ESCRAVO,P,tarefas,g,rank,host,tarefas_proc,mensagens,ocupado_s,ultimo_resultado_s
PINGPONG,g,bytes_ida,bytes_volta,host0,host1,ida_e_volta_us
```
