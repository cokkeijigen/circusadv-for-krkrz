
namespace dc3pp::mes
{

	struct script_info
	{
		struct section
		{
			const uint8_t beg{}, end{};
			auto is(const uint8_t key) const noexcept -> bool;
		};

		enum offset_t : uint8_t
		{
			offset1, // head[0] * 0x04 + 0x04
			offset2  // head[0] * 0x06 + 0x04
		};

		std::string name;
		offset_t  offset;
		uint16_t version;
		section  uint8x2; // [op: byte] [arg1: uint8] [arg2: uint8]
		section uint8str; // [op: byte] [arg1: uint8] [arg2: string]
		section	  string; // [op: byte] [arg1: string]
		section   encstr; // [op: byte] [arg1: encstr]
		section uint16x4; // [op: byte] [arg1: uint16] [arg2: uint16] [arg3: uint16] [arg4: uint16]
		uint8_t   enckey;
	};

	
	/** dc3pp
	script_info
	{ 
		.name    = "dc3pp", 
		.offset  = offset2, 
		.version =  0x9872, 
		.uint8x2 { 0x00, 0x2A }, 
		.uint8str{ 0x2B, 0x32 },
		.uint8str{ 0x33, 0x4E }, 
		.encstr  { 0x4F, 0x51 }, 
		.uint16x4{ 0x52, 0xFF },  
		.enckey  = 0x20, 
	}
	*/

	struct token
	{
		#pragma pack(push, 1)
		struct uint8x2_t 
		{
			uint8_t op;
			uint8_t arg1;
			uint8_t arg2;
		};
		#pragma pack(pop)

		#pragma pack(push, 1)
		struct uint8str_t 
		{
			uint8_t op;
			uint8_t arg1;
			char str[];
		};
		#pragma pack(pop)

		#pragma pack(push, 1)
		struct string_t 
		{
			uint8_t op;
			char str[];
		};
		#pragma pack(pop)

		#pragma pack(push, 1)
		struct encstr_t
		{
			uint8_t op;
			char str[];
		};
		#pragma pack(pop)

		#pragma pack(push, 1)
		struct uint16x4_t
		{
			uint8_t op;
			uint16_t arg1;
			uint16_t arg2;
			uint16_t arg3;
			uint16_t arg4;
		};
		#pragma pack(pop)

		const uint8_t*      data{};

		auto uint16x4() const noexcept -> const uint16x4_t*;
		auto uint8str() const noexcept -> const uint8str_t*;
		auto uint8x2 () const noexcept -> const uint8x2_t*;
		auto string  () const noexcept -> const string_t*;
		auto encstr  () const noexcept -> const encstr_t*;
		auto opcode  () const noexcept -> uint8_t;
	};


    class vm
    {
    			
    	int32_t offset;
    	uint8_t buffer[];

    	auto next()  noexcept -> void
    	{
    		token _token{ &this->buffer[this->offset] };
			auto opcode = _token.opcode();
    		if (opcode >= 0x00 && opcode <= 0x2A)
    		{
				this->dispense(_token.uint8x2());
    		}
    		else if(opcode >= 0x2B && opcode <= 0x32)
    		{
				this->dispense(_token.uint8str());
    		}
    		else if(opcode >= 0x33 && opcode <= 0x4E)
    		{
    			this->dispense(_token.string());
    		}
    		else if(opcode >= 0x4F && opcode <= 0x51)
    		{
    			this->dispense(_token.encstr());
    		}
    		else if(opcode >= 0x52 && opcode <= 0xFF)
    		{
    			this->dispense(_token.uint16x4());
    		}
    	}

		auto dispense(token::uint8x2_t* token) -> void
		{
			switch (token->op)
			{
				case 0x00:
				case 0x02:
				case 0x03:
				case 0x04:
				case 0x05:
				case 0x07:
				case 0x08:
				case 0x09:
				case 0x0A:
				case 0x0B:
				case 0x0E:
				case 0x0F:
				case 0x10:
				case 0x12:
				case 0x13:
				case 0x14:
				case 0x15:
				case 0x16:
				case 0x18:
				case 0x1A:
				case 0x1B:
				case 0x1C:
				case 0x1E:
				case 0x20:
				case 0x22:
				case 0x26:
				case 0x27:

				default:
				{
					break;
				}
			}
			this->offset += 3;
		}

		auto dispense(token::uint8str_t* token) -> void
		{
			auto len = std::strlen(token->str);
			switch (token->op)
			{
				case 0x2C:
				case 0x2D:
				case 0x2F:
				case 0x30:
				case 0x32:
				default:
				{
					break;
				}
			}
			this->offset +=  len + 3;
		}
		
		auto dispense(token::string_t* token) -> void
		{
			auto len = std::strlen(token->str);
			switch (token->op)
			{
				case 0x34:
				case 0x35:
				case 0x36:
				case 0x37:
				case 0x38:
				case 0x39:
				case 0x3A:
				case 0x3B:
				case 0x3C:
				case 0x3E:
				case 0x3F:
				case 0x42:
				case 0x43:
				case 0x44:
				case 0x45:
				case 0x4A:
				case 0x4C:
					return this->set_data_version(token->str);
				case 0x4D:
				default:
				{
					break;
				}
			}
			this->offset +=  len + 2;
		}
		
		auto dispense(token::encstr_t* token) -> void
		{
			auto len = std::strlen(token->str);
			switch (token->op)
			{
				case 0x4F:
				case 0x50:
				case 0x51:
				default:
				{
					break;
				}
			}
			this->offset +=  len + 2;
		}
		
		auto dispense(token::uint16x4_t* token) -> void
		{
			switch (token->op)
			{
				case 0x53:
				case 0x54:
				case 0x55:
				case 0x56:
				case 0x57:
				case 0x59:
				case 0x5A:
				case 0x5B:
				case 0x5D:
				case 0x5E:
				case 0x5F:
				case 0x60:
				case 0x61:
				case 0x62:
				case 0x63:
				case 0x64:
				case 0x66:
				case 0x67:
				case 0x68:
				case 0x69:
				case 0x6B:
				case 0x6C:
				case 0x6D:
				case 0x6E:
				case 0x6F:
				default:
				{
					break;
				}
			}
			this->offset += 9
		}

		// opcode: 0x4C
		auto set_data_version(const char* version) -> void
		{
			/**
			窗口标题栏右键 -> [バージョン情報]
						↓
			D.C.Ⅲ P.P. ～ダ・カーポⅢ プラチナパートナー～  Ver. 1.00
					データVer. {string}
			*/
		}
    };

}
