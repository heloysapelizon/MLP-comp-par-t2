# Mesmas flags na sequencial e na paralela (regra 4.2 do enunciado).
# Sem -fopenmp: o Eigen fica com uma thread por processo.
# Sem -march=native: os nos poderiam gerar codigo de ponto flutuante diferente
# e o checksum deixaria de bater entre execucoes.
CXX      ?= g++
MPICXX   ?= mpicxx
EIGEN    ?= eigen
CXXFLAGS  = -std=c++17 -O3 -Wall -DNDEBUG
INCLUDES  = -I$(EIGEN)

COMUM = tarefa.cpp NeuralNetwork.cpp
HEADERS = tarefa.h NeuralNetwork.h load_mnist.h

all: sequencial mestre_escravo pingpong

# A sequencial nao usa MPI, mas e compilada pelo mesmo mpicxx: assim as duas
# versoes saem do mesmo compilador, com as mesmas flags, e o checksum bate.
sequencial: sequencial.cpp $(COMUM) $(HEADERS)
	$(MPICXX) $(CXXFLAGS) $(INCLUDES) sequencial.cpp $(COMUM) -o $@

mestre_escravo: mestre_escravo.cpp $(COMUM) $(HEADERS)
	$(MPICXX) $(CXXFLAGS) $(INCLUDES) mestre_escravo.cpp $(COMUM) -o $@

pingpong: pingpong.cpp
	$(MPICXX) $(CXXFLAGS) pingpong.cpp -o $@

# Programa original do MLP-From-Scratch (treino completo no MNIST)
demo: main.cpp NeuralNetwork.cpp $(HEADERS) loadcsv.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp NeuralNetwork.cpp -o $@

clean:
	rm -f sequencial mestre_escravo pingpong demo

.PHONY: all clean
