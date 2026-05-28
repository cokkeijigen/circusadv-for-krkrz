#include <cstring>
#include <fstream>
#include <zlib.h>
#include <image_crx.hpp>

namespace adv::crx
{
    using pixel_header = crxg_header;
static uint32_t read32le(const uint8_t* p) noexcept
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read16le(const uint8_t* p) noexcept
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write32le(uint8_t* p, uint32_t v) noexcept
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void write16le(uint8_t* p, uint16_t v) noexcept
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

// ---- CRXB container helpers ----

static const uint8_t* find_crxg_in_container(const uint8_t* data, size_t size)
{
    if (size < 16) return nullptr;
    uint32_t magic = read32le(data);
    if (magic != 0x42585243) return nullptr;

    uint32_t entry_count = read32le(data + 8);
    if (entry_count == 0) return nullptr;

    size_t index_base = 0x10;
    for (uint32_t i = 0; i < entry_count; ++i)
    {
        size_t entry_off = index_base + (size_t)i * 0x20;
        if (entry_off + 8 > size) break;

        uint32_t data_off = read32le(data + entry_off);
        if ((size_t)data_off + 12 > size) continue;

        uint32_t entry_magic = read32le(data + data_off);
        if (entry_magic == 0x44585243)
        {
            uint32_t crxg_off = read32le(data + data_off + 8);
            if ((size_t)crxg_off + 20 > size) continue;
            return data + crxg_off;
        }
        else if (entry_magic == 0x47585243)
        {
            return data + data_off;
        }
    }

    for (size_t i = index_base + entry_count * 0x20; i + 20 < size; i += 4)
    {
        if (read32le(data + i) == 0x47585243)
            return data + i;
    }
    return nullptr;
}

// ---- Line decoders (match GARbro's UnpackV2 exactly) ----
// Each filter receives:
//   dst       - pointer to current row start in frame buffer
//   src       - ref to pointer into decompressed raw data (advanced by filter)
//   row_bytes - w * pixel_size (number of bytes to decode per row)
//   stride    - aligned stride (used for prev_row offset)
//   pixel_size - bytes per pixel

static void decode_filter0(uint8_t* dst, const uint8_t*& src,
    int row_bytes, int /*stride*/, int pixel_size)
{
    std::memcpy(dst, src, (size_t)pixel_size);
    src += pixel_size;
    for (int x = pixel_size; x < row_bytes; ++x)
        dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)dst[x - pixel_size]);
}

static void decode_filter1(uint8_t* dst, const uint8_t*& src,
    int row_bytes, int stride, int pixel_size)
{
    uint8_t* prev = dst - stride;
    for (int x = 0; x < row_bytes; ++x)
        dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x]);
}

static void decode_filter2(uint8_t* dst, const uint8_t*& src,
    int row_bytes, int stride, int pixel_size)
{
    uint8_t* prev = dst - stride;
    std::memcpy(dst, src, (size_t)pixel_size);
    src += pixel_size;
    for (int x = pixel_size; x < row_bytes; ++x)
        dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x - pixel_size]);
}

static void decode_filter3(uint8_t* dst, const uint8_t*& src,
    int row_bytes, int stride, int pixel_size)
{
    uint8_t* prev = dst - stride;
    int count = row_bytes - pixel_size;
    for (int x = 0; x < count; ++x)
        dst[x] = (uint8_t)((uint16_t)*src++ + (uint16_t)prev[x + pixel_size]);
    std::memcpy(dst + count, src, (size_t)pixel_size);
    src += pixel_size;
}

static void decode_filter4(uint8_t* dst, const uint8_t*& src,
    int row_bytes, int stride, int pixel_size, int width)
{
    for (int ch = 0; ch < pixel_size; ++ch)
    {
        int w = width;
        uint8_t val = *src++;
        uint8_t* ptr = dst + ch;
        while (w > 0)
        {
            *ptr = val;
            ptr += pixel_size;
            if (0 == --w)
                break;
            uint8_t next = *src++;
            if (val == next)
            {
                int count = *src++;
                for (int j = 0; j < count; ++j)
                {
                    *ptr = val;
                    ptr += pixel_size;
                }
                w -= count;
                if (w > 0)
                    val = *src++;
            }
            else
            {
                val = next;
            }
        }
    }
}

// ---- UnpackV2: zlib decompress + line filter decode ----

static bool decode_pixels_v2(const uint8_t* packed, size_t packed_size,
    int w, int h, int pixel_size, std::vector<uint8_t>& frame)
{
    size_t raw_size = (size_t)w * h * pixel_size + 0x2000;
    std::vector<uint8_t> raw(raw_size);
    uLongf dest_len = (uLongf)raw_size;

    int zret = uncompress(raw.data(), &dest_len, packed, (uLong)packed_size);
    if (zret != Z_OK)
        return false;
    raw.resize(dest_len);

    int row_bytes = w * pixel_size;
    int stride = ((row_bytes + 3) / 4) * 4;
    frame.resize((size_t)stride * h);
    std::memset(frame.data(), 0, frame.size());

    const uint8_t* src = raw.data();
    const uint8_t* src_end = raw.data() + raw.size();

    for (int y = 0; y < h; ++y)
    {
        if (src >= src_end) return false;
        uint8_t ctl = *src++;

        uint8_t* dst = frame.data() + y * stride;
        switch (ctl)
        {
        case 0: decode_filter0(dst, src, row_bytes, stride, pixel_size); break;
        case 1: decode_filter1(dst, src, row_bytes, stride, pixel_size); break;
        case 2: decode_filter2(dst, src, row_bytes, stride, pixel_size); break;
        case 3: decode_filter3(dst, src, row_bytes, stride, pixel_size); break;
        case 4: decode_filter4(dst, src, row_bytes, stride, pixel_size, w); break;
        default: return false;
        }
    }
    return true;
}

// ---- Alpha conversion ----
// After UnpackV2, pixel byte order depends on mode:
//   mode=0: [A, B, G, R] with inverted alpha (255=transparent)
//   mode=1: [B, G, R, A] standard BMP format (0=transparent)
//   mode=2: [A, B, G, R] with standard alpha (0=transparent)
// For mode=0: rotate to [B, G, R, A^255]  (flip alpha)
// For mode=1: no conversion needed (already BGRA)
// For mode=2: rotate to [B, G, R, A^0]    (no flip)

static void convert_alpha(image_data& img) noexcept
{
    if (img.bpp != 4)
        return;
    if (img.mode == 1)
        return;

    int alpha_flip = img.mode == 2 ? 0 : 0xFF;
    for (int y = 0; y < img.height; ++y)
    {
        uint8_t* row = img.pixels.data() + (size_t)y * img.stride;
        for (int x = 0; x < img.width; ++x)
        {
            uint8_t* p = row + x * 4;
            uint8_t a = p[0];
            uint8_t b = p[1];
            uint8_t g = p[2];
            uint8_t r = p[3];
            p[0] = b;
            p[1] = g;
            p[2] = r;
            p[3] = (uint8_t)(a ^ alpha_flip);
        }
    }
}

// ---- UnpackV1: LZSS decompression (GARbro style, 64KB sliding window) ----
// The LZSS stream decompresses into a buffer of stride * height bytes.
// This matches GARbro's Reader.UnpackV1 exactly.

static bool unpack_v1(const uint8_t* packed, size_t packed_size,
    int w, int h, int pixel_size, std::vector<uint8_t>& frame)
{
    int row_bytes = w * pixel_size;
    int stride = ((row_bytes + 3) / 4) * 4;
    size_t out_size = (size_t)stride * h;
    frame.resize(out_size);
    std::memset(frame.data(), 0, out_size);

    std::vector<uint8_t> window(0x10000);
    size_t win_pos = 0;
    size_t dst = 0;
    int flag = 0;
    size_t src_pos = 0;

    while (dst < out_size)
    {
        flag >>= 1;
        if (0 == (flag & 0x100))
        {
            if (src_pos >= packed_size) return false;
            flag = packed[src_pos++] | 0xff00;
        }

        if (0 != (flag & 1))
        {
            if (src_pos >= packed_size) return false;
            uint8_t dat = packed[src_pos++];
            window[win_pos++] = dat;
            win_pos &= 0xffff;
            frame[dst++] = dat;
        }
        else
        {
            if (src_pos >= packed_size) return false;
            uint8_t control = packed[src_pos++];
            int count, offset;

            if (control >= 0xc0)
            {
                if (src_pos + 1 > packed_size) return false;
                offset = ((control & 3) << 8) | packed[src_pos++];
                count = 4 + ((control >> 2) & 0xf);
            }
            else if (0 != (control & 0x80))
            {
                offset = control & 0x1f;
                count = 2 + ((control >> 5) & 3);
                if (0 == offset)
                {
                    if (src_pos >= packed_size) return false;
                    offset = packed[src_pos++];
                }
            }
            else if (0x7f == control)
            {
                if (src_pos + 4 > packed_size) return false;
                count = 2 + (packed[src_pos] | (packed[src_pos + 1] << 8));
                src_pos += 2;
                offset = packed[src_pos] | (packed[src_pos + 1] << 8);
                src_pos += 2;
            }
            else
            {
                if (src_pos + 2 > packed_size) return false;
                offset = packed[src_pos] | (packed[src_pos + 1] << 8);
                src_pos += 2;
                count = control + 4;
            }

            offset = (int)(win_pos - offset) & 0xffff;
            for (int k = 0; k < count && dst < out_size; k++)
            {
                uint8_t dat = window[offset++];
                offset &= 0xffff;
                window[win_pos++] = dat;
                win_pos &= 0xffff;
                frame[dst++] = dat;
            }
        }
    }
    return true;
}

// ---- public API ----

bool parse_crx(const uint8_t* data, size_t size, image_data& out)
{
    if (!data || size < 20)
        return false;

    uint32_t magic = read32le(data);
    const uint8_t* crxg_ptr = data;

    if (magic == 0x42585243)
    {
        crxg_ptr = find_crxg_in_container(data, size);
        if (!crxg_ptr)
            return false;
    }
    else if (magic != 0x47585243)
    {
        return false;
    }

    pixel_header hdr;
    std::memcpy(&hdr, crxg_ptr, sizeof(hdr));
    if (!hdr.is_valid())
        return false;

    int w = hdr.width;
    int h = hdr.height;
    int pixel_size = hdr.bpp();
    int crx_type = hdr.crx_type;

    if (w <= 0 || h <= 0 || pixel_size < 1)
        return false;

    size_t offset_in_file = (size_t)(crxg_ptr - data);
    const uint8_t* pixel_data = crxg_ptr + 20;
    size_t pixel_data_size = size - offset_in_file - 20;

    // Extended header for type >= 3: uint32 count + count * 16
    if (crx_type >= 3)
    {
        if (pixel_data_size < 4) return false;
        uint32_t ext_count = read32le(pixel_data);
        pixel_data += 4;
        pixel_data_size -= 4;
        size_t ext_bytes = (size_t)ext_count * 16;
        if (pixel_data_size < ext_bytes) return false;
        pixel_data += ext_bytes;
        pixel_data_size -= ext_bytes;
    }

    if (hdr.flags & 0x10)
    {
        if (pixel_data_size < 4) return false;
        pixel_data += 4;
        pixel_data_size -= 4;
    }

    out.width = w;
    out.height = h;
    out.bpp = pixel_size;
    out.mode = hdr.mode;
    int row_bytes = w * pixel_size;
    out.stride = ((row_bytes + 3) / 4) * 4;

    // For 8-bit index color images (colors != 0 && colors != 1), read palette
    if (pixel_size == 1)
    {
        int colors = hdr.colors;
        if (colors > 0x100) colors = 0x100;
        int color_entry_size = (hdr.colors == 0x102) ? 4 : 3;
        size_t palette_bytes = (size_t)colors * color_entry_size;
        if (pixel_data_size < palette_bytes)
            return false;

        out.palette_colors = colors;
        out.palette.resize((size_t)colors * 3);
        for (int i = 0; i < colors; ++i)
        {
            // Palette is stored as RGB in file, convert to BGR for BMP
            out.palette[(size_t)i * 3 + 0] = pixel_data[(size_t)i * color_entry_size + 2];
            out.palette[(size_t)i * 3 + 1] = pixel_data[(size_t)i * color_entry_size + 1];
            out.palette[(size_t)i * 3 + 2] = pixel_data[(size_t)i * color_entry_size + 0];
        }
        pixel_data += palette_bytes;
        pixel_data_size -= palette_bytes;
    }

    //auto size22222222222 = pixel_data - (crxg_ptr + 20);
    //printf("0x%X\n", size22222222222);

    if (crx_type == 1)
    {
        if (!unpack_v1(pixel_data, pixel_data_size, w, h, pixel_size, out.pixels))
            return false;
        convert_alpha(out);
        return true;
    }

    if (!decode_pixels_v2(pixel_data, pixel_data_size,
                          w, h, pixel_size, out.pixels))
        return false;

    // Alpha conversion: match GARbro's in-place conversion after UnpackV2
    convert_alpha(out);

    return true;
}

bool parse_crx_file(const std::string& path, image_data& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;

    std::streampos file_size = f.tellg();
    if (file_size <= 0) return false;

    std::vector<uint8_t> buf((size_t)file_size);
    f.seekg(0);
    f.read(reinterpret_cast<char*>(buf.data()), file_size);
    f.close();

    return parse_crx(buf.data(), buf.size(), out);
}

bool write_bmp(const std::string& path, const image_data& img)
{
    if (img.width <= 0 || img.height <= 0 || img.pixels.empty())
        return false;

    int pixel_size = img.bpp;
    int bmp_bpp = pixel_size == 4 ? 32 : 24;
    int bmp_row_size = ((img.width * bmp_bpp + 31) / 32) * 4;
    int pixel_bytes = bmp_row_size * img.height;

    // For 8-bit palette images, we expand to 24-bit for BMP output
    const uint8_t* pixel_src = img.pixels.data();
    std::vector<uint8_t> expanded;
    if (pixel_size == 1 && img.palette_colors > 0)
    {
        bmp_bpp = 24;
        bmp_row_size = ((img.width * bmp_bpp + 31) / 32) * 4;
        pixel_bytes = bmp_row_size * img.height;
        expanded.resize((size_t)img.width * 3 * img.height);
        for (int y = 0; y < img.height; ++y)
        {
            const uint8_t* src_row = img.pixels.data() + (size_t)y * img.stride;
            uint8_t* dst_row = expanded.data() + (size_t)y * img.width * 3;
            for (int x = 0; x < img.width; ++x)
            {
                int idx = src_row[x];
                if (idx >= img.palette_colors) idx = 0;
                dst_row[x * 3 + 0] = img.palette[(size_t)idx * 3 + 0];
                dst_row[x * 3 + 1] = img.palette[(size_t)idx * 3 + 1];
                dst_row[x * 3 + 2] = img.palette[(size_t)idx * 3 + 2];
            }
        }
        pixel_src = expanded.data();
        pixel_size = 3;
    }

    int header_size;
    int data_offset;
    if (bmp_bpp == 32)
    {
        // BITMAPV5HEADER (124 bytes) + BITMAPFILEHEADER (14 bytes) = 138
        header_size = 124;
        data_offset = 14 + header_size;
    }
    else
    {
        // BITMAPINFOHEADER (40 bytes) + BITMAPFILEHEADER (14 bytes) = 54
        header_size = 40;
        data_offset = 14 + header_size;
    }
    int file_size = data_offset + pixel_bytes;

    std::vector<uint8_t> bmp(file_size);
    std::memset(bmp.data(), 0, (size_t)file_size);

    // BITMAPFILEHEADER (14 bytes)
    bmp[0] = 'B'; bmp[1] = 'M';
    write32le(bmp.data() + 2,  (uint32_t)file_size);
    write32le(bmp.data() + 10, (uint32_t)data_offset);

    if (bmp_bpp == 32)
    {
        // BITMAPV5HEADER (size=124)
        write32le(bmp.data() + 14, 124);
        write32le(bmp.data() + 18, (uint32_t)img.width);
        write32le(bmp.data() + 22, (uint32_t)img.height);
        write16le(bmp.data() + 26, 1);
        write16le(bmp.data() + 28, 32);
        write32le(bmp.data() + 30, 3);          // BI_BITFIELDS
        write32le(bmp.data() + 34, (uint32_t)pixel_bytes);
        write32le(bmp.data() + 38, 2835);       // 72 DPI X
        write32le(bmp.data() + 42, 2835);       // 72 DPI Y
        write32le(bmp.data() + 54, 0x00FF0000); // Red mask
        write32le(bmp.data() + 58, 0x0000FF00); // Green mask
        write32le(bmp.data() + 62, 0x000000FF); // Blue mask
        write32le(bmp.data() + 66, 0xFF000000); // Alpha mask
        write32le(bmp.data() + 70, 0x73524742); // sRGB color space
    }
    else
    {
        // BITMAPINFOHEADER (size=40)
        write32le(bmp.data() + 14, 40);
        write32le(bmp.data() + 18, (uint32_t)img.width);
        write32le(bmp.data() + 22, (uint32_t)img.height);
        write16le(bmp.data() + 26, 1);
        write16le(bmp.data() + 28, 24);
        write32le(bmp.data() + 34, (uint32_t)pixel_bytes);
    }

    // After parse_crx, pixels are in standard BGRA/BGR format:
    //   24bpp: [B, G, R] per pixel
    //   32bpp: [B, G, R, A] per pixel (alpha already flipped by convert_alpha)
    // Expanded 8-bit: [B, G, R] per pixel
    // BMP needs bottom-up rows.
    uint8_t* dst = bmp.data() + data_offset;
    for (int y = 0; y < img.height; ++y)
    {
        int src_y = img.height - 1 - y;
        const uint8_t* src_line = pixel_src + (size_t)src_y * img.width * pixel_size;
        std::memcpy(dst + (size_t)y * bmp_row_size, src_line,
                    (size_t)img.width * pixel_size);
    }

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bmp.data()), bmp.size());
    f.close();
    return true;
}

} // namespace dc3pp::crx