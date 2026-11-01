# Como rodar os experimentos e coletar os resultados

Passo a passo para rodar o T2 no cluster Atlântica, do login até os resultados no
GitHub. Os comandos com o prompt `$` rodam **na Atlântica**; os marcados como
**no Mac** rodam no seu computador.

## 1. Entrar na Atlântica

A Atlântica só é acessível pela sparta. Use o seu usuário da PUCRS na sparta e o
usuário do LAD na Atlântica:

```bash
# no Mac
ssh -o PubkeyAuthentication=no <matricula>@portoalegre@sparta.pucrs.br
# já na sparta
ssh <usuario_lad>@atlantica.lad.pucrs.br
```

O prompt deve mudar para `<usuario_lad>@atlantica:~$`. Todo o resto deste guia
roda aí, salvo quando indicado.

## 2. Preparar o repositório

**Só na primeira vez:**

```bash
cd ~
git clone https://github.com/heloysapelizon/MLP-comp-par-t2.git
cd MLP-comp-par-t2
bash scripts/preparar.sh      # baixa o Eigen e o MNIST (precisa de internet: rode no nó de login)
make
```

**Nas vezes seguintes**, para pegar a versão mais nova:

```bash
cd ~/MLP-comp-par-t2
git checkout -- .             # descarta mudanças locais (por exemplo, tabelas regeradas)
git pull
make
```

**Teste de que tudo funciona** (~5 s). As três linhas precisam mostrar os mesmos dois
últimos números (o checksum):

```bash
./sequencial 13 | grep CSV
srun -N 1 -n 2 ./mestre_escravo 1 13 | grep CSV
srun -N 1 -n 7 ./mestre_escravo 4 13 | grep CSV
```

## 3. Escolher os nós

```bash
sinfo -N -o "%N %c %z %T"
```

Escolha dois nós `idle` (livres) e use **sempre os mesmos dois** em todos os jobs, como
pede o enunciado. Nos exemplos abaixo:

```bash
NOS=atlantica03,atlantica04
```

A variável `NOS` vale só para a sessão atual. Se você sair e entrar de novo, defina-a
outra vez.

## 4. Calibrar o número de tarefas

O T1 (tempo da sequencial) precisa ficar entre 30 e 60 s. Meça o tempo de uma tarefa
num nó de cálculo:

```bash
srun -N 1 -w ${NOS%%,*} --exclusive ./sequencial 5000 | grep tempo
```

Calcule `TAREFAS ≈ 42 / tempo_por_tarefa`. Por exemplo, com 0,0017 s por tarefa,
dá 42 / 0,0017 ≈ 25000. Ponha o valor em `scripts/comum.sh`:

```bash
nano scripts/comum.sh         # edite a linha TAREFAS=${TAREFAS:-25000}; Ctrl+O, Enter, Ctrl+X
```

## 5. Teste rápido de granularidade (opcional, ~10 min)

Serve para conferir, antes da rodada completa, que g = 1 fica mais lento que um g
intermediário. A saída vai para `testes/`, que não entra no git nem na análise:

```bash
mkdir -p testes
REPS=3 TAREFAS=10000 G_LISTA="1 4 16 64 256" sbatch --nodelist=$NOS --output=testes/gran_%j.out scripts/granularidade.sbatch
```

Quando terminar (ver o passo 7), compare os tempos de cada g:

```bash
grep -E '^(RUN|CSV)' testes/gran_*.out
```

## 6. Rodada oficial

Rode nesta ordem. Sua conta aceita **no máximo 2 jobs na fila** ao mesmo tempo
(erro `AssocMaxSubmitJobLimit`); por isso os jobs vão em pares, esperando o par
anterior terminar.

**6.1. Sequencial e granularidade** (~7 min + ~20 min):

```bash
sbatch --nodelist=${NOS%%,*} scripts/sequencial.sbatch
sbatch --nodelist=$NOS scripts/granularidade.sbatch
```

**6.2. Escolher g.** Quando os dois terminarem:

```bash
python3 scripts/analise.py resultados | grep -A16 "Granularidade, P = 32"
```

O g com a menor mediana é o escolhido. Ponha-o em `scripts/comum.sh`, na linha
`G=${G:-...}`, e atualize o comentário acima dela com os números que justificam a
escolha:

```bash
nano scripts/comum.sh
```

**6.3. Escalabilidade forte e fraca** (~15 min + ~2 h):

```bash
sbatch --nodelist=$NOS scripts/forte.sbatch
sbatch --nodelist=$NOS scripts/fraca.sbatch
```

**6.4. Checksums de referência da fraca** (~45 min). Submeta quando a forte terminar,
por causa do limite de 2 jobs:

```bash
sbatch --nodelist=${NOS%%,*} scripts/checksum_fraca.sbatch
```

Cada job grava a sua saída em `resultados/<experimento>_<número do job>.out`.

## 7. Acompanhar os jobs

```bash
squeue -u $USER                          # PD = na fila, R = rodando; sumiu da lista = terminou
tail -f resultados/forte_<job>.out       # acompanhar ao vivo (Ctrl+C para sair)
grep -c '^CSV' resultados/forte_<job>.out  # quantas execuções já terminaram
scancel <job>                            # cancelar um job
```

Se um job ficar muito tempo em `PD`, veja quem está usando os nós:
`squeue -w atlantica03`.

## 8. Conferir os resultados

Quando todos os jobs terminarem:

```bash
grep -iE 'error|invalid|cancel|time limit' resultados/*.out | grep -v Vulnerab   # deve sair vazio
python3 scripts/analise.py resultados
```

Na saída, confira:
- **Avisos:** não deve aparecer. Se aparecer, há execuções com outra entrada ou outro g
  misturadas em `resultados/`.
- **Equivalência:** todas as linhas devem dizer `OK`, inclusive os tamanhos da fraca.
- **Coluna `n`:** 10 em todas as tabelas.
- **Sequencial:** a mediana deve ficar entre 30 e 60 s.

O `analise.py` grava `resultados/tabelas.md`, `resultados/escravos.csv`,
`resultados/speedup.pdf/.png` e `resultados/eficiencia.pdf/.png`. Se ele disser
"matplotlib ausente", instale com `python3 -m pip install --user matplotlib` e rode de
novo, ou gere os gráficos no Mac (passo 9).

Se um job tiver sido submetido em duplicata ou cancelado no meio, tire a saída dele da
pasta antes da análise: `mkdir -p resultados/extra && mv resultados/<arquivo>.out resultados/extra/`.
O `analise.py` só lê os `.out` que estão diretamente em `resultados/`.

## 9. Coletar os resultados

O enunciado exige as saídas brutas do Slurm no repositório.

**Opção A: enviar da Atlântica para o GitHub.** O GitHub não aceita a senha comum no
`git push`: é preciso um *token*. Para criar um, no navegador, vá em github.com, clique na
sua foto, depois **Settings → Developer settings → Personal access tokens → Tokens
(classic) → Generate new token (classic)**. Marque o escopo `repo` e copie o token. Depois:

```bash
git config user.name "Seu Nome"          # só na primeira vez
git config user.email "seu@email"        # só na primeira vez
git add resultados scripts/comum.sh
git commit -m "Saidas do Slurm e tabelas do T2"
git pull --rebase
git push        # usuário: seu login do GitHub; senha: cole o TOKEN (não aparece nada ao colar)
```

Sua conta precisa ter acesso de escrita ao repositório. Se der erro `403`, use a opção B.

**Opção B: copiar para o Mac.** No Mac, com o mesmo usuário e senha do passo 1:

```bash
# no Mac
scp -r -o PubkeyAuthentication=no \
  -o ProxyCommand="ssh -o PubkeyAuthentication=no -W %h:%p <matricula>@portoalegre@sparta.pucrs.br" \
  <usuario_lad>@atlantica.lad.pucrs.br:~/MLP-comp-par-t2/resultados ~/MLP-comp-par-t2/
```

Ele pede duas senhas: a da sparta e a da Atlântica. Depois, no Mac:

```bash
# no Mac
cd ~/MLP-comp-par-t2
python3 scripts/analise.py resultados    # tabelas e gráficos
git add resultados scripts/comum.sh
git commit -m "Saidas do Slurm e tabelas do T2"
git push
```

Se o `G` ou o `TAREFAS` foram alterados no cluster, faça a mesma alteração em
`scripts/comum.sh` no Mac antes do commit, para o repositório ficar igual ao que rodou.

## Problemas comuns

| Sintoma | Causa e solução |
| --- | --- |
| `bash: scripts/preparar.sh: No such file or directory` | Você está fora da pasta do repositório: `cd ~/MLP-comp-par-t2`. |
| `zsh: command not found: srun` | O comando rodou no Mac, não na Atlântica. Entre no cluster (passo 1). |
| `sbatch: error: AssocMaxSubmitJobLimit` | Limite de jobs na fila. Espere um job terminar e submeta de novo. |
| `srun` parado, sem saída | O nó pedido está ocupado por outro job. Veja com `squeue -w <nó>` e use outro nó livre. |
| `git push` recusa a senha | O GitHub exige um token no lugar da senha (passo 9, opção A). |
| `git pull` reclama de alterações locais | Rode `git checkout -- .` (descarta só arquivos regerados) e repita. |
| "Avisos" no topo de `tabelas.md` | Há saídas antigas ou de teste em `resultados/`. Mova-as para uma subpasta. |
| Equivalência diz `DIVERGE` | O checksum mudou entre execuções. Confira se todas usaram o mesmo binário (`== git:` no início de cada saída) e investigue antes de seguir. |
