#include <collector/channel_client.h>
#include <collector/frontend_controller.h>
#include <collector/entry.h>

#include "mocks.h"

#include <common/stopwatch.h>
#include <test-helpers/helpers.h>

#include <ut/assert.h>
#include <ut/test.h>

#pragma warning(disable:4965)

using namespace std;
using namespace wpl::mt;

namespace micro_profiler
{
	namespace tests
	{
		namespace
		{
			int dummy = 0;

			void RaiseAt(event_flag *e, volatile long *times_to_event)
			{
				if (0 == _InterlockedDecrement(times_to_event))
					e->raise();
			}

			void ValidateThread(shared_ptr<running_thread> *hthread, event_flag *second_initialized, bool *first_finished)
			{
				shared_ptr<running_thread> previous_hthread = *hthread;

				*hthread = this_thread::open();
				if (previous_hthread)
				{
					*first_finished = !previous_hthread->is_running();
					second_initialized->raise();
				}
			}

			void LogThread1(shared_ptr<running_thread> *hthread, event_flag *initialized)
			{
				*hthread = this_thread::open();
				initialized->raise();
			}

			void LogThreadN(vector< shared_ptr<running_thread> > *log, volatile long *times_to_initialized,
				event_flag *initialized)
			{
				log->push_back(this_thread::open());
				RaiseAt(initialized, times_to_initialized);
			}

			void ControlInitialization(event_flag *proceed, vector< shared_ptr<running_thread> > *log,
				volatile long *times_to_initialized, event_flag *initialized)
			{
				proceed->wait();
				LogThreadN(log, times_to_initialized, initialized);
			}

			handle *profile_this(frontend_controller &fc)
			{	return fc.profile(&dummy);	}


			class sync_stop_frontend_controller : public frontend_controller
			{
			public:
				sync_stop_frontend_controller(calls_collector_i &collector, mocks::FrontendState &state)
					: frontend_controller(collector, state.MakeFactory())
				{	}

				~sync_stop_frontend_controller()
				{	force_stop();	}
			};


			class auto_frontend_controller : public sync_stop_frontend_controller
			{
			public:
				auto_frontend_controller(calls_collector_i &collector, mocks::FrontendState &state)
					: sync_stop_frontend_controller(collector, state), _profiler_handle(profile(&dummy))
				{	}

			private:
				const auto_ptr<handle> _profiler_handle;
			};


			class scoped_thread_join : wpl::noncopyable
			{
			public:
				scoped_thread_join(shared_ptr<running_thread> &thread_)
					: _thread(thread_)
				{	}

				~scoped_thread_join()
				{	_thread->join();	}

			private:
				shared_ptr<running_thread> &_thread;
			};
		}

		begin_test_suite( FrontendControllerTests )
			test( FactoryIsNotCalledIfCollectionHandleWasNotObtained )
			{
				// INIT
				mocks::Tracer tracer;
				event_flag initialized(false, true);
				mocks::FrontendState state(bind(&event_flag::raise, &initialized));

				// ACT
				{	frontend_controller fc(tracer, state.MakeFactory());	}

				// ASSERT
				assert_equal(waitable::timeout, initialized.wait(0));
			}


			test( ForcedStopIsAllowedOnStartedController )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				frontend_controller fc(tracer, state.MakeFactory());

				// ACT / ASSERT (must not throw)
				fc.force_stop();
			}


			test( NonNullHandleObtained )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				sync_stop_frontend_controller fc(tracer, state);

				// ACT
				auto_ptr<handle> h(profile_this(fc));
				
				// ASSERT
				assert_not_null(h.get());
			}


			test( FrontendIsCreatedInASeparateThreadWhenProfilerHandleObtained )
			{
				// INIT
				mocks::Tracer tracer;
				shared_ptr<running_thread> hthread;
				event_flag initialized(false, true);
				mocks::FrontendState state(bind(&LogThread1, &hthread, &initialized));
				scoped_thread_join stj(hthread);
				frontend_controller fc(tracer, state.MakeFactory());

				// ACT
				auto_ptr<handle> h(profile_this(fc));

				// ASSERT
				initialized.wait();

				assert_not_null(hthread);
				assert_not_equal(this_thread::open()->get_id(), hthread->get_id());
			}


			test( TwoCoexistingFrontendsHasDifferentWorkerThreads )
			{
				// INIT
				mocks::Tracer tracer1, tracer2;
				shared_ptr<running_thread> hthread1, hthread2;
				event_flag initialized1(false, true), initialized2(false, true);
				mocks::FrontendState state1(bind(&LogThread1, &hthread1, &initialized1)),
					state2(bind(&LogThread1, &hthread2, &initialized2));
				auto_frontend_controller fc1(tracer1, state1);
				auto_frontend_controller fc2(tracer2, state2);

				// ACT
				initialized1.wait();
				initialized2.wait();

				// ASSERT
				assert_not_equal(hthread1->get_id(), hthread2->get_id());
			}


			test( FrontendStopsImmediatelyAtForceStopRequest )
			{
				// INIT
				mocks::Tracer tracer(11);
				shared_ptr<running_thread> hthread;
				event_flag initialized(false, true);
				mocks::FrontendState state(bind(&LogThread1, &hthread, &initialized));
				frontend_controller fc(tracer, state.MakeFactory());
				auto_ptr<handle> h(profile_this(fc));

				initialized.wait();

				// ACT
				fc.force_stop();

				// ASSERT
				assert_is_false(hthread->is_running());
			}


			test( FrontendIsRecreatedOnRepeatedHandleObtaining )
			{
				// INIT
				mocks::Tracer tracer;
				event_flag initialized(false, true);
				volatile long counter = 2;
				mocks::FrontendState state(bind(&RaiseAt, &initialized, &counter));
				sync_stop_frontend_controller fc(tracer, state);
				auto_ptr<handle> h;

				// ACT
				h.reset(profile_this(fc));
				h.reset();
				h.reset(profile_this(fc));

				// ASSERT
				initialized.wait();

				// INIT
				counter = 3;

				// ACT
				h.reset();
				h.reset(profile_this(fc));
				h.reset();
				h.reset(profile_this(fc));
				h.reset();
				h.reset(profile_this(fc));

				// ASSERT
				initialized.wait();
			}


			test( FrontendCreationIsRefCounted2 )
			{
				// INIT
				mocks::Tracer tracer;
				event_flag initialized(false, true);
				volatile long counter = 1;
				mocks::FrontendState state(bind(&RaiseAt, &initialized, &counter));
				frontend_controller fc(tracer, state.MakeFactory());

				// ACT
				auto_ptr<handle> h1(profile_this(fc));
				auto_ptr<handle> h2(profile_this(fc));

				initialized.wait();

				h1.reset();
				h2.reset();
				fc.force_stop();

				// ASSERT
				assert_equal(0, counter);
			}


			test( FrontendCreationIsRefCounted3 )
			{
				// INIT
				mocks::Tracer tracer;
				event_flag initialized(false, true);
				volatile long counter = 1;
				mocks::FrontendState state(bind(&RaiseAt, &initialized, &counter));
				frontend_controller fc(tracer, state.MakeFactory());

				// ACT
				auto_ptr<handle> h1(profile_this(fc));
				auto_ptr<handle> h2(profile_this(fc));
				auto_ptr<handle> h3(profile_this(fc));

				initialized.wait();

				h3.reset();
				h2.reset();
				h1.reset();
				fc.force_stop();

				// ASSERT
				assert_equal(0, counter);
			}


			test( FirstThreadIsDeadByTheTimeOfTheSecondInitialization )
			{
				// INIT
				mocks::Tracer tracer;
				shared_ptr<running_thread> hthread;
				event_flag second_initialized(false, true);
				bool first_finished = false;
				mocks::FrontendState state(bind(&ValidateThread, &hthread, &second_initialized, &first_finished));
				scoped_thread_join stj(hthread);
				frontend_controller fc(tracer, state.MakeFactory());
				auto_ptr<handle> h;

				// ACT
				h.reset(profile_this(fc));
				h.reset();
				h.reset(profile_this(fc));
				second_initialized.wait();

				// ASSERT
				assert_is_true(first_finished);
			}


			test( AllWorkerThreadsMustExitOnHandlesClosure )
			{
				// INIT
				mocks::Tracer tracer;
				event_flag proceed(false, false), finished(false, true);
				vector< shared_ptr<running_thread> > htread_log;
				volatile long times = 2;
				mocks::FrontendState state(bind(&ControlInitialization, &proceed, &htread_log, &times, &finished));
				frontend_controller fc(tracer, state.MakeFactory());
				auto_ptr<handle> h;

				// ACT
				h.reset(profile_this(fc));
				h.reset();
				h.reset(profile_this(fc));
				h.reset();
				proceed.raise();
				finished.wait();	// wait for the second thread to initialize the frontend

				// ASSERT (must not hang)
				htread_log[1]->join();
			}


			test( ProfilerHandleReleaseIsNonBlockingAndFrontendThreadIsFinishedEventually )
			{
				// INIT
				mocks::Tracer tracer;
				shared_ptr<running_thread> hthread;
				event_flag initialized(false, true);
				mocks::FrontendState state(bind(&LogThread1, &hthread, &initialized));
				scoped_thread_join stj(hthread);
				frontend_controller fc(tracer, state.MakeFactory());
				auto_ptr<handle> h(profile_this(fc));

				initialized.wait();

				// ACT (must not hang)
				hthread->suspend();
				h.reset();
				hthread->resume();

				// ASSERT (must not hang)
				hthread->join();
			}


			test( FrontendInterfaceReleasedAtPFDestroyed )
			{
				// INIT
				mocks::Tracer tracer;
				shared_ptr<running_thread> hthread;
				event_flag initialized(false, true);
				mocks::FrontendState state(bind(&LogThread1, &hthread, &initialized));
				frontend_controller fc(tracer, state.MakeFactory());
				auto_ptr<handle> h(profile_this(fc));

				// ACT
				initialized.wait();
				h.reset();
				hthread->join();

				// ASERT
				assert_equal(0u, state.ref_count);
			}


			test( FrontendInitializedWithProcessIdAndTicksResolution )
			{
				// INIT
				mocks::Tracer tracer;
				event_flag initialized(false, true);
				mocks::FrontendState state(bind(&event_flag::raise, &initialized));
				auto_frontend_controller fc(tracer, state);

				// ACT
				initialized.wait();

				// ASERT
				assert_equal(get_current_process_executable(), state.process_init.executable);
				assert_equal(c_ticks_per_second, state.process_init.ticks_per_second);
			}


			test( FrontendIsNotBotheredWithEmptyDataUpdates )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				auto_frontend_controller fc(tracer, state);

				// ACT / ASSERT
				assert_equal(waitable::timeout, state.updated.wait(500));
			}


			test( ImageLoadEventArrivesAtHandleObtaining )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				image images[] = { image(L"symbol_container_1"), image(L"symbol_container_2"), };
				sync_stop_frontend_controller fc(tracer, state);

				// ACT
				auto_ptr<handle> h1(fc.profile(images[0].get_symbol_address("get_function_addresses_1")));
				state.modules_state_updated.wait();

				// ASSERT
				assert_equal(1u, state.update_log.size());
				assert_equal(1u, state.update_log[0].image_loads.size());
				assert_equal(images[0].load_address(), state.update_log[0].image_loads[0].load_address);
				assert_not_equal(wstring::npos, state.update_log[0].image_loads[0].path.find(L"symbol_container_1"));

				// ACT
				auto_ptr<handle> h2(fc.profile(images[1].get_symbol_address("get_function_addresses_2")));
				state.modules_state_updated.wait();

				// ASSERT
				assert_equal(2u, state.update_log.size());
				assert_equal(1u, state.update_log[1].image_loads.size());
				assert_equal(images[1].load_address(), state.update_log[1].image_loads[0].load_address);
				assert_not_equal(wstring::npos, state.update_log[1].image_loads[0].path.find(L"symbol_container_2"));
			}


			test( ImageUnloadEventArrivesAtHandleRelease )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				image images[] = { image(L"symbol_container_1"), image(L"symbol_container_2"), };
				sync_stop_frontend_controller fc(tracer, state);

				auto_ptr<handle> h1(fc.profile(images[0].get_symbol_address("get_function_addresses_1")));
				state.modules_state_updated.wait();
				auto_ptr<handle> h2(fc.profile(images[1].get_symbol_address("get_function_addresses_2")));
				state.modules_state_updated.wait();

				// ACT
				h1.reset();
				state.modules_state_updated.wait();

				// ASSERT
				assert_equal(3u, state.update_log.size());
				assert_equal(1u, state.update_log[2].image_unloads.size());
				assert_equal(images[0].load_address(), state.update_log[2].image_unloads[0]);

				// ACT
				h2.reset();
				state.modules_state_updated.wait();

				// ASSERT
				assert_equal(4u, state.update_log.size());
				assert_equal(1u, state.update_log[3].image_unloads.size());
				assert_equal(images[1].load_address(), state.update_log[3].image_unloads[0]);
			}


			test( LastBatchIsReportedToFrontend )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				sync_stop_frontend_controller fc(tracer, state);
				auto_ptr<handle> h(profile_this(fc));
				call_record trace1[] = { { 0, (void *)0x1223 }, { 1000, (void *)0 }, };
				call_record trace2[] = { { 0, (void *)0x12230 }, { 1000, (void *)0 }, };

				state.modules_state_updated.wait();

				// ACT
				state.update_lock.lower();	// suspend on before exiting next update
				tracer.Add(thread::id(), trace1);
				state.updated.wait();
				h.reset();
				tracer.Add(thread::id(), trace2);
				state.update_lock.raise();	// resume
				state.modules_state_updated.wait();

				// ASSERT
				assert_equal(4u, state.update_log.size());
				assert_equal(1u, state.update_log[0].image_loads.size());
				assert_not_equal(state.update_log[1].update.end(), state.update_log[1].update.find(0x1223));
				assert_not_equal(state.update_log[2].update.end(), state.update_log[2].update.find(0x12230));
				assert_equal(1u, state.update_log[3].image_unloads.size());
			}


			test( MakeACallAndWaitForDataPost )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				auto_frontend_controller fc(tracer, state);

				state.modules_state_updated.wait();
				state.update_log.clear();

				// ACT
				call_record trace1[] = {
					{	0, (void *)0x1223	},
					{	1000, (void *)0	},
				};

				tracer.Add(thread::id(), trace1);

				state.updated.wait();

				// ASERT
				assert_equal(1u, state.update_log.size());

				mocks::statistics_map_detailed::const_iterator callinfo_1 = state.update_log[0].update.find(0x1223);

				assert_not_equal(state.update_log[0].update.end(), callinfo_1);

				assert_equal(1u, callinfo_1->second.times_called);
				assert_equal(0u, callinfo_1->second.max_reentrance);
				assert_equal(1000, callinfo_1->second.inclusive_time);
				assert_equal(callinfo_1->second.inclusive_time, callinfo_1->second.exclusive_time);
				assert_equal(callinfo_1->second.inclusive_time, callinfo_1->second.max_call_time);

				// ACT
				call_record trace2[] = {
					{	10000, (void *)0x31223	},
					{	14000, (void *)0	},
				};

				tracer.Add(thread::id(), trace2);

				state.updated.wait();

				// ASERT
				assert_equal(2u, state.update_log.size());

				assert_equal(1u, state.update_log[1].update.size());	// The new batch MUST NOT not contain previous function.

				mocks::statistics_map_detailed::const_iterator callinfo_2 = state.update_log[1].update.find(0x31223);

				assert_not_equal(state.update_log[1].update.end(), callinfo_2);

				assert_equal(1u, callinfo_2->second.times_called);
				assert_equal(0u, callinfo_2->second.max_reentrance);
				assert_equal(4000, callinfo_2->second.inclusive_time);
				assert_equal(callinfo_2->second.inclusive_time, callinfo_2->second.exclusive_time);
				assert_equal(callinfo_2->second.inclusive_time, callinfo_2->second.max_call_time);
			}


			test( SecondProfilerInstanceIsWorkable )
			{
				// INIT
				mocks::Tracer tracer;
				event_flag initialized(false, true);
				volatile long times = 2;
				mocks::FrontendState state(bind(&RaiseAt, &initialized, &times));
				sync_stop_frontend_controller fc(tracer, state);
				auto_ptr<handle> h(profile_this(fc));
				call_record trace[] = {
					{	10000, (void *)0x31223	},
					{	14000, (void *)0	},
				};

				h.reset();
				h.reset(profile_this(fc));
				initialized.wait();

				// ACT
				tracer.Add(thread::id(), trace);

				// ASSERT (must not hang)
				state.updated.wait();

				// ACT (duplicated intentionally - flush-at-exit events may count for the first wait)
				tracer.Add(thread::id(), trace);

				// ASSERT (must not hang)
				state.updated.wait();
			}


			test( PassReentranceCountToFrontend )
			{
				// INIT
				mocks::Tracer tracer;
				mocks::FrontendState state;
				auto_frontend_controller fc(tracer, state);

				state.modules_state_updated.wait();
				state.update_log.clear();

				// ACT
				call_record trace1[] = {
					{	1, (void *)0x31000	},
						{	2, (void *)0x31000	},
							{	3, (void *)0x31000	},
								{	4, (void *)0x31000	},
								{	5, (void *)0	},
							{	6, (void *)0	},
						{	7, (void *)0	},
					{	8, (void *)0	},
					{	9, (void *)0x31000	},
						{	10, (void *)0x31000	},
							{	11, (void *)0x31000	},
							{	12, (void *)0	},
						{	13, (void *)0	},
					{	14, (void *)0	},
				};

				tracer.Add(thread::id(), trace1);

				state.updated.wait();

				// ASERT
				assert_equal(1u, state.update_log.size());

				const mocks::function_statistics_detailed callinfo_1 = state.update_log[0].update[0x31000];

				assert_equal(7u, callinfo_1.times_called);
				assert_equal(3u, callinfo_1.max_reentrance);

				// ACT
				call_record trace2[] = {
					{	1, (void *)0x31000	},
						{	2, (void *)0x31000	},
							{	3, (void *)0x31000	},
								{	4, (void *)0x31000	},
									{	5, (void *)0x31000	},
									{	6, (void *)0	},
								{	7, (void *)0	},
							{	8, (void *)0	},
						{	9, (void *)0	},
					{	10, (void *)0	},
				};

				tracer.Add(thread::id(), trace2);

				state.updated.wait();

				// ASERT
				assert_equal(2u, state.update_log.size());

				const mocks::function_statistics_detailed &callinfo_2 = state.update_log[1].update[0x31000];

				assert_equal(5u, callinfo_2.times_called);
				assert_equal(4u, callinfo_2.max_reentrance);
			}
			

			test( PerformanceDataTakesProfilerLatencyIntoAccount )
			{
				// INIT
				mocks::Tracer tracer1(13), tracer2(29);
				mocks::FrontendState state1, state2;

				auto_frontend_controller fe1(tracer1, state1);
				auto_frontend_controller fe2(tracer2, state2);

				state1.modules_state_updated.wait();
				state1.update_log.clear();
				state2.modules_state_updated.wait();
				state2.update_log.clear();

				// ACT
				call_record trace[] = {
					{	10000, (void *)0x3171717	},
					{	14000, (void *)0	},
				};

				tracer1.Add(thread::id(), trace);
				state1.updated.wait();

				tracer2.Add(thread::id(), trace);
				state2.updated.wait();

				// ASSERT
				assert_equal(4000 - 13, state1.update_log[0].update[0x3171717].inclusive_time);
				assert_equal(4000 - 29, state2.update_log[0].update[0x3171717].inclusive_time);
			}


			test( ChildrenStatisticsIsPassedAlongWithTopLevels )
			{
				// INIT
				mocks::Tracer tracer(11);
				mocks::FrontendState state;
				auto_frontend_controller fc(tracer, state);

				state.modules_state_updated.wait();
				state.update_log.clear();

				// ACT
				call_record trace[] = {
					{	1, (void *)0x31000	},
						{	20, (void *)0x37000	},
						{	50, (void *)0	},
						{	51, (void *)0x41000	},
						{	70, (void *)0	},
						{	72, (void *)0x37000	},
						{	90, (void *)0	},
					{	120, (void *)0	},
					{	1000, (void *)0x11000	},
						{	1010, (void *)0x13000	},
						{	1100, (void *)0	},
					{	1400, (void *)0	},
					{	1420, (void *)0x13000	},
					{	1490, (void *)0	},
				};

				tracer.Add(thread::id(), trace);

				state.updated.wait();

				// ASSERT
				const mocks::function_statistics_detailed &callinfo_parent1 = state.update_log[0].update[0x31000];
				const mocks::function_statistics_detailed &callinfo_parent2 = state.update_log[0].update[0x11000];
				const mocks::function_statistics_detailed &callinfo_parent3 = state.update_log[0].update[0x13000];

				assert_equal(2u, callinfo_parent1.callees.size());
				assert_equal(108, callinfo_parent1.inclusive_time);
				assert_equal(8, callinfo_parent1.exclusive_time);
				
				assert_equal(1u, callinfo_parent2.callees.size());
				assert_equal(389, callinfo_parent2.inclusive_time);
				assert_equal(288, callinfo_parent2.exclusive_time);

				assert_equal(0u, callinfo_parent3.callees.size());
				assert_equal(138, callinfo_parent3.inclusive_time);
				assert_equal(138, callinfo_parent3.exclusive_time);


				const function_statistics &callinfo_child11 = state.update_log[0].update[0x31000].callees[0x37000];
				const function_statistics &callinfo_child12 = state.update_log[0].update[0x31000].callees[0x41000];

				assert_equal(2u, callinfo_child11.times_called);
				assert_equal(26, callinfo_child11.inclusive_time);
				assert_equal(26, callinfo_child11.exclusive_time);

				assert_equal(1u, callinfo_child12.times_called);
				assert_equal(8, callinfo_child12.inclusive_time);
				assert_equal(8, callinfo_child12.exclusive_time);
			}


			test( FrontendContinuesAfterControllerDestructionUntilAfterHandleIsReleased )
			{
				// INIT
				mocks::Tracer tracer(11);
				shared_ptr<running_thread> hthread;
				event_flag initialized(false, true);
				mocks::FrontendState state(bind(&LogThread1, &hthread, &initialized));
				scoped_thread_join stj(hthread);
				auto_ptr<frontend_controller> fc(new frontend_controller(tracer, state.MakeFactory()));
				auto_ptr<handle> h(profile_this(*fc));
				call_record trace[] = {
					{	10000, (void *)0x31223	},
					{	14000, (void *)0	},
				};

				tracer.Add(thread::id(), trace);
				state.updated.wait();

				// ACT / ASSERT (must not hang)
				fc.reset();

				// ACT
				tracer.Add(thread::id(), trace);
				
				// ASSERT
				state.updated.wait();

				// ACT (duplicated intentionally - flush-at-exit events may count for the first wait)
				tracer.Add(thread::id(), trace);

				// ASSERT (must not hang)
				state.updated.wait();
			}
		end_test_suite
	}
}
