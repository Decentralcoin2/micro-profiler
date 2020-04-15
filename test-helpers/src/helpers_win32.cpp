#include <test-helpers/helpers.h>

#include <ut/assert.h>
#include <windows.h>

using namespace std;

namespace micro_profiler
{
	namespace tests
	{
		string get_current_process_executable()
		{
			char fullpath[MAX_PATH + 1] = { };

			::GetModuleFileNameA(NULL, fullpath, MAX_PATH);
			return fullpath;
		}


		image::image(string path)
			: shared_ptr<void>(::LoadLibraryA(path.c_str()), &::FreeLibrary)
		{
			if (!get())
				throw runtime_error("Cannot load module specified!");

			char fullpath[MAX_PATH + 1] = { };

			::GetModuleFileNameA(static_cast<HMODULE>(get()), fullpath, MAX_PATH);
			_fullpath = fullpath;
		}

		byte *image::base_ptr() const
		{	return static_cast<byte *>(get());	}

		long_address_t image::base() const
		{	return reinterpret_cast<size_t>(get());	}

		const char *image::absolute_path() const
		{	return _fullpath.c_str();	}

		void *image::get_symbol_address(const char *name) const
		{
			if (void *symbol = ::GetProcAddress(static_cast<HMODULE>(get()), name))
				return symbol;
			throw runtime_error("Symbol specified was not found!");
		}

		unsigned image::get_symbol_rva(const char *name) const
		{
			if (void *symbol = ::GetProcAddress(static_cast<HMODULE>(get()), name))
				return static_cast<unsigned>(static_cast<byte *>(symbol) - base_ptr());
			throw runtime_error("Symbol specified was not found!");
		}

		shared_ptr<void> occupy_memory(void *start, unsigned int length)
		{
			return shared_ptr<void>(::VirtualAlloc(start, length, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY | PAGE_GUARD),
				[length] (void *p) {
				::VirtualFree(p, 0, MEM_RELEASE);
			});
		}
	}
}

extern "C" int setenv(const char *name, const char *value, int overwrite)
{
	if (overwrite || !GetEnvironmentVariableA(name, NULL, 0))
		::SetEnvironmentVariableA(name, value);
	return 0;
}
