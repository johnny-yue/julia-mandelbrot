#include <complex>
#include <iostream>
#include <fstream>

using std::complex;
using complex_d = std::complex<double>;

const uint32_t MAX_ITERATION = 200;

complex_d sequence(complex_d z, complex_d c){
    return std::pow(z, 2) + c;
}

uint32_t mandelbrot(complex_d c) {
    double escape_radios = 2.0;
    complex_d n = c;
    uint32_t i = 0;
    for(;i<MAX_ITERATION;i++){
        n = sequence(n, c);
        if (std::abs(n) > escape_radios) {
            break;
        }
    }
    return i;
}

void gen_image(int** bitmap, int length, int height) {
    int center_x = length / 2;
    int center_y = height / 2;
    double x_shift = -0.8;
    double step = 3.3 / length;
    // assuming 2.0 x 2.0 range
    for(int x=0;x<length;x++) {
        for (int y=0;y<height;y++) {
            double x_d = (x - center_x) * step + x_shift;
            double y_d = (y - center_y) * step;
            complex_d v {x_d, y_d};
            uint32_t r = mandelbrot(v);
            bitmap[x][y] = r;
        }
    }
}

void output_image(int** bitmap, int length, int height) {
    std::ofstream image;
    image.open("image.bpm");
    image << "P2\n";
    image << length << " " << height << "\n";
    image << 20 << "\n";

    for(int y=0;y<height;y++) {
        for (int x=0;x<length;x++) {
            image << (MAX_ITERATION-bitmap[x][y]) / 10 << " ";
        }
        image << "\n";
    }

    image.close();
}

int main() {

    int length = 3000;
    int height = 2400;

    int** bitmap;
    bitmap = new int*[length];
    for(int i=0;i<length;i++) {
        bitmap[i] = new int[height];
    }

    gen_image(bitmap, length, height);
    output_image(bitmap, length, height);

    return 0;
}