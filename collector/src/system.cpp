//	Copyright (c) 2011-2018 by Artem A. Gevorkyan (gevorkyan.org)
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.

#include <collector/system.h>

#include <intrin.h>
#include <memory>
#include <windows.h>

#pragma intrinsic(__rdtsc)

namespace micro_profiler
{
	timestamp_t ticks_per_second()
	{
		LARGE_INTEGER pc_freq, pc_start, pc_end;
		timestamp_t tsc_start, tsc_end;

		::QueryPerformanceFrequency(&pc_freq);
		::QueryPerformanceCounter(&pc_start);
		tsc_start = __rdtsc();
		for (volatile int i = 0; i < 10000000; ++i)
		{	}
		tsc_end = __rdtsc();
		::QueryPerformanceCounter(&pc_end);
		return pc_freq.QuadPart * (tsc_end - tsc_start) / (pc_end.QuadPart - pc_start.QuadPart);
	}

	unsigned int current_thread_id()
	{	return ::GetCurrentThreadId();	}


	mutex::mutex()
	{
		typedef char static_size_assertion[sizeof(CRITICAL_SECTION) <= sizeof(_mtx_buffer)];

		::InitializeCriticalSection(static_cast<CRITICAL_SECTION *>(static_cast<void*>(_mtx_buffer)));
	}

	mutex::~mutex()
	{	::DeleteCriticalSection(static_cast<CRITICAL_SECTION *>(static_cast<void*>(_mtx_buffer)));	}

	void mutex::lock()
	{	::EnterCriticalSection(static_cast<CRITICAL_SECTION *>(static_cast<void*>(_mtx_buffer)));	}

	void mutex::unlock()
	{	::LeaveCriticalSection(static_cast<CRITICAL_SECTION *>(static_cast<void*>(_mtx_buffer)));	}


   long interlocked_compare_exchange(long volatile *destination, long exchange, long comperand)
   {  return _InterlockedCompareExchange(destination, exchange, comperand);  }

   long long interlocked_compare_exchange64(long long volatile *destination, long long exchange, long long comperand)
   {  return _InterlockedCompareExchange64(destination, exchange, comperand);  }
}
