#!/usr/bin/env python3
"""Le as saidas do Slurm e gera as tabelas e os graficos do T2.

Uso: python3 scripts/analise.py [diretorio]      (padrao: resultados)

Entrada: as linhas RUN, CSV, ESCRAVO, MESTRE e PINGPONG que os scripts sbatch e
os programas imprimem. Saida, no mesmo diretorio:
  tabelas.md                    uma tabela por experimento (mediana, min, max)
  escravos.csv                  todas as linhas ESCRAVO, com o experimento
  speedup.pdf/.png              speed-up da forte com a reta ideal
  eficiencia.pdf/.png           eficiencia da forte e da fraca
"""
import csv
import glob
import os
import statistics
import sys
from collections import defaultdict


TOPOLOGIA = {}  # nucleos e threads por no, da linha TOPOLOGIA dos scripts


def le_saidas(diretorio):
    execucoes, pingpong = [], []
    for arquivo in sorted(glob.glob(os.path.join(diretorio, "*.out"))):
        run = None
        with open(arquivo, errors="replace") as f:
            for linha in f:
                c = linha.strip().split(",")
                if c[0] == "RUN" and len(c) == 5:
                    run = dict(exp=c[1], P=int(c[2]), ppn=int(c[3]), rep=int(c[4]),
                               arquivo=os.path.basename(arquivo))
                elif c[0] == "CSV" and len(c) == 10 and run:
                    execucoes.append(dict(run, versao=c[1], N=int(c[3]), g=int(c[4]),
                                          amostras=int(c[5]), epocas=int(c[6]), tempo=float(c[7]),
                                          soma=int(c[8]), ordem=int(c[9]), escravos=[], mestre=""))
                elif c[0] == "MESTRE" and execucoes:
                    execucoes[-1]["mestre"] = c[1]
                elif c[0] == "ESCRAVO" and len(c) == 10 and execucoes:
                    execucoes[-1]["escravos"].append(dict(
                        rank=int(c[4]), host=c[5], tarefas=int(c[6]), mensagens=int(c[7]),
                        ocupado=float(c[8]), fim=float(c[9])))
                elif c[0] == "TOPOLOGIA" and len(c) == 3 and c[1].isdigit():
                    TOPOLOGIA.update(nucleos=int(c[1]), threads=int(c[2]))
                elif c[0] == "PINGPONG" and len(c) == 7:
                    pingpong.append(dict(g=int(c[1]), intra=c[4] == c[5], rtt_us=float(c[6])))
    return execucoes, pingpong


def resumo(valores):
    return statistics.median(valores), min(valores), max(valores)


def tabela(titulo, cabecalho, linhas):
    out = [f"### {titulo}", "", "| " + " | ".join(cabecalho) + " |",
           "|" + "---|" * len(cabecalho)]
    out += ["| " + " | ".join(str(x) for x in l) + " |" for l in linhas]
    return "\n".join(out) + "\n"


def f3(x):
    return f"{x:.3f}"


def main():
    diretorio = sys.argv[1] if len(sys.argv) > 1 else "resultados"
    execucoes, pingpong = le_saidas(diretorio)
    if not execucoes:
        sys.exit(f"nenhuma linha CSV encontrada em {diretorio}/*.out")
    por_exp = defaultdict(list)
    for e in execucoes:
        por_exp[e["exp"]].append(e)
    md = []

    # ---- Equivalencia: cada tamanho de entrada deve ter um unico checksum ----
    grupos = defaultdict(list)
    for e in execucoes:
        grupos[(e["N"], e["amostras"], e["epocas"])].append(e)
    linhas = []
    for (n, am, ep), es in sorted(grupos.items()):
        cks = sorted({(e["soma"], e["ordem"]) for e in es})
        tem_seq = any(e["versao"].startswith("sequencial") for e in es)
        ok = "OK" if len(cks) == 1 and tem_seq else ("DIVERGE" if len(cks) > 1 else "sem sequencial")
        linhas.append([n, am, ep, len(es), "; ".join(f"{s}/{o}" for s, o in cks),
                       "sim" if tem_seq else "nao", ok])
    md.append(tabela("Equivalencia (checksum soma/ordem por entrada)",
                     ["tarefas", "amostras", "epocas", "execucoes", "soma/ordem", "tem sequencial", "situacao"],
                     linhas))

    # ---- Sequencial (T1) ----
    seq = [e for e in por_exp.get("sequencial", []) if e["versao"] == "sequencial"]
    if not seq:
        sys.exit("faltam as execucoes da sequencial (scripts/sequencial.sbatch)")
    ref = statistics.mode((e["N"], e["amostras"], e["epocas"]) for e in seq)
    avisos = []
    outras = sorted({(e["N"], e["amostras"], e["epocas"]) for e in seq} - {ref})
    if outras:
        avisos.append(f"sequencial: entrada de referencia = {ref}; ignoradas as entradas {outras}")

    # Forte e granularidade usam a entrada de referencia; a fraca, P vezes mais
    # tarefas. Execucoes com outra entrada (de uma calibragem antiga, por
    # exemplo) ficam de fora, com aviso, em vez de se misturarem nas tabelas.
    def da_referencia(e, fator=1):
        return (e["N"], e["amostras"], e["epocas"]) == (ref[0] * fator, ref[1], ref[2])

    for exp, fator in (("forte", None), ("forte_1no", None), ("fraca", "P"), ("granularidade", None)):
        todas = por_exp.get(exp, [])
        boas = [e for e in todas if da_referencia(e, e["P"] if fator else 1)]
        if len(boas) < len(todas):
            avisos.append(f"{exp}: {len(todas) - len(boas)} execucoes com outra entrada ignoradas")
        por_exp[exp] = boas
    # Forte e fraca devem usar um unico g (o G escolhido); o mais frequente vale.
    g_escolhido = None
    escala = por_exp.get("forte", []) + por_exp.get("forte_1no", []) + por_exp.get("fraca", [])
    if escala:
        g_escolhido = statistics.mode(e["g"] for e in escala)
        for exp in ("forte", "forte_1no", "fraca"):
            todas = por_exp.get(exp, [])
            por_exp[exp] = [e for e in todas if e["g"] == g_escolhido]
            if len(por_exp[exp]) < len(todas):
                avisos.append(f"{exp}: {len(todas) - len(por_exp[exp])} execucoes com g != {g_escolhido} ignoradas")
    tempos_seq = [e["tempo"] for e in seq if (e["N"], e["amostras"], e["epocas"]) == ref]
    T1, mn, mx = resumo(tempos_seq)
    tempo_tarefa = T1 / ref[0]
    md.append(tabela("Sequencial (T1)", ["tarefas", "amostras", "epocas", "n", "mediana (s)", "min (s)",
                                         "max (s)", "tempo/tarefa (s)"],
                     [[*ref, len(tempos_seq), f3(T1), f3(mn), f3(mx), f"{tempo_tarefa:.4f}"]]))

    # ---- Forte ----
    forte = {}
    linhas = []
    for exp in ("forte", "forte_1no"):
        por_p = defaultdict(list)
        for e in por_exp.get(exp, []):
            por_p[e["P"]].append(e)
        for P in sorted(por_p):
            med, mn, mx = resumo([e["tempo"] for e in por_p[P]])
            S = T1 / med
            if exp == "forte":
                forte[P] = (S, T1 / mx, T1 / mn)
            nos = len({e["mestre"] for e in por_p[P]} | {s["host"] for e in por_p[P] for s in e["escravos"]})
            linhas.append([P, exp.replace("forte_1no", "1 no").replace("forte", "2 nos"), nos,
                           len(por_p[P]), f3(med), f3(mn), f3(mx), f"{S:.2f}", f"{S / P:.3f}",
                           f"{S / (P - 1):.3f}"])
    md.append(tabela(f"Escalabilidade forte (g = {g_escolhido})", ["P", "distribuicao", "hosts vistos", "n", "mediana (s)",
                                              "min (s)", "max (s)", "S", "E = S/P",
                                              "E por escravo = S/(P-1)"], linhas))

    # ---- Fraca ----
    fraca = {}
    linhas = []
    por_p = defaultdict(list)
    for e in por_exp.get("fraca", []):
        por_p[e["P"]].append(e)
    trabalho_ref = ref[0] * ref[1] * ref[2]
    for P in sorted(por_p):
        es = por_p[P]
        med, mn, mx = resumo([e["tempo"] for e in es])
        trabalho = es[0]["N"] * es[0]["amostras"] * es[0]["epocas"]
        fraca[P] = T1 / med
        linhas.append([P, es[0]["N"], trabalho, f"{trabalho / trabalho_ref:.1f}", len(es), f3(med), f3(mn),
                       f3(mx), f"{T1 / med:.3f}"])
    md.append(tabela(f"Escalabilidade fraca (E = T1/TP, g = {g_escolhido})", ["P", "tarefas", "amostras treinadas",
                                                          "trabalho / referencia", "n", "mediana (s)",
                                                          "min (s)", "max (s)", "E fraca"], linhas))

    # ---- Granularidade e balanceamento ----
    por_g = defaultdict(list)
    for e in por_exp.get("granularidade", []):
        por_g[e["g"]].append(e)
    linhas = []
    for g in sorted(por_g):
        es = por_g[g]
        med, mn, mx = resumo([e["tempo"] for e in es])

        def mediana_por_rep(fn):
            return statistics.median(fn(e) for e in es)
        tar_min = mediana_por_rep(lambda e: min(s["tarefas"] for s in e["escravos"]))
        tar_max = mediana_por_rep(lambda e: max(s["tarefas"] for s in e["escravos"]))
        oc_min = mediana_por_rep(lambda e: min(s["ocupado"] for s in e["escravos"]))
        oc_max = mediana_por_rep(lambda e: max(s["ocupado"] for s in e["escravos"]))
        msgs = mediana_por_rep(lambda e: sum(s["mensagens"] for s in e["escravos"]))
        # Ociosidade no fim do saco: quanto antes do ultimo resultado cada escravo
        # terminou; a media sobre os escravos e o tempo perdido por desbalanceamento.
        ocioso = mediana_por_rep(lambda e: statistics.mean(e["tempo"] - s["fim"] for s in e["escravos"]))
        por_tarefa = mediana_por_rep(lambda e: sum(s["ocupado"] for s in e["escravos"])
                                     / max(1, sum(s["tarefas"] for s in e["escravos"])))
        linhas.append([g, len(es), f3(med), f3(mn), f3(mx), int(msgs), f"{tar_min:g}-{tar_max:g}",
                       f"{oc_min:.2f}-{oc_max:.2f}", f3(ocioso), f"{por_tarefa:.4f}"])
    if linhas:
        p_gran = sorted({e["P"] for es in por_g.values() for e in es})
        md.append(tabela(f"Granularidade, P = {', '.join(map(str, p_gran))}"
                         " (valores por escravo: mediana das repeticoes)",
                         ["g", "n", "mediana (s)", "min (s)", "max (s)", "mensagens", "tarefas/escravo",
                          "ocupado/escravo (s)", "ocioso no fim, media (s)", "tempo/tarefa (s)"], linhas))

        # Carga por escravo com o g escolhido (o da forte; sem forte, o de menor mediana).
        g_tab = g_escolhido if g_escolhido in por_g else min(
            por_g, key=lambda g: statistics.median(e["tempo"] for e in por_g[g]))
        por_rank = defaultdict(list)
        for e in por_g[g_tab]:
            for s_ in e["escravos"]:
                por_rank[s_["rank"]].append((s_, e["tempo"]))
        linhas = []
        for r in sorted(por_rank):
            v = por_rank[r]
            linhas.append([r, v[0][0]["host"], f"{statistics.median(x['tarefas'] for x, _ in v):g}",
                           f3(statistics.median(x["ocupado"] for x, _ in v)),
                           f3(statistics.median(t - x["fim"] for x, t in v))])
        md.append(tabela(f"Carga por escravo, P = {', '.join(map(str, p_gran))}, g = {g_tab} "
                         "(mediana das repeticoes)",
                         ["rank", "host", "tarefas", "ocupado (s)", "ocioso no fim (s)"], linhas))

    if pingpong:
        por = defaultdict(lambda: defaultdict(list))
        for p in pingpong:
            por[p["g"]][p["intra"]].append(p["rtt_us"])
        linhas = []
        for g in sorted(por):
            def celula(vs):
                return "{:.2f} ({:.2f}-{:.2f})".format(*resumo(vs)) if vs else "-"
            pior = max(statistics.median(vs) for vs in por[g].values())
            linhas.append([g, 4 * (2 + g), celula(por[g].get(True)), celula(por[g].get(False)),
                           f"{g * tempo_tarefa * 1e6 / pior:,.0f}"])
        md.append(tabela("Ida e volta (pingpong), mediana (min-max), x computacao do bloco",
                         ["g", "bytes na volta", "dentro do no (us)", "entre nos (us)",
                          "computacao / comunicacao (pior mediana)"], linhas))

    if avisos:
        md.insert(0, "### Avisos\n\n" + "\n".join(f"- {a}" for a in avisos) + "\n")
    with open(os.path.join(diretorio, "tabelas.md"), "w") as f:
        f.write("\n".join(md))
    print("\n".join(md))

    with open(os.path.join(diretorio, "escravos.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["arquivo", "exp", "P", "tarefas_total", "g", "rep", "tempo_total", "rank", "host",
                    "tarefas", "mensagens", "ocupado_s", "ultimo_resultado_s"])
        for e in execucoes:
            for s in e["escravos"]:
                w.writerow([e["arquivo"], e["exp"], e["P"], e["N"], e["g"], e["rep"], e["tempo"], s["rank"],
                            s["host"], s["tarefas"], s["mensagens"], s["ocupado"], s["fim"]])

    graficos(diretorio, forte, fraca)


# Cores: duas primeiras posicoes da paleta categorica validada (azul, laranja);
# marcadores diferentes para continuar legivel impresso em preto e branco.
AZUL, LARANJA, CINZA, TEXTO = "#2a78d6", "#eb6834", "#8a8984", "#52514e"


def graficos(diretorio, forte, fraca):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib ausente: graficos nao gerados")
        return
    plt.rcParams.update({"font.size": 8, "axes.spines.top": False, "axes.spines.right": False,
                         "axes.edgecolor": CINZA, "axes.labelcolor": TEXTO, "xtick.color": TEXTO,
                         "ytick.color": TEXTO, "legend.frameon": False})

    def eixo_p(ax, ps):
        ax.set_xscale("log", base=2)
        ax.set_xticks(ps)
        ax.set_xticklabels([str(p) for p in ps])
        ax.set_xlabel("processos (P)")
        ax.grid(axis="y", color="#e5e4e0", linewidth=0.6)
        # Com 2 nos: ate 2*nucleos, um processo por nucleo fisico; ate
        # 2*threads, HT; acima disso, mais processos que threads.
        nuc, thr = 2 * TOPOLOGIA.get("nucleos", 16), 2 * TOPOLOGIA.get("threads", 32)
        for inicio, fim, rotulo, cor in ((nuc, thr, "HT", "#f0efec"), (thr, None, "sobrecarga", "#e2e1dc")):
            acima = [p for p in ps if p > inicio and (fim is None or p <= fim)]
            if acima:
                esq = (inicio * min(acima)) ** 0.5  # meio do caminho em escala log
                dir_ = (fim * min(p for p in ps if p > fim)) ** 0.5 if fim and max(ps) > fim else max(ps) * 1.15
                ax.axvspan(esq, dir_, color=cor, zorder=0)
                ax.text((esq * dir_) ** 0.5, 0.02, rotulo, transform=ax.get_xaxis_transform(),
                        ha="center", va="bottom", color=TEXTO, fontsize=7)

    if forte:
        ps = sorted(forte)
        fig, ax = plt.subplots(figsize=(3.4, 2.5))
        ax.plot(ps, ps, "--", color=CINZA, linewidth=1, label="ideal (S = P)")
        s = [forte[p][0] for p in ps]
        erro = [[forte[p][0] - forte[p][1] for p in ps], [forte[p][2] - forte[p][0] for p in ps]]
        ax.errorbar(ps, s, yerr=erro, color=AZUL, marker="o", markersize=4, linewidth=2, capsize=2,
                    label="medido (mediana, min-max)")
        ax.set_yscale("log", base=2)
        ax.set_yticks([1] + ps)
        ax.set_yticklabels([str(p) for p in [1] + ps])
        ax.set_ylabel("speed-up S = T1/TP")
        eixo_p(ax, ps)
        ax.legend(loc="upper left")
        fig.tight_layout()
        for ext in ("pdf", "png"):
            fig.savefig(os.path.join(diretorio, f"speedup.{ext}"), dpi=200)
        plt.close(fig)

    if forte or fraca:
        ps = sorted(set(forte) | set(fraca))
        fig, ax = plt.subplots(figsize=(3.4, 2.7))
        ax.axhline(1, linestyle="--", color=CINZA, linewidth=1, label="ideal")
        if forte:
            ax.plot(sorted(forte), [forte[p][0] / p for p in sorted(forte)], color=AZUL, marker="o",
                    markersize=4, linewidth=2, label="forte (S/P)")
        if fraca:
            ax.plot(sorted(fraca), [fraca[p] for p in sorted(fraca)], color=LARANJA, marker="s",
                    markersize=4, linewidth=2, label="fraca (T1/TP)")
        # Eficiencia acima de 1 nao deve acontecer; se acontecer, fica visivel.
        maior = max([forte[p][0] / p for p in forte] + list(fraca.values()))
        ax.set_ylim(0, max(1.1, maior * 1.05))
        ax.set_ylabel("eficiencia")
        eixo_p(ax, ps)
        ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.22), ncol=3, handlelength=1.6)
        fig.tight_layout()
        for ext in ("pdf", "png"):
            fig.savefig(os.path.join(diretorio, f"eficiencia.{ext}"), dpi=200)
        plt.close(fig)


if __name__ == "__main__":
    main()
