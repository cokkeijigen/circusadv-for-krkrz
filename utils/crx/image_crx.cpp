#include <iostream>
#include <cstring>
#include <zlib.h>
#include <image_crx.hpp>

namespace adv::crx
{

    auto image_crx::parse() noexcept -> void
    {
        this->m_is_parsed = false;

        if (this->m_data.data() == nullptr || this->m_data.size() < sizeof(crx::crxg_header))
        {
            return;
        }

        this->m_header = reinterpret_cast<crx::crxg_header*>(this->m_data.data());

        if (!this->m_header->is_valid())
        {
            return;
        }

        if (this->m_header->height == 0 || this->m_header->width == 0 || this->m_header->bpp() < 1)
        {
            return;
        }
        
        size_t crxg_data_offset{};
        const auto crxg_data{ this->m_data.subspan(sizeof(crx::crxg_header)) };

        if (this->m_header->crx_type >= 3)
        {
            if (crxg_data.size() < 4)
            {
                return;
            }

            crxg_data_offset += 4;
            
            const auto ext_bytes
            { 
                *reinterpret_cast<uint32_t*>(crxg_data.data()) * 16
            };

            if (crxg_data.size() - crxg_data_offset < ext_bytes)
            {
                return;
            }

            crxg_data_offset += ext_bytes;
        }

        if (this->m_header->flags & 0x10)
        {
            if (crxg_data.size() - crxg_data_offset < 4)
            {
                return;
            }
            crxg_data_offset += 4;
        }

        this->m_image_data.width  = this->m_header->width;
        this->m_image_data.height = this->m_header->height;
        this->m_image_data.bpp    = this->m_header->bpp();
        this->m_image_data.mode   = this->m_header->mode;

        int   row_bytes{ this->m_header->width * this->m_header->bpp() };
        this->m_image_data.stride = ((row_bytes + 3) / 4) * 4;

        if (crxg_data_offset == sizeof(crx::extended_header))
        {
            this->m_extended = reinterpret_cast<crx::extended_header*>(crxg_data.data());
        }
        
        if (this->m_header->bpp() == 1)
        {

            const int           colors{ this->m_header->colors > 0x100 ? 0x100 : this->m_header->colors };
            const int color_entry_size{ this->m_header->colors == 0x102 ? 4 : 3 };

            const auto palette_size{ static_cast<size_t>(colors * color_entry_size)    };
            const auto palette_span{ crxg_data.subspan(crxg_data_offset, palette_size) };

            this->m_image_data.palette_colors = colors;
            this->m_image_data.palette.resize(static_cast<size_t>(colors) * 3);

            for (size_t i{}; i < static_cast<size_t>(colors); ++i)
            {
                this->m_image_data.palette[i * 3 + 0] = palette_span[i * color_entry_size + 2];
                this->m_image_data.palette[i * 3 + 1] = palette_span[i * color_entry_size + 1];
                this->m_image_data.palette[i * 3 + 2] = palette_span[i * color_entry_size + 0];
            }

            crxg_data_offset += static_cast<size_t>(colors) * color_entry_size;
        }

        if (this->m_header->crx_type == 1)
        {
            if (!this->unpack_v1(crxg_data.subspan(crxg_data_offset))) 
            {
                return;
            }
            this->m_is_parsed = true;
        }
        else 
        {
            if (!this->unpack_v2(crxg_data.subspan(crxg_data_offset)))
            {
                return;
            }
            this->m_is_parsed = true;
        }

        if (this->m_image_data.bpp != 4 || this->m_image_data.mode == 1)
        {
            return;
        }

        const int alpha_flip{ this->m_image_data.mode == 2 ? 0 : 0xFF };
        for (size_t y{}; y < static_cast<size_t>(this->m_image_data.height); ++y)
        {
            uint8_t* row
            {
                this->m_image_data.pixels.data() + y *
                this->m_image_data.stride
            };
            for (int x{}; x < this->m_image_data.width; ++x)
            {
                uint8_t* p{ row + x * 4 };
                std::swap(p[0], p[1]);
                std::swap(p[1], p[2]);
                std::swap(p[2], p[3]);
                p[3] ^= alpha_flip;
            }
        }
    }

    auto image_crx::unpack_v1(const std::span<uint8_t> data) noexcept -> bool
    {
        const int   row_bytes{ this->m_image_data.width * this->m_image_data.bpp     };
        const int pixels_size{ this->m_image_data.stride * this->m_image_data.height };

        this->m_image_data.pixels.clear();
        this->m_image_data.pixels.resize(pixels_size);

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
                this->m_image_data.pixels[dst_pos++] = dat;
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
                    this->m_image_data.pixels[dst_pos++] = dat;
                }
            }
        }
        return true;
    }

    auto image_crx::unpack_v2(const std::span<uint8_t> data) noexcept -> bool
    {
        const int pixels_size{ this->m_image_data.stride * this->m_image_data.height };
        const int   row_bytes{ this->m_image_data.width * this->m_image_data.bpp     };

        const size_t raw_size
        {
            static_cast<size_t>(this->m_image_data.width) *
            static_cast<size_t>(this->m_image_data.height)*
            static_cast<size_t>(this->m_image_data.bpp)
            + 0x2000
        };

        const uint8_t* crxg_data_ptr{ data.data() };
        size_t        crxg_data_size{ data.size() };

        std::vector<uint8_t> raw{};
        raw.resize(raw_size);

        uLongf dest_len{ static_cast<uLongf>(raw_size) };
        uLongf  src_len{ static_cast<uLongf>(crxg_data_size) };
        const int  zret{ uncompress(raw.data(), &dest_len, crxg_data_ptr, src_len) };

        if (zret != Z_OK) 
        {
            return false;
        }
        
        raw.resize(static_cast<size_t>(dest_len));

        this->m_image_data.pixels.clear();
        this->m_image_data.pixels.resize(static_cast<size_t>(pixels_size));

        const uint8_t* src{ raw.data() };
        const uint8_t* src_end{ raw.data() + raw.size() };
            
        for (size_t y{}; y < static_cast<size_t>(this->m_image_data.height); ++y)
        {
            if (src >= src_end) 
            {
                return false;
            }

            uint8_t* dst{ this->m_image_data.pixels.data() + y * this->m_image_data.stride };

            switch (*src++) 
            {
            case 0: 
            {
                std::memcpy(dst, src, static_cast<size_t>(this->m_image_data.bpp));
                src += this->m_image_data.bpp;

                for (int x = this->m_image_data.bpp; x < row_bytes; ++x)
                {
                    const auto tmp
                    { 
                        static_cast<uint16_t>(*src) +
                        static_cast<uint16_t>(dst[x - this->m_image_data.bpp])
                    };
                    dst[x] = static_cast<uint8_t>(tmp);
                    ++src;
                }
                break;
            }
            case 1:
            {
                const uint8_t* prev{ dst - this->m_image_data.stride };
                for (int x{}; x < row_bytes; ++x)
                {
                    const auto tmp
                    {
                        static_cast<uint16_t>(*src) +
                        static_cast<uint16_t>(prev[x])
                    };
                    dst[x] = static_cast<uint8_t>(tmp);
                    ++src;
                }
                break;
            }
            case 2: 
            {
                const uint8_t* prev{ dst - this->m_image_data.stride };
                std::memcpy(dst, src, static_cast<size_t>(this->m_image_data.bpp));
                src += static_cast<size_t>(this->m_image_data.bpp);

                for (int x = this->m_image_data.bpp; x < row_bytes; ++x)
                {
                    const auto tmp
                    {
                        static_cast<uint16_t>(*src) +
                        static_cast<uint16_t>(prev[x - this->m_image_data.bpp])
                    };
                    dst[x] = static_cast<uint8_t>(tmp);
                    ++src;
                }
                break;
            }
            case 3: 
            {
                const uint8_t* prev{ dst - this->m_image_data.stride };
                const int     count{ row_bytes - this->m_image_data.bpp };
                    
                for (int x{}; x < count; ++x)
                {
                    const auto tmp
                    {
                        static_cast<uint16_t>(*src) +
                        static_cast<uint16_t>(prev[x + this->m_image_data.bpp])
                    };
                    dst[x] = static_cast<uint8_t>(tmp);
                    ++src;
                }
                    
                std::memcpy(dst + count, src, static_cast<size_t>(this->m_image_data.bpp));
                src += this->m_image_data.bpp;
                    
                break;
            }
            case 4: 
            {
                for (int ch{}; ch < this->m_image_data.bpp; ++ch)
                {
                    uint8_t* ptr{ dst + ch };
                    uint8_t  val{ *src++   };

                    int w{ this->m_image_data.width };
                    while (w > 0) 
                    {
                        *ptr = val;
                        ptr += this->m_image_data.bpp;
                            
                        if (--w == 0) 
                        {
                            break;
                        }

                        uint8_t next{ *src++ };
                        if (val == next) 
                        {
                            int count{ *src++ };
                            for (int j{}; j < count; ++j)
                            {
                                *ptr = val;
                                ptr += this->m_image_data.bpp;
                            }
                            w -= count;
                            if (w > 0)
                            {
                                val = *src++;
                            }
                        }
                        else 
                        {
                            val = next;
                        }
                    }
                }
                break;
            }
            default: return false;
            }
        }

        return true;
    }
}