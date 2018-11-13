#include <collector/binary_translation.h>

extern "C" {
	#include <ld32.h>
}

namespace micro_profiler
{
	namespace
	{
		typedef unsigned int dword;

		bool is_target_address_8_outside(const byte *relative, const_byte_range source)
		{
			relative += 1 + static_cast<char>(*relative);
			return (relative < source.begin()) | (relative >= source.end());
		}

		bool is_target_address_32_outside(const byte *relative, const_byte_range source)
		{
			relative += 4 + *reinterpret_cast<const dword *>(relative);
			return (relative < source.begin()) | (relative >= source.end());
		}
	}

	inconsistent_function_range_exception::inconsistent_function_range_exception()
		: runtime_error("")
	{	}

	size_t calculate_function_length(const_byte_range source, size_t min_length)
	{
		size_t result = 0, l = 0;
		const byte *ptr = source.begin();

		for (bool end = false; !end; end = min_length <= l, min_length -= l, result += l, ptr += l)
			l = length_disasm((void *)ptr);
		return result;
	}

	void move_function(byte *destination, const byte *source_base, const_byte_range source_)
	{
		const byte *source = source_.begin();
		size_t size = source_.length();
		const ptrdiff_t delta = source_base - destination;

		for (size_t l = 0; size; destination += l, source += l, size -= l)
		{
			switch (*source)
			{
			default:
				l = length_disasm(const_cast<byte *>(source));
				memcpy(destination, source, l);
				break;

			case 0xEB:
				if (is_target_address_8_outside(source + 1, source_))
					throw inconsistent_function_range_exception();
				*destination = *source, *(destination + 1) = *(source + 1);
				l = 2;
				break;

			case 0xE9:
			case 0xE8:
				*destination = *source;
				*reinterpret_cast<dword *>(destination + 1) = *reinterpret_cast<const dword *>(source + 1)
					+ (is_target_address_32_outside(source + 1, source_) ? delta : 0);
				l = 5;
				break;
			}
		}
	}
}