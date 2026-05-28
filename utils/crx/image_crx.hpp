#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <span>

namespace adv::crx
{
	// CRXG pixel data header (20 bytes)
	#pragma pack(push, 1)
	struct crxg_header
	{
		inline static constexpr const uint32_t crxg_magic{ 0x47585243 };

		uint32_t magic;        // "CRXG" = 0x47585243
		int16_t  offset_x;     // +0x4: X position on canvas
		int16_t  offset_y;     // +0x6: Y position on canvas
		uint16_t width;        // +0x8
		uint16_t height;       // +0xA
		uint16_t crx_type;     // +0xC: 1=uncompressed, 2=zlib, 3=zlib+extended
		uint16_t flags;        // +0xE: bit4=compressed, bit0-3=palette flags
		uint16_t colors;       // +0x10: 0=24bit, 1=32bit, other=8bit indexed
		uint16_t mode;         // +0x12: alpha mode (1=0 is transparent)

		inline auto       bpp() const noexcept -> int  { return 0 == colors ? 3 : 1 == colors ? 4 : 1; };
		inline auto has_alpha() const noexcept -> bool { return 1 == colors; }
		inline auto  is_valid() const noexcept -> bool { return crxg_magic == magic; }
	};
	#pragma pack(pop)

	// Extended header for crx_type == 3 (24 bytes after pixel_header)
	#pragma pack(push, 1)
	struct extended_header
	{
		uint32_t unknown1;      // +0x14: usually 1
		uint32_t unknown2;      // +0x18: usually 0
		uint16_t map_width;     // +0x1C: canvas width  (>= width)
		uint16_t map_height;    // +0x1E: canvas height (>= height)
		uint16_t pic_start_x;   // +0x20: == offset_x
		uint16_t pic_start_y;   // +0x22: == offset_y
		uint16_t use_width;     // +0x24: effective non-transparent width
		uint16_t use_height;    // +0x26: effective non-transparent height
		uint32_t compress_len;  // +0x28: compressed data size
	};
	#pragma pack(pop)

	// Decoded BGRA image
	struct image_data
	{
		int  width{};
		int height{};
		int    bpp{};               // 3 or 4
		int stride{};
		int   mode{};               // alpha mode
		int palette_colors{};
		std::vector<uint8_t> palette{};
		std::vector<uint8_t>  pixels{};
	};

	class image_crx
	{
		std::span<uint8_t>    m_data{};
		crx::image_data m_image_data{};

		crx::crxg_header*       m_header{};
		crx::extended_header* m_extended{};

		bool   m_is_parsed{};

	public:

		inline image_crx(std::span<uint8_t> data) noexcept;

		inline auto is_parsed() const noexcept -> bool;

		inline auto       data() const noexcept -> const std::span<uint8_t>&;
		inline auto image_data() const noexcept -> const crx::image_data&;

		inline auto     crxg_header() const noexcept -> const crx::crxg_header*;
		inline auto extended_header() const noexcept -> const crx::extended_header*;

	protected:
		
		auto parse() noexcept -> void;

		auto unpack_v1(const std::span<uint8_t> data) noexcept -> bool;
		auto unpack_v2(const std::span<uint8_t> data) noexcept -> bool;
	};

	inline image_crx::image_crx(std::span<uint8_t> data) noexcept 
	{
		this->m_data = data;
		this->parse();
	}

	inline auto image_crx::is_parsed() const noexcept -> bool
	{
		return this->m_is_parsed;
	}

	inline auto image_crx::data() const noexcept -> const std::span<uint8_t>&
	{
		return this->m_data;
	}

	inline auto image_crx::image_data() const noexcept -> const crx::image_data&
	{
		return this->m_image_data;
	}

	inline auto image_crx::crxg_header() const noexcept -> const crx::crxg_header*
	{
		return this->m_header;
	}
	
	inline auto image_crx::extended_header() const noexcept -> const crx::extended_header*
	{
		return this->m_extended;
	}

	using image = image_crx;
	using data  = image_data;

	// Parse CRX file from path
	auto parse_crx_file(const std::string& path, image_data& out) -> bool;

	// Parse CRX from raw buffer (handles CRXB container + CRXG raw)
	auto parse_crx(const uint8_t* data, size_t size, image_data& out) -> bool;

	// Write decoded image to BMP file
	auto write_bmp(const std::string& path, const image_data& img) -> bool;

}