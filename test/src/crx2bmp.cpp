#include <cstdio>
#include <cstring>
#include <fstream>
#include <image_crx.hpp>

namespace crx2bmp 
{

    extern "C" auto main(const int argc, const char const* argv[]) -> int
    {
        if (argc < 2)
        {
            std::fprintf
            (   
                stderr,
                "Usage: crx2bmp <input.crx> [output.bmp]\n"
                "Convert Circus CRX image to BMP.\n"
                "If output.bmp is omitted, replaces .crx extension with .bmp\n"
            );
            return 1;
        }

        const char*  input_path{ argv[1] };
        std::string output_path{};

        if (argc >= 3) 
        {
            output_path.assign(argv[2]);
        }
        else 
        {
            output_path.assign(input_path);
            size_t dot{ output_path.rfind('.') };
            if (dot != std::string::npos)
            {
                output_path.resize(dot);
            }
            output_path.append(".bmp");
        }

        std::ifstream file{ input_path, std::ios::binary };
        if (!file.is_open())
        {
            std::fprintf(stderr, "Error: failed to open CRX file '%s'\n", input_path);
            return 1;
        }

        file.seekg(0, std::ios::end);
        const std::streampos file_size{ file.tellg() };
        if (file_size <= 0)
        {
            std::fprintf(stderr, "Error: empty CRX file '%s'\n", input_path);
            return 1;
        }
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer{};
        buffer.resize(static_cast<size_t>(file_size));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size))
        {
            std::fprintf(stderr, "Error: failed to read CRX file '%s'\n", input_path);
            return 1;
        };

        #if 1
        adv::crx::image crx{ buffer };

        if (!crx.is_parsed())
        {
            std::fprintf(stderr, "Error: failed to parse CRX file '%s'\n", input_path);
            return 1;
        }


        auto const img{ crx.image_data() };
        #endif


        #if 0
        adv::crx::image_data img{};
        parse_crx_file(input_path, img);
        #endif


        std::printf
        (
            "CRX: %dx%d, bpp=%d, mode=%d, stride=%d\n",
            img.width, img.height, img.bpp, img.mode, img.stride
        );

        if (!adv::crx::write_bmp(output_path, img))
        {
            std::fprintf
            (
                stderr, 
                "Error: failed to write BMP '%s'\n", 
                output_path.c_str()
            );
            return 1;
        }

        std::printf
        (
            "Success: wrote '%s' (%zu bytes)\n",
            output_path.c_str(), img.pixels.size()
        );

        return 0;
    }
    
}