#include "AdriaException.h"

namespace adria
{

	AdriaException::AdriaException(size_t line, char const* file, char const* message) :line{ line }, file{ file }
	{
		std::ostringstream iss;
		iss << "App Exception \n";
		iss << "On line " << line << " in file " << file << "\n";
		msg = iss.str() + std::string(message);
	}

	const char* AdriaException::what() const
	{
		return msg.c_str();
	}

}
