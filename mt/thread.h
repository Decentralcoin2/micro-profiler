#pragma once

#if defined(MP_MT_GENERIC)
	#include <thread>

	namespace mt
	{
		using std::thread;
		using std::chrono::milliseconds;

		namespace this_thread
		{
			using namespace std::this_thread;
		}
	}
#else
	#include <wpl/mt/thread.h>

	namespace mt
	{
		using wpl::mt::thread;
		typedef unsigned int milliseconds;

		namespace this_thread
		{
			thread::id get_id();
			void sleep_for(milliseconds period);
		}
	}
#endif