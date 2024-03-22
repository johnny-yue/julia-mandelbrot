#define OLC_PGE_APPLICATION

#include <complex>
#include <iostream>
#include <fstream>
#include <fmt/core.h>
#include "hip/hip_runtime.h"
#include "olcPixelGameEngine.h"

using std::complex;
using complex_d = std::complex<double>;

constexpr uint32_t MAX_ITERATION = 255;
constexpr uint32_t N_THREAD = 30;

int GPU_THREAD_N = 256;
int n_data;
bool GPU_CALC = true;

void save_to_csv(double *values, std::string name, int width, int height)
{
    auto filename = name + ".csv";
    std::ofstream csv(filename);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            csv << values[width * y + x] << ",";
        }
        csv << "\n";
    }
    csv << "\n";
    csv.close();
}

static inline int64_t rdsysns()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
}

complex_d sequence(complex_d z, complex_d c)
{
    return std::pow(z, 2) + c;
}

int total_power_count = 0;

uint32_t mandelbrot(complex_d c)
{
    double escape_radios = 2.0;
    complex_d n = c;
    uint32_t i = 0;
    for (; i < MAX_ITERATION; i++)
    {
        n = sequence(n, c);
        if (std::abs(n) > escape_radios)
        {
            break;
        }
    }
    total_power_count += i;
    return i;
}

uint32_t julia(complex_d x, complex_d c)
{
    double escape_radios = 2.0;
    uint32_t i = 0;
    for (; i < MAX_ITERATION; i++)
    {
        x = sequence(x, c);
        if (std::abs(x) > escape_radios)
        {
            break;
        }
    }

    return i;
}

__global__ void mandelbrot_gpu(double *cr, double *ci, int *bitmap, int NPIXEL)
{
    int id = blockDim.x * blockIdx.x + threadIdx.x;
    if (id < NPIXEL)
    {
        complex_d c{cr[id], ci[id]};
        int escape = 0;
        double escape_radios = 2.0;
        complex_d n = c;
        int i = 0;
        for (; i < MAX_ITERATION; i++)
        {
            n = std::pow(n, 2) + c;
            if (std::abs(n) > escape_radios)
            {
                break;
            }
        }
        bitmap[id] = i;
    }
}

void output_image(int **bitmap, int length, int height)
{
    std::ofstream image;
    image.open("image.bpm");
    image << "P2\n";
    image << length << " " << height << "\n";
    image << 20 << "\n";

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < length; x++)
        {
            image << (MAX_ITERATION - bitmap[x][y]) / 10 << " ";
        }
        image << "\n";
    }

    image.close();
}

class MandelbrotDisplay : public olc::PixelGameEngine
{
public:
    MandelbrotDisplay(int32_t height, int32_t width) : height(height), width(width)
    {
        sAppName = "Mandelbrot Display";
        NPIXEL = this->width * this->height;
        // hipFree(NULL);

        bitmap_size = width * height * sizeof(int);
        cmap_size = width * height * sizeof(double);

        bitmapMandelbrot = (int *)malloc(bitmap_size);
        bitmapJulia = (int *)malloc(bitmap_size);

        cmap_i_host = (double *)malloc(cmap_size);
        cmap_r_host = (double *)malloc(cmap_size);

        auto result = hipMalloc(&cmap_i_device, cmap_size);
        result = hipMalloc(&cmap_r_device, cmap_size);

        result = hipMalloc(&mandelbrot_result_gpu, bitmap_size);

        gen_image_mandelbrot(bitmapMandelbrot, width, height, zoom);
        gen_image_julia(bitmapJulia, width, height, 0, 0);
        Construct(width * 2, height, 1, 1);
    }

private:
    bool OnUserCreate() override
    {
        // Called once at the start, so create things here
        return true;
    }

    bool OnUserUpdate(float fElapsedTime) override
    {
        double new_zoom = zoom;
        int32_t mouse_x = GetMouseX();
        int32_t mouse_y = GetMouseY();

        if (GetMouseWheel() > 0)
        {
            std::cout << "mouse up\n";
            new_zoom = new_zoom * 1.25;
        }
        else if (GetMouseWheel() < 0)
        {
            std::cout << "mouse down\n";
            new_zoom = new_zoom * 0.8;
            if (new_zoom < 1.0)
            {
                new_zoom = 1;
            }
        }
        if (new_zoom != zoom)
        {

            fmt::print("mouse position {} {}\n", mouse_x, mouse_y);
            fmt::print("zoom change from {} to {}\n", zoom, new_zoom);

            double px = double(mouse_x) / width - 0.5;
            double py = double(mouse_y) / height - 0.5;

            double old_distance_x = range / zoom * px;
            double new_distance_x = old_distance_x * zoom / new_zoom;

            double old_distance_y = range / zoom * py;
            double new_distance_y = old_distance_y * zoom / new_zoom;

            double add_shift_x = old_distance_x - new_distance_x;
            double add_shift_y = old_distance_y - new_distance_y;

            shift_x += add_shift_x;
            shift_y += add_shift_y;

            int center_x = width / 2;
            int center_y = height / 2;

            double step = range / width / new_zoom;

            for (int x = 0; x < width; x++)
            {
                double x_d = (x - center_x) * step + shift_x;
                for (int y = 0; y < height; y++)
                {
                    double y_d = (y - center_y) * step + shift_y;
                    cmap_r_host[width * y + x] = x_d;
                    cmap_i_host[width * y + x] = y_d;
                }
            }

            if (GPU_CALC)
            {
                // construct CMAP
                fmt::print("cpu draw, step{} \n", step);
                auto result = hipMemcpy(cmap_i_device, cmap_i_host, cmap_size, hipMemcpyHostToDevice);
                result = hipMemcpy(cmap_r_device, cmap_r_host, cmap_size, hipMemcpyHostToDevice);

                int thread_n = 256;
                int block_n = (NPIXEL + 256 - 1) / 256;

                hipLaunchKernelGGL(mandelbrot_gpu, block_n, thread_n, 0, 0, cmap_r_device, cmap_i_device, mandelbrot_result_gpu, NPIXEL);

                result = hipMemcpy(bitmapMandelbrot, mandelbrot_result_gpu, bitmap_size, hipMemcpyDeviceToHost);
            }
            else
            {
                // use the old bitmap to do interpolation
                // while calculating the new bitmap
                // gen_image_mandelbrot(bitmapMandelbrot, width, height, new_zoom);
                fmt::print("cpu draw, step{} \n", step);
                for (int i = 0; i < NPIXEL; i++)
                {
                    complex_d c{cmap_r_host[i], cmap_i_host[i]};
                    uint32_t r = mandelbrot(c);
                    bitmapMandelbrot[i] = r;
                }
            }

            zoom = new_zoom;
            should_draw = true;
            // DrawString(30, 30, std::to_string(new_zoom));
        }

        if (mouse_x != mouse_x_old || mouse_y != mouse_y_old)
        {
            gen_image_julia(bitmapJulia, width, height, mouse_x, mouse_y);
            // fmt::print("redraw julia");
            for (int x = 0; x < width; x++)
            {
                for (int y = 0; y < height; y++)
                {
                    int value = bitmapJulia[width * y + x];
                    Draw(x + width, y, olc::Pixel(value, value, value));
                }
            }
        }

        // called once per frame
        if (should_draw)
        {

            std::cout << "redraw with zoom:" << zoom << "\n";
            for (int x = 0; x < width; x++)
            {
                for (int y = 0; y < height; y++)
                {
                    int value = bitmapMandelbrot[width * y + x];
                    Draw(x, y, olc::Pixel(value, value, value));
                }
            }

            should_draw = false;
        }

        // for (int i = 0; i < 100; i++)
        // {
        //     Draw(i, 50, olc::Pixel(50, 150, 100));
        // }

        return true;
    }

    void gen_image_julia(int *bitmap, int width, int height, int x, int y)
    {
        // compute c with regard to mandelbrot set
        int center_x = width / 2;
        int center_y = height / 2;

        double step = range / width / zoom;
        double c_x = (x - center_x) * step + shift_x;
        double c_y = (y - center_y) * step + shift_y;
        complex_d c{c_x, c_y};

        // reset step for julia generation
        step = range / width;

        for (int x = 0; x < width; x++)
        {
            double x_d = (x - center_x) * step;
            for (int y = 0; y < height; y++)
            {
                double y_d = (y - center_y) * step;
                complex_d v{x_d, y_d};
                uint32_t r = julia(v, c);
                bitmap[width * y + x] = r;
            }
        }
    }

    void gen_image_mandelbrot(int *bitmap, int length, int height, double zoom)
    {
        total_power_count = 0;
        auto start = rdsysns();

        int center_x = length / 2;
        int center_y = height / 2;

        double step = range / length / zoom;

        memset(cmap_r_host, 0, length * height * sizeof(double));
        for (int x = 0; x < length; x++)
        {
            double x_d = (x - center_x) * step + shift_x;
            for (int y = 0; y < height; y++)
            {
                double y_d = (y - center_y) * step + shift_y;
                cmap_r_host[length * y + x] = x_d;
                cmap_i_host[length * y + x] = y_d;
                if (x == 0 || x == 1)
                {
                    fmt::print("x:{} y:{} i:{} v:{}\n", x, y, x * y + x, x_d);
                }
            }
        }

        // save_to_csv(cmap_r_host, "cmap_r_host", length, height);

        for (int x = 0; x < length; x++)
        {
            // double x_d = (x - center_x) * step + shift_x;
            for (int y = 0; y < height; y++)
            {
                // double y_d = (y - center_y) * step + shift_y;
                double x_d = cmap_r_host[length * y + x];
                double y_d = cmap_i_host[length * y + x];
                complex_d v{x_d, y_d};
                uint32_t r = mandelbrot(v);
                bitmap[length * y + x] = r;
            }
        }

        auto end = rdsysns();

        should_draw = true;
        auto total_ns = end - start;
        fmt::print("gen_image_mandelbrot, zoom {}, elapsed {}, total_power {}, ns_per_power {}\n", zoom, total_ns, total_power_count, total_ns / total_power_count);
    }

    bool should_draw = true;
    int *bitmapMandelbrot;
    int *bitmapJulia;
    int *mandelbrot_result_gpu;

    int cmap_size;
    int bitmap_size;

    double *cmap_i_host;
    double *cmap_r_host;
    double *cmap_i_device;
    double *cmap_r_device;

    int32_t width;
    int32_t height;
    int NPIXEL;
    double zoom = 1.0;
    double range = 3.0;
    double shift_x = -0.8;
    double shift_y = 0.0;
    int mouse_x_old = 0;
    int mouse_y_old = 0;
};

int main()
{

    // The following line is used to compile the sample for the game engine.
    // g++ olcExampleProgram.cpp -lpng -lGL -lX11
    MandelbrotDisplay m(300, 300);

    m.Start();

    return 0;
}