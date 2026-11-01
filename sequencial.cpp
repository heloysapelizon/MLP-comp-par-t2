// Versao sequencial de referencia (T1 do T2): executa todas as tarefas em
// ordem, sem MPI. Mede so o laco das tarefas; a leitura do MNIST fica de fora,
// com os mesmos limites da versao paralela.
//
// Uso: ./sequencial [tarefas] [amostras] [epocas] [passo]
//   passo > 0: imprime tambem o checksum dos prefixos passo, 2*passo, 4*passo...
//   (o checksum de N tarefas e o do prefixo de tamanho N de uma execucao maior,
//   entao uma so execucao da a referencia de todos os tamanhos da fraca)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "tarefa.h"

int main(int argc, char** argv) {
    const int num_tarefas = argc > 1 ? std::atoi(argv[1]) : 4000;
    const int amostras    = argc > 2 ? std::atoi(argv[2]) : 100;
    const int epocas      = argc > 3 ? std::atoi(argv[3]) : 2;
    const int passo       = argc > 4 ? std::atoi(argv[4]) : 0;
    if (num_tarefas < 0 || amostras < 1 || amostras > 60000 || epocas < 1 || passo < 0) {
        std::fprintf(stderr, "uso: %s [tarefas] [amostras <= 60000] [epocas] [passo]\n", argv[0]);
        return 1;
    }

    const Dados dados = carrega_dados(amostras, epocas);
    std::vector<int> acertos(num_tarefas);

    // ---- regiao medida ----
    const auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < num_tarefas; ++i) acertos[i] = executa_tarefa(i, dados);
    const auto t2 = std::chrono::steady_clock::now();
    // -----------------------

    const double tempo = std::chrono::duration<double>(t2 - t1).count();
    const Checksum ck = calcula_checksum(acertos);
    std::printf("tarefas=%d amostras=%d epocas=%d (amostras treinadas = %lld)\n",
                num_tarefas, amostras, epocas, 1LL * num_tarefas * amostras * epocas);
    std::printf("tempo=%.6f s  tempo_por_tarefa=%.6f s\n", tempo,
                num_tarefas ? tempo / num_tarefas : 0.0);
    std::printf("checksum soma=%lld ordem=%lld\n", ck.soma, ck.ordem);
    // CSV,versao,P,tarefas,g,amostras,epocas,tempo_s,soma,ordem
    std::printf("CSV,sequencial,1,%d,0,%d,%d,%.6f,%lld,%lld\n",
                num_tarefas, amostras, epocas, tempo, ck.soma, ck.ordem);
    for (int n = passo; passo > 0 && n < num_tarefas; n *= 2) {
        const Checksum p = calcula_checksum(std::vector<int>(acertos.begin(), acertos.begin() + n));
        std::printf("CSV,sequencial_prefixo,1,%d,0,%d,%d,0,%lld,%lld\n", n, amostras, epocas, p.soma, p.ordem);
    }
    return 0;
}
