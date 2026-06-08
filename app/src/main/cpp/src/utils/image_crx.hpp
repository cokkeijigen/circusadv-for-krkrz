#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <span>
#include <optional>

namespace adv::crx
{

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

		static inline auto cast(const void* ptr) noexcept -> const crxg_header* 
		{
			return reinterpret_cast<const crxg_header*>(ptr);
		}
	};
	#pragma pack(pop)

	#pragma pack(push, 1)
	struct extended_header
	{
		uint32_t count;
		struct entry
		{
			uint32_t unknown;    // usually 0
			uint16_t map_width;   // canvas width  (>= width)
			uint16_t map_height;  // canvas height (>= height)
			uint16_t pic_start_x; // == offset_x
			uint16_t pic_start_y; // == offset_y
			uint16_t use_width;   // effective non-transparent width
			uint16_t use_height;  // effective non-transparent height
		} entries[NULL];

		static inline auto cast(const void* ptr) noexcept -> const extended_header*
		{
			return reinterpret_cast<const extended_header*>(ptr);
		}
	};
	#pragma pack(pop)

	struct palette_data 
	{
		uint16_t     length;
		uint16_t       size;
		const uint8_t* data;
	};

	class image_crx;

	class crxg_view
	{
		std::span<const uint8_t> m_raw{};
		const crxg_header*       m_header{};
		const extended_header* m_extended{};

		palette_data m_palette{};
		std::span<const uint8_t> m_compress{};

		friend image_crx;

	public:

		inline crxg_view() noexcept {};
		crxg_view(const std::span<const uint8_t> data) noexcept;

		inline auto raw     () const noexcept -> const std::span<const uint8_t>&;
		inline auto header  () const noexcept -> const crx::crxg_header*;
		inline auto extended() const noexcept -> const crx::extended_header*;
		inline auto palette () const noexcept -> const crx::palette_data&;
		inline auto compress() const noexcept -> const std::span<const uint8_t>&;
	};

	class image_crx
	{
		crxg_view m_view{};

	public:

		inline image_crx(std::span<const uint8_t> data) noexcept;

		inline auto is_valid() const noexcept -> bool;
		inline auto     view() const noexcept -> const crxg_view&;

		inline auto   width() const noexcept -> int;
		inline auto  height() const noexcept -> int;
		inline auto     bpp() const noexcept -> int;
		inline auto  stride() const noexcept -> int;
		inline auto palette() const noexcept -> const crx::palette_data&;

		inline auto decode(uint8_t keyidx = 255) const noexcept -> std::optional<std::vector<uint8_t>>;
		auto decode(std::vector<uint8_t>& buffer, uint8_t keyidx = 255) const noexcept -> bool;

		inline auto unpack() const noexcept -> std::optional<std::vector<uint8_t>>;
		auto unpack(std::vector<uint8_t>& buffer) const noexcept -> bool;
	};

	// Write decoded BGRA pixel data to BMP file
	auto write_bmp(const std::string& path, const std::vector<uint8_t>& pixels, int width, int height) -> bool;
	
    inline auto crxg_view::raw() const noexcept -> const std::span<const uint8_t>&
	{
		return this->m_raw;
	}

	inline auto crxg_view::header() const noexcept -> const crx::crxg_header*
	{
		return this->m_header;
	}

	inline auto crxg_view::extended() const noexcept -> const crx::extended_header*
	{
		return this->m_extended;
	}

	inline auto crxg_view::palette() const noexcept -> const crx::palette_data&
	{
		return this->m_palette;
	}

	inline auto crxg_view::compress() const noexcept -> const std::span<const uint8_t>&
	{
		return this->m_compress;
	}

    inline image_crx::image_crx(std::span<const uint8_t> data) noexcept : m_view{ data } 
	{
	}

	inline auto image_crx::is_valid() const noexcept -> bool 
	{
 		return bool
		{ 
			this->m_view.m_header != nullptr  &&
			this->m_view.m_header->is_valid() &&
			this->m_view.m_compress.size()  > 0
		};
	}

	inline auto image_crx::view() const noexcept -> const crxg_view&
	{
		return this->m_view;
	}

	inline auto image_crx::width() const noexcept -> int
	{
		return 
		{
			this->m_view.m_header != nullptr ? 
			this->m_view.m_header->width : 0
		};
	}

	inline auto image_crx::height() const noexcept -> int
	{
		return
		{
			this->m_view.m_header != nullptr ?
			this->m_view.m_header->height : 0
		};
	}

	inline auto image_crx::bpp() const noexcept -> int
	{
		return
		{
			this->m_view.m_header != nullptr ?
			this->m_view.m_header->bpp() : 0
		};
	}

	inline auto image_crx::stride() const noexcept -> int
	{
		if (this->m_view.m_header == nullptr) 
		{
			return 0;
		}

		const int row_bytes
		{ 
			this->m_view.m_header->width *
			this->m_view.m_header->bpp() 
		};
		return { ((row_bytes + 3) / 4) * 4 };
	}

	inline auto image_crx::palette() const noexcept -> const crx::palette_data&
	{
		return this->m_view.m_palette;
	}

	inline auto image_crx::decode(uint8_t keyidx) const noexcept -> std::optional<std::vector<uint8_t>>
	{
		std::vector<uint8_t> result{};
		if (this->decode(result, keyidx))
		{
			return result;
		};

		return std::nullopt;
	}

	inline auto image_crx::unpack() const noexcept -> std::optional<std::vector<uint8_t>>
	{
		std::vector<uint8_t> result{};
		if (this->unpack(result)) 
		{
			return result;
		};

		return std::nullopt;
	}
}