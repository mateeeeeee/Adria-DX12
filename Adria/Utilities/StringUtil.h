#pragma once
#include <string>
#include <vector>

namespace adria
{
	std::wstring ToWideString(std::string const& in);
	std::string ToString(std::wstring const& in);
	
	std::string ToLower(std::string const& in);
	std::string ToUpper(std::string const& in);

	bool FromCString(char const* in, int& out);
	bool FromCString(char const* in, float& out);
	bool FromCString(char const* in, std::string& out);
	bool FromCString(char const* in, bool& out);
	bool FromCString(char const* in, Vector3& out);

	std::string IntToString(int val);
	std::string FloatToString(float val);
	std::string CStrToString(char const* val);
	std::string BoolToString(bool val);
	std::string Vector3ToString(Vector3 const& val);

	std::vector<std::string> SplitString(std::string const& text, char delimeter);
}