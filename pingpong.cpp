// Ida e volta entre os ranks 0 e 1 com os mesmos tamanhos de mensagem do
// mestre/escravo: int[2] na ida (TAG_TAREFA) e int[2+g] na volta
// (TAG_RESULTADO). Item (a) da analise de granularidade: compara o custo de
// comunicacao de um bloco com o custo de processa-lo.
//
// Uso: srun -n 2 ./pingpong g1 [g2 ...]    (ex.: ./pingpong 1 4 16 64)
#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);
    if (P < 2 || argc < 2) {
        if (rank == 0) std::fprintf(stderr, "uso: srun -n 2 ./pingpong g1 [g2 ...]\n");
        MPI_Finalize();
        return 1;
    }

    const int AQUECIMENTO = 100, REPETICOES = 10000;
    char host[MPI_MAX_PROCESSOR_NAME] = {0}, host1[MPI_MAX_PROCESSOR_NAME] = {0};
    int len;
    MPI_Get_processor_name(host, &len);
    if (rank == 1) MPI_Send(host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
    if (rank == 0) MPI_Recv(host1, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int a = 1; a < argc; ++a) {
        const int g = std::atoi(argv[a]);
        int ida[2] = {0, g};
        std::vector<int> volta(2 + g, 0);
        double t = 0;
        for (int i = 0; i < AQUECIMENTO + REPETICOES; ++i) {
            if (i == AQUECIMENTO) {
                MPI_Barrier(MPI_COMM_WORLD);
                t = MPI_Wtime();
            }
            if (rank == 0) {
                MPI_Send(ida, 2, MPI_INT, 1, 2, MPI_COMM_WORLD);
                MPI_Recv(volta.data(), 2 + g, MPI_INT, 1, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else if (rank == 1) {
                MPI_Recv(ida, 2, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(volta.data(), 2 + g, MPI_INT, 0, 3, MPI_COMM_WORLD);
            }
        }
        if (rank == 0) {
            const double rtt = (MPI_Wtime() - t) / REPETICOES;
            // PINGPONG,g,bytes_ida,bytes_volta,host0,host1,ida_e_volta_us
            std::printf("PINGPONG,%d,%zu,%zu,%s,%s,%.3f\n", g, 2 * sizeof(int), (2 + g) * sizeof(int), host,
                        host1, rtt * 1e6);
        }
    }

    MPI_Finalize();
    return 0;
}
