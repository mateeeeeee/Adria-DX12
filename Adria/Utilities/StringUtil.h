#pragma once
#include <string>
#include <vector>

namespace adria
{
	std::wstring ToWideString(std::string const&);
	std::string ToString(std::wstring const&);
	
	std::string ToLower(std::string const&);
	std::string ToUpper(std::string const&);

	Bool FromCString(Char const* in, int& out);
	Bool FromCString(Char const* in, Float& out);
	Bool FromCString(Char const* in, std::string& out);
	Bool FromCString(Char const* in, Bool& out);
	Bool FromCString(Char const* in, Vector3& out);

	std::string IntToString(Sint val);
	std::string FloatToString(Float val);
	std::string CStrToString(Char const* val);
	std::string BoolToString(Bool val);
	std::string Vector3ToString(Vector3 const& val);

	std::vector<std::string> SplitString(std::string const& text, Char delimeter);
}