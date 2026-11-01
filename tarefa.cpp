#include "tarefa.h"

#include <algorithm>
#include <cstdlib>
#include "NeuralNetwork.h"
#include "load_mnist.h"

static std::string dir_mnist() {
    const char* env = std::getenv("MNIST_DIR");
    std::string dir = env ? env : "MNIST";
    if (!dir.empty() && dir.back() != '/') dir += '/';
    return dir;
}

Dados carrega_dados(int amostras, int epocas) {
    const int TREINO_MNIST = 60000;
    Dados d;
    d.amostras = amostras;
    d.epocas = epocas;
    d.num_fatias = std::max(1, std::min(NUM_FATIAS, TREINO_MNIST / amostras));

    const std::string dir = dir_mnist();
    int n, linhas, colunas;
    const int max_treino = amostras * d.num_fatias;
    d.treino_x = load_mnist_images(dir + "train-images.idx3-ubyte", n, linhas, colunas, max_treino);
    VectorXd rotulos = load_mnist_labels(dir + "train-labels.idx1-ubyte", n, max_treino);
    d.treino_y = toOneHot(rotulos, 10);

    d.validacao_x = load_mnist_images(dir + "t10k-images.idx3-ubyte", n, linhas, colunas, NUM_VALIDACAO);
    d.validacao_rotulo = load_mnist_labels(dir + "t10k-labels.idx1-ubyte", n, NUM_VALIDACAO);
    return d;
}

// O indice percorre as 6 combinacoes de (ocultos, eta) e depois as fatias,
// entao qualquer bloco de tarefas consecutivas mistura tarefas de custo
// diferente (32 ocultos custa mais que 16), como numa busca de hiperparametros.
ConfigTarefa config_da_tarefa(int indice, int num_fatias) {
    static const int OCULTOS[] = {16, 32};
    static const double ETAS[] = {0.05, 0.1, 0.2};
    ConfigTarefa c;
    c.ocultos = OCULTOS[indice % 2];
    c.eta = ETAS[(indice / 2) % 3];
    c.fatia = (indice / 6) % num_fatias;
    c.semente = static_cast<unsigned>(indice);
    return c;
}

int executa_tarefa(int indice, const Dados& d) {
    const ConfigTarefa c = config_da_tarefa(indice, d.num_fatias);

    NeuralNetwork nn {{784, c.ocultos, c.ocultos, 10}, {"relu", "relu", "softmax"}, c.semente};
    const MatrixXd x = d.treino_x.middleRows(static_cast<Index>(c.fatia) * d.amostras, d.amostras);
    const MatrixXd y = d.treino_y.middleRows(static_cast<Index>(c.fatia) * d.amostras, d.amostras);
    nn.learn(x, y, d.epocas, c.eta, TAMANHO_LOTE, "cross_entropy_loss", false);

    const MatrixXd pred = nn.predict(d.validacao_x);
    int acertos = 0;
    for (Index i = 0; i < pred.rows(); ++i) {
        Index classe;
        pred.row(i).maxCoeff(&classe);
        if (classe == static_cast<Index>(d.validacao_rotulo(i))) ++acertos;
    }
    return acertos;
}

Checksum calcula_checksum(const std::vector<int>& acertos) {
    Checksum c {0, 0};
    for (std::size_t i = 0; i < acertos.size(); ++i) {
        c.soma += acertos[i];
        c.ordem += static_cast<long long>(i + 1) * acertos[i];
    }
    return c;
}
