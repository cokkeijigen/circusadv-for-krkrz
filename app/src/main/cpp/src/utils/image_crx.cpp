#include <iostream>
#include <cstring>
#include <fstream>
#include <zlib.h>
#include <image_crx.hpp>
#include "native-lib.hpp"
namespace adv::crx
{

    static auto unpack_v1(const crxg_header* header, const std::span<const uint8_t> data, std::vector<uint8_t>& output) noexcept -> bool
    {
        const int   row_bytes{ header->width * header->bpp() };
        const int pixels_size{ ((row_bytes + 3) / 4) * 4 * header->height };

        if (output.capacity() < pixels_size) 
        {
            output.clear();
            output.reserve(pixels_size);
        }
        output.resize(pixels_size);

        std::vector<uint8_t> buffer{};
        buffer.resize(0x10000);

        int flag{};
        size_t buf_pos{}, dst_pos{}, src_pos{};

        const uint8_t* packed{ data.data() };
        size_t    packed_size{ data.size() };

        while (dst_pos < pixels_size)
        {
            flag >>= 1;
            if (0 == (flag & 0x100))
            {
                if (src_pos >= packed_size) 
                { 
                    return false;
                }

                flag = packed[src_pos++] | 0xff00;
            }

            if (0 != (flag & 1))
            {
                if (src_pos >= packed_size) 
                {
                    return false;
                }
                uint8_t dat{ packed[src_pos++] };
                buffer[buf_pos++] = dat;
                buf_pos &= 0xffff;
                output[dst_pos++] = dat;
            }
            else
            {
                if (src_pos >= packed_size)
                {
                    return false;
                }

                uint8_t control{ packed[src_pos++] };
                int count{}, offset{};

                if (control >= 0xc0)
                {
                    if (src_pos + 1 > packed_size)
                    {
                        return false;
                    }

                    offset = ((control & 3) << 8) | packed[src_pos++];
                    count = 4 + ((control >> 2) & 0xf);
                }
                else if (0 != (control & 0x80))
                {
                    offset = control & 0x1f;
                    count = 2 + ((control >> 5) & 3);
                    if (0 == offset)
                    {
                        if (src_pos >= packed_size)
                        {
                            return false;
                        }
                        offset = packed[src_pos++];
                    }
                }
                else if (0x7f == control)
                {
                    if (src_pos + 4 > packed_size)
                    {
                        return false;
                    }

                    count = 2 + (packed[src_pos] | (packed[src_pos + 1] << 8));
                    src_pos += 2;
                    offset = packed[src_pos] | (packed[src_pos + 1] << 8);
                    src_pos += 2;
                }
                else
                {
                    if (src_pos + 2 > packed_size) 
                    {
                        return false;
                    }

                    offset = packed[src_pos] | (packed[src_pos + 1] << 8);
                    src_pos += 2;
                    count = control + 4;
                }

                offset = (static_cast<int>(buf_pos) - offset) & 0xffff;
                for (int k{}; k < count && dst_pos < pixels_size; ++k)
                {
                    uint8_t dat{ buffer[offset++] };
                    offset &= 0xffff;
                    buffer[buf_pos++] = dat;
                    buf_pos &= 0xffff;
                    output[dst_pos++] = dat;
                }
            }
        }
        return true;
    }

    static auto unpack_v2(const crxg_header* header, const std::span<const uint8_t> data, std::vector<uint8_t>& output) noexcept -> bool
    {
        const int   w{ header->width  };
        const int   h{ header->height };
        const int   pixel_size{ header->bpp() };
        const int   row_bytes{ w * pixel_size };
        const int      stride{ ((row_bytes + 3) / 4) * 4 };
        const int pixels_size{ stride * h };

        const uint8_t* packed{ data.data() };
        size_t    packed_size{ data.size() };

        size_t raw_size = (size_t)w * h * pixel_size + 0x2000;
        std::vector<uint8_t> raw(raw_size);
        uLongf dest_len = (uLongf)raw_size;

        const int zret = ::uncompress(raw.data(), &dest_len, packed, (uLong)packed_size);
        if (zret != Z_OK) return false;
        raw.resize(dest_len);

        output.resize((size_t)stride * h);
        std::memset(output.data(), 0, output.size());

        const uint8_t* src = raw.data();
        const uint8_t* src_end = raw.data() + raw.size();

        for (int y = 0; y < h; ++y)
        {
            if (src >= src_end) return false;

            uint8_t* dst = output.data() + y * stride;

            switch (*src++)
            {
            case 0:
            {
                std::memcpy(dst, src, (size_t)pixel_size);
                src += pixel_size;
                for (int x = pixel_size; x < row_bytes; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)dst[x - pixel_size]);
                break;
            }
            case 1:
            {
                uint8_t* prev = dst - stride;
                for (int x = 0; x < row_bytes; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x]);
                break;
            }
            case 2:
            {
                uint8_t* prev = dst - stride;
                std::memcpy(dst, src, (size_t)pixel_size);
                src += pixel_size;
                for (int x = pixel_size; x < row_bytes; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x - pixel_size]);
                break;
            }
            case 3:
            {
                uint8_t* prev = dst - stride;
                int count = row_bytes - pixel_size;
                for (int x = 0; x < count; ++x)
                    dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x + pixel_size]);
                std::memcpy(dst + count, src, (size_t)pixel_size);
                src += pixel_size;
                break;
            }
            case 4:
            {
                for (int ch = 0; ch < pixel_size; ++ch)
                {
                    int ww = w;
                    uint8_t val = *src++;
                    uint8_t* ptr = dst + ch;
                    while (ww > 0)
                    {
                        *ptr = val;
                        ptr += pixel_size;
                        if (0 == --ww) break;
                        uint8_t next = *src++;
                        if (val == next)
                        {
                            int count = *src++;
                            for (int j = 0; j < count; ++j)
                            {
                                *ptr = val;
                                ptr += pixel_size;
                            }
                            ww -= count;
                            if (ww > 0) val = *src++;
                        }
                        else
                        {
                            val = next;
                        }
                    }
                }
                break;
            }
            default:
                std::fprintf(stderr, "unpack_v2: unknown filter %d at row %d\n", *(src - 1), y);
                return false;
            }
        }

        return true;
    }

    crxg_view::crxg_view(const std::span<const uint8_t> data) noexcept : m_raw{ data }
    {
        if (this->m_raw.data() == nullptr || this->m_raw.size() < sizeof(crx::crxg_header))
        {
            return;
        }
        
        this->m_header = crx::crxg_header::cast(this->m_raw.data());

        if (!this->m_header->is_valid())
        {
            return;
        }

        size_t crxg_data_offset{};
        const auto crxg_data{ this->m_raw.subspan(sizeof(crx::crxg_header)) };

        if (this->m_header->crx_type >= 3)
        {
            if (crxg_data.size() < 4)
            {
                return;
            }

            this->m_extended = crx::extended_header::cast(crxg_data.data());

            const auto ext_bytes
            {
                this->m_extended->count * sizeof(crx::extended_header::entry)
            };

            if (crxg_data.size() - crxg_data_offset < ext_bytes)
            {
                return;
            }

            crxg_data_offset += ext_bytes + 4;
        }

        if (this->m_header->flags & 0x10)
        {
            if (crxg_data.size() - crxg_data_offset < 4)
            {
                return;
            }
            crxg_data_offset += 4;
        }

        if (this->m_header->bpp() == 1)
        {

            const int           colors{ this->m_header->colors > 0x100 ? 0x100 : this->m_header->colors };
            const int color_entry_size{ this->m_header->colors == 0x102 ? 4 : 3 };
            
            this->m_palette.size   = color_entry_size;
            this->m_palette.length = colors;

            const auto palette_size{ static_cast<size_t>(colors * color_entry_size) };
            this->m_palette.data   = crxg_data.data() + crxg_data_offset;

            crxg_data_offset += static_cast<size_t>(colors * color_entry_size);
        }

        this->m_compress = crxg_data.subspan(crxg_data_offset);
    }

    auto image_crx::unpack(std::vector<uint8_t>& buffer) const noexcept -> bool
    {
        if (!this->is_valid())
        {
            return false;
        }

        if (this->m_view.m_header->crx_type == 1)
        {
            if (!crx::unpack_v1(this->m_view.m_header, this->m_view.m_compress, buffer))
            {
                return false;
            }
        }
        else
        {
            if (!crx::unpack_v2(this->m_view.m_header, this->m_view.m_compress, buffer))
            {
                return false;
            }
        }

        return true;
    }
    
    auto image_crx::decode(std::vector<uint8_t>& buffer, uint8_t keyidx) const noexcept -> bool
    {
        if (!this->unpack(buffer)) 
        {
            return false;
        }

        std::vector<uint8_t> temp{};
        const auto* header  { this->m_view.m_header };
        const int   w       { header->width  };
        const int   h       { header->height };
        const int   src_bpp { header->bpp()  };
        const int   src_row_bytes { w * src_bpp };
        const int   src_stride    { ((src_row_bytes + 3) / 4) * 4 };

        if (src_bpp == 1)
        {
            LOGD("src_bpp == 1 !!!!!!!!!!!!!!!!!");

            // 8-bit indexed: expand palette to 32-bit BGRA
            const auto& pal{ this->m_view.m_palette };
            if (pal.data == nullptr || pal.length == 0) 
            {
                return false;
            }

            const int       dst_stride{ w * 4 };
            const size_t    dst_size  { static_cast<size_t>(dst_stride) * h };

            // If capacity is insufficient, move old data to temp to avoid
            // the copy that resize would otherwise do on reallocation.
            if (buffer.capacity() < dst_size)
            {
                temp = std::move(buffer);
            }

            buffer.resize(dst_size);

            const uint8_t* src_base{ temp.empty() ? buffer.data() : temp.data() };
            for (int y{ h - 1 }; y >= 0; --y)
            {
                const uint8_t* src_row{ src_base + static_cast<size_t>(y) * src_stride };
                uint8_t*       dst_row{ buffer.data() + static_cast<size_t>(y) * dst_stride };
                for (int x{ w - 1 }; x >= 0; --x)
                {
                    const uint8_t idx{ src_row[x] };
                    const size_t pal_off{ static_cast<size_t>(idx < pal.length ? idx : 0) * pal.size };
                    uint8_t* px{ dst_row + x * 4 };
                    px[0] = pal.data[pal_off + 2]; // B
                    px[1] = pal.data[pal_off + 1]; // G
                    px[2] = pal.data[pal_off + 0]; // R
                    px[3] = (idx == keyidx) ? 0 : 255;
                }
            }
            return true;
        }

        if (src_bpp == 3)
        {
            LOGD("src_bpp == 3 !!!!!!!!!!!!!!!!!");

            // 24-bit BGR: pad to 32-bit BGRA
            const int       dst_stride{ w * 4 };
            const size_t    dst_size  { static_cast<size_t>(dst_stride) * h };

            if (buffer.capacity() < dst_size) 
            {
                temp = std::move(buffer);
            }
            buffer.resize(dst_size);

            const uint8_t* src_base{ temp.empty() ? buffer.data() : temp.data() };

            // Process bottom-to-top, right-to-left
            for (int y{ h - 1 }; y >= 0; --y)
            {
                const uint8_t* src_row{ src_base + static_cast<size_t>(y) * src_stride };
                uint8_t*       dst_row{ buffer.data() + static_cast<size_t>(y) * dst_stride };
                for (int x{ w - 1 }; x >= 0; --x)
                {
                    uint8_t* px{ dst_row + x * 4 };
//                    px[0] = src_row[x * 3 + 0]; // B
//                    px[1] = src_row[x * 3 + 1]; // G
//                    px[2] = src_row[x * 3 + 2]; // R
                    px[0] = src_row[x * 3 + 2]; // R
                    px[1] = src_row[x * 3 + 1]; // G
                    px[2] = src_row[x * 3 + 0]; // B
                    px[3] = 255;
                }
            }
            return true;
        }

        if (src_bpp == 4 && header->mode != 1)
        {
            // 32-bit with non-standard byte order: convert to BGRA
            // mode 0: [A, B, G, R] with inverted alpha (255=transparent)
            // mode 2: [A, B, G, R] with standard alpha (0=transparent)
            // Target: [B, G, R, A] with standard alpha
            const int alpha_flip{ header->mode == 2 ? 0 : 0xFF };
            for (int y{}; y < h; ++y)
            {
                uint8_t* row{ buffer.data() + static_cast<size_t>(y) * src_stride };
                for (int x{}; x < w; ++x)
                {
                    uint8_t* p{ row + x * 4 }; // [A, B, G, R]
                    const uint8_t a{ static_cast<uint8_t>(p[0] ^ alpha_flip) };
                    p[0] = p[1]; // B
                    p[1] = p[2]; // G
                    p[2] = p[3]; // R
                    p[3] = a;    // A
                }
            }
        }
        // mode 1: already [B, G, R, A], no conversion needed

        return true;
    }

    auto write_bmp(const std::string& path, const std::vector<uint8_t>& pixels, int width, int height) -> bool
    {
        // Use BITMAPV5HEADER (124 bytes) with BI_BITFIELDS to preserve alpha channel
        // BMP file header (14 bytes) + V5 header (124 bytes) = 138 bytes
        static constexpr int bmp_header_size  { 14 };
        static constexpr int v5_header_size   { 124 };
        static constexpr int total_header_size{ bmp_header_size + v5_header_size };

        #pragma pack(push, 1)
        struct bmp_file_header
        {
            uint16_t bfType      { 0x4D42 };
            uint32_t bfSize      {           };
            uint16_t bfReserved1 { 0         };
            uint16_t bfReserved2 { 0         };
            uint32_t bfOffBits   { total_header_size };
        };
        static_assert(sizeof(bmp_file_header) == 14, "BMP file header must be 14 bytes");

        struct bmp_v5_header
        {
            uint32_t biSize          { v5_header_size };
            int32_t  biWidth         {                };
            int32_t  biHeight        {                };
            uint16_t biPlanes        { 1              };
            uint16_t biBitCount      { 32             };
            uint32_t biCompression   { 3              }; // BI_BITFIELDS
            uint32_t biSizeImage     {                };
            int32_t  biXPelsPerMeter { 0              };
            int32_t  biYPelsPerMeter { 0              };
            uint32_t biClrUsed       { 0              };
            uint32_t biClrImportant  { 0              };
            // BITMAPV4HEADER extension (beyond BITMAPINFOHEADER)
            uint32_t bV4RedMask      { 0x00FF0000     };
            uint32_t bV4GreenMask    { 0x0000FF00     };
            uint32_t bV4BlueMask     { 0x000000FF     };
            uint32_t bV4AlphaMask    { 0xFF000000     };
            uint32_t bV4CSType       { 0x73524742     }; // LCS_sRGB
            int32_t  bV4EndpointsR_X { 0              };
            int32_t  bV4EndpointsR_Y { 0              };
            int32_t  bV4EndpointsR_Z { 0              };
            int32_t  bV4EndpointsG_X { 0              };
            int32_t  bV4EndpointsG_Y { 0              };
            int32_t  bV4EndpointsG_Z { 0              };
            int32_t  bV4EndpointsB_X { 0              };
            int32_t  bV4EndpointsB_Y { 0              };
            int32_t  bV4EndpointsB_Z { 0              };
            uint32_t bV4GammaRed     { 0              };
            uint32_t bV4GammaGreen   { 0              };
            uint32_t bV4GammaBlue    { 0              };
            // BITMAPV5HEADER extension (beyond BITMAPV4HEADER)
            uint32_t bV5Intent       { 0              }; // LCS_GM_ABS_COLORIMETRIC
            uint32_t bV5ProfileData  { 0              };
            uint32_t bV5ProfileSize  { 0              };
            uint32_t bV5Reserved     { 0              };
        };
        static_assert(sizeof(bmp_v5_header) == 124, "BMP V5 header must be 124 bytes");
        #pragma pack(pop)

        const int row_stride{ width * 4 };
        const int pixel_data_size{ row_stride * height };
        const int file_size{ total_header_size + pixel_data_size };

        bmp_file_header fh{};
        fh.bfSize = static_cast<uint32_t>(file_size);

        bmp_v5_header ih{};
        ih.biWidth     = width;
        ih.biHeight    = height;
        ih.biSizeImage = static_cast<uint32_t>(pixel_data_size);

        std::ofstream ofs{ path, std::ios::binary };
        if (!ofs.is_open()) return false;

        ofs.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
        ofs.write(reinterpret_cast<const char*>(&ih), sizeof(ih));

        // Write pixel data bottom-up (BMP convention)
        for (int y{ height - 1 }; y >= 0; --y)
        {
            const size_t row_off{ static_cast<size_t>(y) * row_stride };
            ofs.write(reinterpret_cast<const char*>(pixels.data() + row_off), row_stride);
        }

        return true;
    }

}