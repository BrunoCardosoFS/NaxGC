/*
NaxGC
Copyright (C) 2026 Bruno Cardoso <brunocardosofs@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <cassert>
#include <string>
#include <utility>
#include <variant>

namespace naxgc {

enum class DiagnosticSeverity {
	Error,
	Warning,
};

struct Diagnostic {
	DiagnosticSeverity severity{DiagnosticSeverity::Error};
	std::string code;
	std::string json_path;
	std::string message;

	std::string format() const
	{
		if (json_path.empty()) {
			return code + ": " + message;
		}
		return code + " " + json_path + ": " + message;
	}
};

struct Error {
	std::string code;
	std::string message;
};

template<typename T, typename E = Error> class Result {
public:
	static Result ok(T val) { return Result(std::in_place_index<0>, std::move(val)); }

	static Result err(E error) { return Result(std::in_place_index<1>, std::move(error)); }

	bool is_ok() const noexcept { return data_.index() == 0; }

	bool is_err() const noexcept { return data_.index() == 1; }

	explicit operator bool() const noexcept { return is_ok(); }

	const T &value() const &
	{
		assert(is_ok());
		return std::get<0>(data_);
	}

	T &value() &
	{
		assert(is_ok());
		return std::get<0>(data_);
	}

	T &&value() &&
	{
		assert(is_ok());
		return std::get<0>(std::move(data_));
	}

	const E &error() const &
	{
		assert(is_err());
		return std::get<1>(data_);
	}

	E &error() &
	{
		assert(is_err());
		return std::get<1>(data_);
	}

private:
	template<std::size_t I, typename... Args>
	explicit Result(std::in_place_index_t<I> idx, Args &&...args) : data_(idx, std::forward<Args>(args)...)
	{
	}

	std::variant<T, E> data_;
};

template<typename E> class Result<void, E> {
public:
	static Result ok() { return Result(std::in_place_index<0>, std::monostate{}); }

	static Result err(E error) { return Result(std::in_place_index<1>, std::move(error)); }

	bool is_ok() const noexcept { return data_.index() == 0; }

	bool is_err() const noexcept { return data_.index() == 1; }

	explicit operator bool() const noexcept { return is_ok(); }

	const E &error() const &
	{
		assert(is_err());
		return std::get<1>(data_);
	}

private:
	template<std::size_t I, typename... Args>
	explicit Result(std::in_place_index_t<I> idx, Args &&...args) : data_(idx, std::forward<Args>(args)...)
	{
	}

	std::variant<std::monostate, E> data_;
};

} // namespace naxgc
