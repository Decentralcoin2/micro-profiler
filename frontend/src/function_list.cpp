//	Copyright (c) 2011-2018 by Artem A. Gevorkyan (gevorkyan.org) and Denis Burenko
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

#include <frontend/function_list.h>

#include <common/formatting.h>

#include <utility>
#include <cmath>
#include <clocale>

using namespace std;
using namespace placeholders;
using namespace wpl;
using namespace wpl::ui;

#pragma warning(disable: 4775)

namespace micro_profiler
{
	namespace
	{
		const address_t c_invalid_address = numeric_limits<address_t>::max();

		wstring to_string2(count_t value)
		{
			const size_t buffer_size = 24;
			wchar_t buffer[buffer_size] = { };

			::swprintf(buffer, buffer_size, L"%I64u", value);
			return buffer;
		}

		wstring to_string2(unsigned int value)
		{
			const size_t buffer_size = 24;
			wchar_t buffer[buffer_size] = { };

			::swprintf(buffer, buffer_size, L"%u", value);
			return buffer;
		}

		wstring to_string2(double value)
		{
			const size_t buffer_size = 24;
			wchar_t buffer[buffer_size] = { };

			::swprintf(buffer, buffer_size, L"%g", value);
			return buffer;
		}

		class by_name
		{
		public:
			by_name(shared_ptr<symbol_resolver> resolver)
				: _resolver(resolver)
			{	}

			template <typename AnyT>
			bool operator ()(address_t lhs_addr, const AnyT &, address_t rhs_addr, const AnyT &) const
			{
				symbol_resolver::symbol_t symbol_lhs, symbol_rhs;

				_resolver->get_symbol(lhs_addr, symbol_lhs), _resolver->get_symbol(rhs_addr, symbol_rhs);
				return symbol_lhs.name < symbol_rhs.name;
			}

		private:
			shared_ptr<symbol_resolver> _resolver;
		};

		struct by_times_called
		{
			bool operator ()(address_t, const function_statistics &lhs, address_t, const function_statistics &rhs) const
			{	return lhs.times_called < rhs.times_called;	}

			bool operator ()(address_t, count_t lhs, address_t, count_t rhs) const
			{	return lhs < rhs;	}
		};

		struct by_exclusive_time
		{
			bool operator ()(address_t, const function_statistics &lhs, address_t, const function_statistics &rhs) const
			{	return lhs.exclusive_time < rhs.exclusive_time;	}
		};

		struct by_inclusive_time
		{
			bool operator ()(address_t, const function_statistics &lhs, address_t, const function_statistics &rhs) const
			{	return lhs.inclusive_time < rhs.inclusive_time;	}
		};

		struct by_avg_exclusive_call_time
		{
			bool operator ()(address_t, const function_statistics &lhs, address_t, const function_statistics &rhs) const
			{	return lhs.times_called && rhs.times_called ? lhs.exclusive_time * rhs.times_called < rhs.exclusive_time * lhs.times_called : lhs.times_called < rhs.times_called;	}
		};

		struct by_avg_inclusive_call_time
		{
			bool operator ()(address_t, const function_statistics &lhs, address_t, const function_statistics &rhs) const
			{	return lhs.times_called && rhs.times_called ? lhs.inclusive_time * rhs.times_called < rhs.inclusive_time * lhs.times_called : lhs.times_called < rhs.times_called;	}
		};

		struct by_max_reentrance
		{
			bool operator ()(address_t, const function_statistics &lhs, address_t, const function_statistics &rhs) const
			{	return lhs.max_reentrance < rhs.max_reentrance;	}
		};

		struct by_max_call_time
		{
			bool operator ()(address_t, const function_statistics &lhs, address_t, const function_statistics &rhs) const
			{	return lhs.max_call_time < rhs.max_call_time;	}
		};

		struct get_times_called
		{
			double operator ()(const function_statistics &value) const
			{	return static_cast<double>(value.times_called);	}
		};

		struct exclusive_time
		{
			exclusive_time(double inv_tick_count)
				: _inv_tick_count(inv_tick_count)
			{	}

			double operator ()(const function_statistics &value) const
			{	return _inv_tick_count * value.exclusive_time;	}

			double _inv_tick_count;
		};

		struct inclusive_time
		{
			inclusive_time(double inv_tick_count)
				: _inv_tick_count(inv_tick_count)
			{	}

			double operator ()(const function_statistics &value) const
			{	return _inv_tick_count * value.inclusive_time;	}

			double _inv_tick_count;
		};

		struct exclusive_time_avg
		{
			exclusive_time_avg(double inv_tick_count)
				: _inv_tick_count(inv_tick_count)
			{	}

			double operator ()(const function_statistics &value) const
			{	return value.times_called ? _inv_tick_count * value.exclusive_time / value.times_called : 0.0;	}

			double _inv_tick_count;
		};

		struct inclusive_time_avg
		{
			inclusive_time_avg(double inv_tick_count)
				: _inv_tick_count(inv_tick_count)
			{	}

			double operator ()(const function_statistics &value) const
			{	return value.times_called ? _inv_tick_count * value.inclusive_time / value.times_called : 0.0;	}

			double _inv_tick_count;
		};

		struct max_call_time
		{
			max_call_time(double inv_tick_count)
				: _inv_tick_count(inv_tick_count)
			{	}

			double operator ()(const function_statistics &value) const
			{	return _inv_tick_count * value.max_call_time;	}

			double _inv_tick_count;
		};
	}


	template <typename MapT>
	class linked_statistics_model_impl : public statistics_model_impl<linked_statistics, MapT>
	{
	public:
		linked_statistics_model_impl(const MapT &statistics, signal<void (address_t entry)> &entry_updated,
			signal<void ()> &master_cleared, double tick_interval, shared_ptr<symbol_resolver> resolver);

	protected:
		typedef statistics_model_impl<linked_statistics, MapT> base;

	private:
		virtual void on_updated(address_t address);

	private:
		slot_connection _updates_connection;
		slot_connection _cleared_connection;
	};


	class children_statistics_model_impl : public linked_statistics_model_impl<statistics_map>
	{
	public:
		children_statistics_model_impl(address_t controlled_address, const statistics_map &statistics,
			signal<void (address_t)> &entry_updated, signal<void ()> &master_cleared, double tick_interval,
			shared_ptr<symbol_resolver> resolver);

	private:
		virtual void on_updated(address_t address);

	private:
		address_t _controlled_address;
	};

	typedef linked_statistics_model_impl<statistics_map_callers> parents_statistics;



	template <typename BaseT, typename MapT>
	void statistics_model_impl<BaseT, MapT>::get_text(index_type item, index_type subitem, wstring &text) const
	{
		symbol_resolver::symbol_t symbol;
		const view_type::value_type &row = get_entry(item);

		switch (subitem)
		{
		case 0:	text = to_string2(item + 1);	break;
		case 1:	_resolver->get_symbol(row.first, symbol), text = symbol.name;	break;
		case 2:	text = to_string2(row.second.times_called);	break;
		case 3:	format_interval(text, exclusive_time(_tick_interval)(row.second));	break;
		case 4:	format_interval(text, inclusive_time(_tick_interval)(row.second));	break;
		case 5:	format_interval(text, exclusive_time_avg(_tick_interval)(row.second));	break;
		case 6:	format_interval(text, inclusive_time_avg(_tick_interval)(row.second));	break;
		case 7:	text = to_string2(row.second.max_reentrance);	break;
		case 8:	format_interval(text, max_call_time(_tick_interval)(row.second));	break;
		}
	}

	template <typename BaseT, typename MapT>
	void statistics_model_impl<BaseT, MapT>::set_order(index_type column, bool ascending)
	{
		switch (column)
		{
		case 0:
			_view->disable_projection();
			break;

		case 1:
			_view->set_order(by_name(_resolver), ascending);
			_view->disable_projection();
			break;

		case 2:
			_view->set_order(by_times_called(), ascending);
			_view->project_value(get_times_called());
			break;

		case 3:
			_view->set_order(by_exclusive_time(), ascending);
			_view->project_value(exclusive_time(_tick_interval));
			break;

		case 4:
			_view->set_order(by_inclusive_time(), ascending);
			_view->project_value(inclusive_time(_tick_interval));
			break;

		case 5:
			_view->set_order(by_avg_exclusive_call_time(), ascending);
			_view->project_value(exclusive_time_avg(_tick_interval));
			break;

		case 6:
			_view->set_order(by_avg_inclusive_call_time(), ascending);
			_view->project_value(inclusive_time_avg(_tick_interval));
			break;

		case 7:
			_view->set_order(by_max_reentrance(), ascending);
			_view->disable_projection();
			break;

		case 8:
			_view->set_order(by_max_call_time(), ascending);
			_view->project_value(max_call_time(_tick_interval));
			break;
		}
		on_updated();
	}


	functions_list::functions_list(shared_ptr<statistics_map_detailed> statistics, double tick_interval,
			shared_ptr<symbol_resolver> resolver)
		: statistics_model_impl<table_model, statistics_map_detailed>(*statistics, tick_interval, resolver),
			_statistics(statistics), _tick_interval(tick_interval), _resolver(resolver)
	{	}

	void functions_list::clear()
	{
		_cleared();
		_statistics->clear();
		on_updated();
	}

	void functions_list::print(wstring &content) const
	{
		const char* old_locale = ::setlocale(LC_NUMERIC, NULL);  
		bool locale_ok = ::setlocale(LC_NUMERIC, "") != NULL;  

		content.clear();
		content.reserve(256 * (get_count() + 1)); // kind of magic number
		content += L"Function\tTimes Called\tExclusive Time\tInclusive Time\tAverage Call Time (Exclusive)\tAverage Call Time (Inclusive)\tMax Recursion\tMax Call Time\r\n";
		for (index_type i = 0; i != get_count(); ++i)
		{
			symbol_resolver::symbol_t symbol;
			const view_type::value_type &row = get_entry(i);

			_resolver->get_symbol(row.first, symbol);
			content += symbol.name + L"\t";
			content += to_string2(row.second.times_called) + L"\t";
			content += to_string2(exclusive_time(_tick_interval)(row.second)) + L"\t";
			content += to_string2(inclusive_time(_tick_interval)(row.second)) + L"\t";
			content += to_string2(exclusive_time_avg(_tick_interval)(row.second)) + L"\t";
			content += to_string2(inclusive_time_avg(_tick_interval)(row.second)) + L"\t";
			content += to_string2(row.second.max_reentrance) + L"\t";
			content += to_string2(max_call_time(_tick_interval)(row.second)) + L"\r\n";
		}

		if (locale_ok)
			::setlocale(LC_NUMERIC, old_locale);
	}

	shared_ptr<linked_statistics> functions_list::watch_children(index_type item) const
	{
		if (item >= get_count())
			throw out_of_range("");

		const statistics_map_detailed::value_type &s = get_entry(item);

		return shared_ptr<linked_statistics>(new children_statistics_model_impl(s.first, s.second.callees,
			_statistics->entry_updated, _cleared, _tick_interval, _resolver));
	}

	shared_ptr<linked_statistics> functions_list::watch_parents(index_type item) const
	{
		if (item >= get_count())
			throw out_of_range("");

		const statistics_map_detailed::value_type &s = get_entry(item);

		return shared_ptr<linked_statistics>(new parents_statistics(s.second.callers, _statistics->entry_updated,
			_cleared, _tick_interval, _resolver));
	}

	std::shared_ptr<symbol_resolver> functions_list::get_resolver() const
	{	return _resolver;	}

	
	template <typename MapT>
	linked_statistics_model_impl<MapT>::linked_statistics_model_impl(const MapT &statistics,
			signal<void (address_t)> &entry_updated, signal<void ()> &master_cleared, double tick_interval,
			shared_ptr<symbol_resolver> resolver)
		: statistics_model_impl<linked_statistics, MapT>(statistics, tick_interval, resolver)
	{
		_updates_connection = entry_updated += bind(&linked_statistics_model_impl::on_updated, this, _1);
		_cleared_connection = master_cleared += bind(&linked_statistics_model_impl::detach, this);
	}

	template <typename MapT>
	void linked_statistics_model_impl<MapT>::on_updated(address_t /*address*/)
	{	base::on_updated();	}


	children_statistics_model_impl::children_statistics_model_impl(address_t controlled_address,
			const statistics_map &statistics, signal<void (address_t)> &entry_updated,
			signal<void ()> &cleared, double tick_interval, shared_ptr<symbol_resolver> resolver)
		: linked_statistics_model_impl<statistics_map>(statistics, entry_updated, cleared, tick_interval, resolver),
			_controlled_address(controlled_address)
	{	}

	void children_statistics_model_impl::on_updated(address_t address)
	{
		if (_controlled_address == address)
			base::on_updated();
	}


	template <>
	void statistics_model_impl<linked_statistics, statistics_map_callers>::get_text(index_type item, index_type subitem,
		wstring &text) const
	{
		const statistics_map_callers::value_type &row = get_entry(item);
		symbol_resolver::symbol_t symbol;

		switch (subitem)
		{
		case 0:	text = to_string2(item + 1);	break;
		case 1:	_resolver->get_symbol(row.first, symbol), text = symbol.name;	break;
		case 2:	text = to_string2(row.second);	break;
		}
	}

	template <>
	void statistics_model_impl<linked_statistics, statistics_map_callers>::set_order(index_type column, bool ascending)
	{
		switch (column)
		{
		case 1:	_view->set_order(by_name(_resolver), ascending);	break;
		case 2:	_view->set_order(by_times_called(), ascending);	break;
		}
		this->invalidated(_view->size());
	}


	shared_ptr<functions_list> functions_list::create(timestamp_t ticks_per_second, shared_ptr<symbol_resolver> resolver)
	{
		return shared_ptr<functions_list>(new functions_list(
			shared_ptr<statistics_map_detailed>(new statistics_map_detailed), 1.0 / ticks_per_second, resolver));
	}


	bool functions_list::static_resolver::get_symbol(address_t address, symbol_t &symbol) const
	{
		symbol.name = symbols[address];
		return true;
	}

	void functions_list::static_resolver::add_image(const wstring &/*image*/, address_t /*base_address*/)
	{	}
}
