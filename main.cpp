#define OLC_PGE_APPLICATION

#include <complex>
#include <iostream>
#include <fstream>
#include <fmt/core.h>
#include "olcPixelGameEngine.h"

using std::complex;
using complex_d = std::complex<double>;

constexpr uint32_t MAX_ITERATION = 255;
constexpr uint32_t N_THREAD = 30;

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

        bitmapMandelbrot = new int *[width];
        bitmapJulia = new int *[width];
        for (int i = 0; i < width; i++)
        {
            bitmapMandelbrot[i] = new int[height];
            bitmapJulia[i] = new int[height];
        }
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

            // use the old bitmap to do interpolation
            // while calculating the new bitmap
            gen_image_mandelbrot(bitmapMandelbrot, width, height, new_zoom);
            zoom = new_zoom;

            DrawString(30, 30, std::to_string(new_zoom));
        }
        if (mouse_x != mouse_x_old || mouse_y != mouse_y_old)
        {
            gen_image_julia(bitmapJulia, width, height, mouse_x, mouse_y);
            // fmt::print("redraw julia");
            for (int x = 0; x < width; x++)
            {
                for (int y = 0; y < height; y++)
                {
                    int value = bitmapJulia[x][y];
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
                    int value = bitmapMandelbrot[x][y];
                    Draw(x, y, olc::Pixel(value, value, value));
                }
            }
            should_draw = false;
        }

        return true;
    }

    void gen_image_julia(int **bitmap, int length, int height, int x, int y)
    {
        // compute c with regard to mandelbrot set
        int center_x = length / 2;
        int center_y = height / 2;

        double step = range / length / zoom;
        double c_x = (x - center_x) * step + shift_x;
        double c_y = (y - center_y) * step + shift_y;
        complex_d c{c_x, c_y};

        // reset step for julia generation
        step = range / length;

        for (int x = 0; x < length; x++)
        {
            double x_d = (x - center_x) * step;
            for (int y = 0; y < height; y++)
            {
                double y_d = (y - center_y) * step;
                complex_d v{x_d, y_d};
                uint32_t r = julia(v, c);
                bitmap[x][y] = r;
            }
        }
    }

    void gen_image_mandelbrot(int **bitmap, int length, int height, double zoom)
    {
        total_power_count = 0;
        auto start = rdsysns();

        int center_x = length / 2;
        int center_y = height / 2;

        double step = range / length / zoom;

        for (int x = 0; x < length; x++)
        {
            double x_d = (x - center_x) * step + shift_x;
            for (int y = 0; y < height; y++)
            {
                double y_d = (y - center_y) * step + shift_y;
                complex_d v{x_d, y_d};
                uint32_t r = mandelbrot(v);
                bitmap[x][y] = r;
            }
        }

        auto end = rdsysns();

        should_draw = true;
        auto total_ns = end - start;
        fmt::print("gen_image_mandelbrot, zoom {}, elapsed {}, total_power {}, ns_per_power {}\n", zoom, total_ns, total_power_count, total_ns / total_power_count);
    }

    bool should_draw = true;
    int **bitmapMandelbrot;
    int **bitmapJulia;
    int32_t width;
    int32_t height;
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