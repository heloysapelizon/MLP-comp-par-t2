# Primeira rodada: tarefa de 10 ms

Saídas do Slurm, tabelas e gráficos com a tarefa anterior: 100 imagens, 2 épocas,
validação em 200 imagens, 4000 tarefas (T1 = 42,07 s), g = 1.

Foi refeita com uma tarefa mais curta porque, com esta, a ida e volta entre nós
(~190 µs) pesava só 0,6% de uma tarefa e g = 1 já era o melhor: não havia
granularidade menor para comparar, como pede o enunciado.

Para reproduzir: `git checkout cbd2c2c` (código usado nos jobs) e
`python3 scripts/analise.py resultados/v1_tarefa_10ms`.
