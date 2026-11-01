// Unidade de trabalho do T2, compartilhada pela versao sequencial e pela MPI.
//
// Uma tarefa (a "unidade de trabalho" do enunciado) = treinar uma MLP pequena
// com a configuracao derivada do seu indice e contar os acertos num conjunto
// fixo de validacao. As tarefas sao independentes entre si e a mensagem que
// descreve um bloco de tarefas e so {inicio, qtd}, porque cada processo
// carrega o MNIST antes da regiao medida.
//
// Tamanho da tarefa: o enunciado pede comparar a granularidade escolhida com
// uma bem menor e uma bem maior. Com a tarefa anterior (100 imagens, 2 epocas,
// validacao em 200: 10,5 ms na Atlantica) a ida e volta entre nos (~190 us)
// pesava 0,6% e g = 1 ja era o melhor, sem granularidade menor para comparar
// (resultados em resultados/v1_tarefa_10ms). A tarefa foi reduzida ao minimo
// que ainda aprende algo (20 imagens, 1 epoca, validacao em 20; ~14% de acerto
// contra 10% do acaso) para que g = 1 pague de verdade pelas mensagens.
#pragma once

#include <string>
#include <vector>
#include <Eigen/Dense>

// Quantas fatias disjuntas do conjunto de treino as tarefas usam (no maximo).
// Limita a memoria por processo: amostras * NUM_FATIAS imagens de 784 doubles
// (20 * 500 = 10 mil imagens, 63 MB).
constexpr int NUM_FATIAS = 500;
// Imagens de validacao (as primeiras do t10k), iguais para todas as tarefas.
// Pequeno para a validacao nao dominar o custo de uma tarefa curta.
constexpr int NUM_VALIDACAO = 20;
// Lote pequeno: com poucas imagens, mais atualizacoes por epoca ajudam a rede
// a aprender alguma coisa.
constexpr int TAMANHO_LOTE = 8;

struct Dados {
    Eigen::MatrixXd treino_x;        // (amostras * num_fatias) x 784
    Eigen::MatrixXd treino_y;        // one-hot, (amostras * num_fatias) x 10
    Eigen::MatrixXd validacao_x;     // NUM_VALIDACAO x 784
    Eigen::VectorXd validacao_rotulo;
    int amostras = 0;                // imagens de treino por tarefa
    int epocas = 0;
    int num_fatias = 0;
};

struct ConfigTarefa {
    int ocultos;        // neuronios em cada uma das 2 camadas ocultas
    double eta;         // taxa de aprendizado
    int fatia;          // qual fatia do treino a tarefa usa
    unsigned semente;   // pesos iniciais e embaralhamento
};

// Le o MNIST de dir_mnist (padrao "MNIST/", ou a variavel de ambiente MNIST_DIR).
Dados carrega_dados(int amostras, int epocas);

ConfigTarefa config_da_tarefa(int indice, int num_fatias);

// Treina a rede da tarefa `indice` e devolve os acertos em NUM_VALIDACAO imagens.
int executa_tarefa(int indice, const Dados& dados);

// soma  = soma dos acertos de todas as tarefas
// ordem = soma de (i+1) * acertos[i]: so coincide com a sequencial se cada
//         resultado voltou para a sua posicao de origem no saco.
struct Checksum {
    long long soma;
    long long ordem;
};
Checksum calcula_checksum(const std::vector<int>& acertos);
