#pragma once

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <Eigen/Dense>

using namespace Eigen;

//T2: functions are inline because this header is now included by more than one .cpp

inline int read_int(std::ifstream& stream) {
    unsigned char bytes[4];
    stream.read(reinterpret_cast<char*>(bytes), 4);
    return (int(bytes[0]) << 24) | (int(bytes[1]) << 16) | (int(bytes[2]) << 8) | int(bytes[3]);
}

//T2: max_images (< 0 means all) reads only the first images of the file.
//The 60k training images take 376 MB as doubles; 32 processes per node would need over 12 GB.
inline MatrixXd load_mnist_images(const std::string& filename, int& num_images, int& rows, int& cols,
                                  int max_images = -1) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Cannot open image file: " + filename);

    int magic = read_int(file);
    (void)magic;
    num_images = read_int(file);
    rows = read_int(file);
    cols = read_int(file);
    if (max_images >= 0 && max_images < num_images) num_images = max_images;

    MatrixXd images(num_images, rows * cols);

    for (int i = 0; i < num_images; ++i) {
        for (int j = 0; j < rows * cols; ++j) {
            unsigned char pixel = 0;
            file.read(reinterpret_cast<char*>(&pixel), 1);
            images(i, j) = static_cast<double>(pixel) / 255.0; // normalize
        }
    }

    return images;
}

inline VectorXd load_mnist_labels(const std::string& filename, int& num_labels, int max_labels = -1) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Cannot open label file: " + filename);

    int magic = read_int(file);
    (void)magic;
    num_labels = read_int(file);
    if (max_labels >= 0 && max_labels < num_labels) num_labels = max_labels;

    VectorXd labels(num_labels);

    for (int i = 0; i < num_labels; ++i) {
        unsigned char label = 0;
        file.read(reinterpret_cast<char*>(&label), 1);
        labels(i) = static_cast<int>(label);
    }

    return labels;
}

//one hot encode labels
inline MatrixXd toOneHot(const VectorXd& labels, int num_labels){
    MatrixXd one_hot {MatrixXd::Zero(labels.rows(), num_labels)};
    for (Eigen::Index i {}; i<labels.rows(); ++i){
        one_hot(i, static_cast<int>(labels(i))) = 1.0;
    }
    return one_hot;
}
