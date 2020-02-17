//	Copyright (c) 2011-2020 by Artem A. Gevorkyan (gevorkyan.org)
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

#pragma once

#include <string>

namespace micro_profiler
{
	template <typename T>
	inline std::basic_string<T> operator &(const std::basic_string<T> &lhs, const std::basic_string<T> &rhs)
	{
		if (lhs.empty())
			return rhs;
		if (lhs.back() == T('\\') || lhs.back() == T('/'))
			return lhs + rhs;
		return lhs + T('/') + rhs;
	}

	template <typename T>
	inline std::basic_string<T> operator &(const std::basic_string<T> &lhs, const T *rhs)
	{	return lhs & std::basic_string<T>(rhs);	}

	template <typename T>
	inline std::basic_string<T> operator &(const T *lhs, const std::basic_string<T> &rhs)
	{	return std::basic_string<T>(lhs) & rhs;	}

	inline std::string operator ~(const std::string &value)
	{
		const size_t pos = value.find_last_of("\\/");

		if (pos != std::string::npos)
			return value.substr(0, pos);
		return std::string();
	}

	inline std::string operator *(const std::string &value)
	{
		const size_t pos = value.find_last_of("\\/");

		if (pos != std::string::npos)
			return value.substr(pos + 1);
		return value;
	}
}
