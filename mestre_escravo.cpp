// Versao paralela mestre/escravo do T2 (MPI).
//
// O mestre (rank 0) guarda o saco: as tarefas 0..N-1, entregues em blocos de
// g tarefas consecutivas. Os escravos (ranks 1..P-1) pedem trabalho, executam
// executa_tarefa() e devolvem os acertos; cada resultado vale como o pedido
// do proximo bloco.
//
// Uso: srun -n P ./mestre_escravo g [tarefas] [amostras] [epocas]
//   g = granularidade: tarefas por mensagem (requisito 6)
#include <mpi.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "tarefa.h"

// Tags: e pela tag que cada lado sabe o que a mensagem significa.
enum Tag {
    TAG_PEDIDO    = 1, // escravo -> mestre, vazia: primeiro pedido de trabalho (requisito 1)
    TAG_TAREFA    = 2, // mestre -> escravo, int[2] = {inicio, qtd}: processe inicio..inicio+qtd-1
    TAG_RESULTADO = 3, // escravo -> mestre, int[2+qtd] = {inicio, qtd, acertos...}; vale como novo pedido
    TAG_FIM       = 4, // mestre -> escravo, vazia: saco vazio, encerre (requisito 4)
};

// Tempos que cada escravo mede e manda ao mestre no fim (MPI_Gather). A
// contagem de tarefas e mensagens por escravo e feita pelo proprio mestre.
enum Estat { E_OCUPADO, E_FIM, NUM_ESTAT };

// Distribui o saco ate todos os escravos receberem TAG_FIM. Devolve o instante
// em que chegou o ultimo resultado. tarefas_por[r] e mensagens_por[r] contam o
// que o escravo r devolveu (como no Lab 3, parte 2, item 3).
static double mestre(int num_escravos, int num_tarefas, int g, std::vector<int>& acertos,
                     std::vector<int>& tarefas_por, std::vector<int>& mensagens_por) {
    // Maior mensagem possivel: um resultado com g acertos.
    std::vector<int> buf(2 + g);
    int proxima = 0;          // primeira tarefa ainda nao entregue
    int recebidas = 0;
    int escravos_ativos = num_escravos;
    double t_ultimo = MPI_Wtime(); // vale para num_tarefas == 0

    while (escravos_ativos > 0) {
        MPI_Status st;
        // Ordem de chegada (requisito 2): atende quem ficar pronto primeiro,
        // seja um pedido inicial ou um resultado.
        MPI_Recv(buf.data(), 2 + g, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);

        if (st.MPI_TAG == TAG_RESULTADO) {
            const int inicio = buf[0], qtd = buf[1];
            // Ordem do saco (requisito 3): o resultado volta para a posicao de origem.
            std::copy(buf.begin() + 2, buf.begin() + 2 + qtd, acertos.begin() + inicio);
            recebidas += qtd;
            tarefas_por[st.MPI_SOURCE] += qtd;
            mensagens_por[st.MPI_SOURCE] += 1;
            if (recebidas == num_tarefas) t_ultimo = MPI_Wtime();
        }

        // TAG_PEDIDO ou TAG_RESULTADO: nos dois casos o escravo quer mais trabalho.
        if (proxima < num_tarefas) {
            // Requisito 5: o ultimo bloco leva so o que sobrou, sem exigir que
            // num_tarefas seja multiplo de g ou do numero de escravos.
            int tarefa[2] = {proxima, std::min(g, num_tarefas - proxima)};
            MPI_Send(tarefa, 2, MPI_INT, st.MPI_SOURCE, TAG_TAREFA, MPI_COMM_WORLD);
            proxima += tarefa[1];
        } else {
            MPI_Send(nullptr, 0, MPI_INT, st.MPI_SOURCE, TAG_FIM, MPI_COMM_WORLD);
            --escravos_ativos;
        }
    }
    return t_ultimo;
}

static void escravo(const Dados& dados, int g, double t_inicio, double estat[NUM_ESTAT]) {
    std::vector<int> resultado(2 + g);
    // Requisito 1: a iniciativa e do escravo.
    MPI_Send(nullptr, 0, MPI_INT, 0, TAG_PEDIDO, MPI_COMM_WORLD);

    while (true) {
        int tarefa[2];
        MPI_Status st;
        MPI_Recv(tarefa, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
        if (st.MPI_TAG == TAG_FIM) break;

        const int inicio = tarefa[0], qtd = tarefa[1];
        const double a = MPI_Wtime();
        for (int k = 0; k < qtd; ++k) resultado[2 + k] = executa_tarefa(inicio + k, dados);
        estat[E_OCUPADO] += MPI_Wtime() - a;

        resultado[0] = inicio;
        resultado[1] = qtd;
        MPI_Send(resultado.data(), 2 + qtd, MPI_INT, 0, TAG_RESULTADO, MPI_COMM_WORLD);
        // Instante (relativo ao inicio da regiao medida) do ultimo resultado
        // enviado: mostra quem fica ocioso no fim do saco.
        estat[E_FIM] = MPI_Wtime() - t_inicio;
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    const int g           = argc > 1 ? std::atoi(argv[1]) : 1;
    const int num_tarefas = argc > 2 ? std::atoi(argv[2]) : 4000;
    const int amostras    = argc > 3 ? std::atoi(argv[3]) : 100;
    const int epocas      = argc > 4 ? std::atoi(argv[4]) : 2;
    if (P < 2 || g < 1 || num_tarefas < 0 || amostras < 1 || amostras > 60000 || epocas < 1) {
        if (rank == 0)
            std::fprintf(stderr, "uso: srun -n P ./mestre_escravo g [tarefas] [amostras] [epocas]  (P >= 2, g >= 1)\n");
        MPI_Finalize();
        return 1;
    }

    // Leitura da entrada fora da regiao medida. O mestre nao treina, entao nao
    // precisa do MNIST: so os escravos carregam.
    Dados dados;
    if (rank != 0) dados = carrega_dados(amostras, epocas);

    double estat[NUM_ESTAT] = {0, 0};
    std::vector<int> acertos(rank == 0 ? num_tarefas : 0);
    std::vector<int> tarefas_por(rank == 0 ? P : 0), mensagens_por(rank == 0 ? P : 0);

    // Todos com os dados carregados antes de comecar a medir.
    MPI_Barrier(MPI_COMM_WORLD);
    const double t_inicio = MPI_Wtime();
    double tempo = 0;
    if (rank == 0) {
        tempo = mestre(P - 1, num_tarefas, g, acertos, tarefas_por, mensagens_por) - t_inicio;
    } else {
        escravo(dados, g, t_inicio, estat);
    }

    // Fora da regiao medida: estatisticas por escravo e o no de cada rank.
    std::vector<double> todas(rank == 0 ? NUM_ESTAT * P : 0);
    MPI_Gather(estat, NUM_ESTAT, MPI_DOUBLE, todas.data(), NUM_ESTAT, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    char host[MPI_MAX_PROCESSOR_NAME] = {0};
    int len;
    MPI_Get_processor_name(host, &len);
    std::vector<char> hosts(rank == 0 ? MPI_MAX_PROCESSOR_NAME * P : 0);
    MPI_Gather(host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, hosts.data(), MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0,
               MPI_COMM_WORLD);

    if (rank == 0) {
        const Checksum ck = calcula_checksum(acertos);
        std::printf("P=%d tarefas=%d g=%d amostras=%d epocas=%d (amostras treinadas = %lld)\n", P,
                    num_tarefas, g, amostras, epocas, 1LL * num_tarefas * amostras * epocas);
        std::printf("tempo=%.6f s\n", tempo);
        std::printf("checksum soma=%lld ordem=%lld\n", ck.soma, ck.ordem);
        // CSV,versao,P,tarefas,g,amostras,epocas,tempo_s,soma,ordem
        std::printf("CSV,mestre_escravo,%d,%d,%d,%d,%d,%.6f,%lld,%lld\n", P, num_tarefas, g, amostras,
                    epocas, tempo, ck.soma, ck.ordem);
        std::printf("MESTRE,%s\n", hosts.data());
        // ESCRAVO,P,tarefas,g,rank,host,tarefas_proc,mensagens,ocupado_s,ultimo_resultado_s
        for (int r = 1; r < P; ++r) {
            const double* e = &todas[NUM_ESTAT * r];
            std::printf("ESCRAVO,%d,%d,%d,%d,%s,%d,%d,%.6f,%.6f\n", P, num_tarefas, g, r,
                        &hosts[MPI_MAX_PROCESSOR_NAME * r], tarefas_por[r], mensagens_por[r],
                        e[E_OCUPADO], e[E_FIM]);
        }
    }

    MPI_Finalize();
    return 0;
}
