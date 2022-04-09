#pragma once
#include <stdexcept>
#include <string>
#include <sstream>

namespace adria
{

	class AdriaException : public std::exception
	{
		size_t line;
		std::string file;
		std::string msg;
	public:
		AdriaException(size_t line, char const* file, char const* msg = "");

		char const* what() const override;
	};

}