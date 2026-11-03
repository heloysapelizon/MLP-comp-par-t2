#!/usr/bin/env python3
"""Gera so os dois graficos exigidos pelo relatorio do T2, a partir das saidas
do Slurm (as mesmas linhas RUN/CSV/ESCRAVO que o analise.py le):

  speedup.pdf/.png     speed-up da forte com a reta ideal (S = P)
  eficiencia.pdf/.png  eficiencia da forte (S/P) e da fraca (T1/TP)

Uso: python3 scripts/graficos.py [diretorio]      (padrao: resultados)

E um atalho para quem quer so as figuras: o analise.py gera estes mesmos
arquivos junto das tabelas. A leitura das saidas, a selecao da entrada de
referencia e do g, e a plotagem sao reaproveitadas de analise.py, para que os
dois produzam exatamente o mesmo grafico. Precisa de matplotlib.
"""
import statistics
import sys
from collections import defaultdict

import analise  # mesmo diretorio: le_saidas, resumo, graficos, TOPOLOGIA


def escalabilidade(execucoes):
    """Reconstroi os dicionarios forte e fraca que o graficos() de analise.py
    espera, com as mesmas regras do analise.py:
      - entrada de referencia = a mais frequente na sequencial;
      - a forte usa essa entrada; a fraca, P vezes mais tarefas;
      - um unico g, o mais frequente na forte/fraca;
      - mediana das repeticoes por P.

    forte[P] = (S, S_min, S_max) com S = T1 / mediana(TP)
    fraca[P] = T1 / mediana(TP)
    """
    por_exp = defaultdict(list)
    for e in execucoes:
        por_exp[e["exp"]].append(e)

    seq = [e for e in por_exp.get("sequencial", []) if e["versao"] == "sequencial"]
    if not seq:
        sys.exit("faltam as execucoes da sequencial (scripts/sequencial.sbatch)")
    ref = statistics.mode((e["N"], e["amostras"], e["epocas"]) for e in seq)
    tempos_seq = [e["tempo"] for e in seq if (e["N"], e["amostras"], e["epocas"]) == ref]
    T1 = statistics.median(tempos_seq)

    def da_referencia(e, fator):
        return (e["N"], e["amostras"], e["epocas"]) == (ref[0] * fator, ref[1], ref[2])

    forte_exec = [e for e in por_exp.get("forte", []) if da_referencia(e, 1)]
    fraca_exec = [e for e in por_exp.get("fraca", []) if da_referencia(e, e["P"])]

    candidatos = (forte_exec
                  + [e for e in por_exp.get("forte_1no", []) if da_referencia(e, 1)]
                  + fraca_exec)
    if candidatos:
        g = statistics.mode(e["g"] for e in candidatos)
        forte_exec = [e for e in forte_exec if e["g"] == g]
        fraca_exec = [e for e in fraca_exec if e["g"] == g]

    forte = {}
    por_p = defaultdict(list)
    for e in forte_exec:
        por_p[e["P"]].append(e)
    for P, es in por_p.items():
        med, mn, mx = analise.resumo([e["tempo"] for e in es])
        forte[P] = (T1 / med, T1 / mx, T1 / mn)

    fraca = {}
    por_p = defaultdict(list)
    for e in fraca_exec:
        por_p[e["P"]].append(e)
    for P, es in por_p.items():
        fraca[P] = T1 / statistics.median(e["tempo"] for e in es)

    return forte, fraca


def main():
    diretorio = sys.argv[1] if len(sys.argv) > 1 else "resultados"
    execucoes, _ = analise.le_saidas(diretorio)   # tambem preenche analise.TOPOLOGIA
    if not execucoes:
        sys.exit(f"nenhuma linha CSV encontrada em {diretorio}/*.out")
    forte, fraca = escalabilidade(execucoes)
    if not forte and not fraca:
        sys.exit("sem execucoes da forte nem da fraca com a entrada de referencia")
    analise.graficos(diretorio, forte, fraca)
    print(f"graficos em {diretorio}/: speedup.pdf/.png, eficiencia.pdf/.png "
          f"(forte: {sorted(forte)}; fraca: {sorted(fraca)})")


if __name__ == "__main__":
    main()
