#define OLC_PGE_APPLICATION

#include <complex>
#include <iostream>
#include <fstream>
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

        bitmap = new int *[width];
        for (int i = 0; i < width; i++)
        {
            bitmap[i] = new int[height];
        }
        gen_image(bitmap, width, height, zoom);
        Construct(height, width, 1, 1);
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
        if (GetMouseWheel() > 0)
        {
            std::cout << "mouse up\n";
            new_zoom = new_zoom * 1.3;
        }
        else if (GetMouseWheel() < 0)
        {
            std::cout << "mouse down\n";
            new_zoom = new_zoom * 0.7;
            if (new_zoom < 1.0)
            {
                new_zoom = 1;
            }
        }
        if (new_zoom != zoom)
        {
            std::cout << "zoom change from " << zoom << " to " << new_zoom << "\n";

            // use the old bitmap to do interpolation
            // while calculating the new bitmap
            gen_image(bitmap, width, height, new_zoom);
            zoom = new_zoom;

            DrawString(30, 30, std::to_string(new_zoom));
        }

        // called once per frame
        if (should_draw)
        {
            std::cout << "redraw with zoom:" << zoom << "\n";
            for (int x = 0; x < ScreenWidth(); x++)
            {
                for (int y = 0; y < ScreenHeight(); y++)
                {
                    int value = bitmap[x][y];
                    Draw(x, y, olc::Pixel(value, value, value));
                }
            }
            should_draw = false;
        }

        return true;
    }

    void gen_image(int **bitmap, int length, int height, double zoom)
    {
        total_power_count = 0;
        auto start = rdsysns();
        std::cout << "gen_image with zoom:" << zoom << " " << length << " " << height << "\n";

        int center_x = length / 2;
        int center_y = height / 2;
        double x_shift = -0.8;
        double step = 3.3 / length / zoom;
        // assuming 2.0 x 2.0 range
        for (int x = 0; x < length; x++)
        {
            double x_d = (x - center_x) * step + x_shift;
            for (int y = 0; y < height; y++)
            {
                double y_d = (y - center_y) * step;
                complex_d v{x_d, y_d};
                uint32_t r = mandelbrot(v);
                bitmap[x][y] = r;
            }
        }

        auto end = rdsysns();

        should_draw = true;
        auto total_ns = end - start;
        std::cout << "gen_image completed, elapsed:" << total_ns / 100000
                  << " total_power:" << total_power_count
                  << " per power:" << total_ns / total_power_count << "\n ";
    }

    bool should_draw = true;
    int **bitmap;
    int32_t width;
    int32_t height;
    double zoom = 1.0;
};

int main()
{

    // The following line is used to compile the sample for the game engine.
    // g++ olcExampleProgram.cpp -lpng -lGL -lX11
    MandelbrotDisplay m(800, 800);

    m.Start();

    return 0;
}