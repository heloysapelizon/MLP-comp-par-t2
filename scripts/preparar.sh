#!/bin/bash
# Baixa as dependencias que nao vao para o git: Eigen 3.4.0 (so cabecalhos)
# e os 4 arquivos do MNIST, com os nomes que load_mnist.h espera.
# Uso (na raiz do repositorio): bash scripts/preparar.sh
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -d eigen ]; then
    git clone --depth 1 --branch 3.4.0 https://gitlab.com/libeigen/eigen.git eigen
fi

# Espelho do MNIST mantido pelo PyTorch (o site original costuma ficar fora do ar)
URL=https://ossci-datasets.s3.amazonaws.com/mnist
mkdir -p MNIST
for nome in train-images train-labels t10k-images t10k-labels; do
    case $nome in *images) tipo=idx3 ;; *) tipo=idx1 ;; esac
    destino="MNIST/$nome.$tipo-ubyte"
    if [ ! -f "$destino" ]; then
        curl -fL "$URL/$nome-$tipo-ubyte.gz" | gunzip > "$destino"
    fi
done
ls -l MNIST
