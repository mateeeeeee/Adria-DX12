#pragma once
#include <string>
#include <vector>

namespace adria
{
	std::wstring ToWideString(std::string const& in);
	std::string ToString(std::wstring const& in);
	
	std::string ToLower(std::string const& in);
	std::string ToUpper(std::string const& in);

	bool FromCString(const char* in, int& out);
	bool FromCString(const char* in, float& out);
	bool FromCString(const char* in, const char*& out);
	bool FromCString(const char* in, bool& out);

	std::string IntToString(int val);
	std::string FloatToString(float val);
	std::string CStrToString(char const* val);
	std::string BoolToString(bool val);

	std::vector<std::string> SplitString(std::string const& text, char delimeter);
}