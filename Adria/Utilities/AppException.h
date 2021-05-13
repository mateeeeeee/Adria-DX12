#pragma once
#include <stdexcept>
#include <string>
#include <sstream>

namespace adria
{

	class AppException : public std::exception
	{
		size_t line;
		std::string file;
		std::string msg;
	public:
		AppException(size_t line, char const* file, char const* msg = "");

		char const* what() const override;
	};

}