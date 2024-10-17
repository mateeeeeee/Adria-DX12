#include <sstream>
#include <codecvt>
#include <locale>
#include "StringUtil.h"

namespace adria
{

	std::wstring ToWideString(std::string const& str)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.from_bytes(str);
	}
	std::string ToString(std::wstring const& wstr)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.to_bytes(wstr);
	}

	std::string ToLower(std::string const& str)
	{
		std::string out; out.resize(str.size());
		for (uint32 i = 0; i < str.size(); ++i)
		{
			out[i] = std::tolower(str[i]);
		}
		return out;
	}
	std::string ToUpper(std::string const& str)
	{
		std::string out; out.resize(str.size());
		for (uint32 i = 0; i < str.size(); ++i)
		{
			out[i] = std::toupper(str[i]);
		}
		return out;
	}
	
	bool FromCString(const char* in, int& out)
	{
		std::istringstream iss(in);
		iss >> out;
		return !iss.fail() && iss.eof();
	}

	bool FromCString(const char* in, float& out)
	{
		std::istringstream iss(in);
		iss >> out;
		return !iss.fail() && iss.eof();
	}

	bool FromCString(const char* in, std::string& out)
	{
		out = in;
		return true;
	}

	bool FromCString(const char* in, bool& out)
	{
		std::string str(in);
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		if (str == "0" || str == "false")
		{
			out = false;
			return true;
		}
		else if (str == "1" || str == "true")
		{
			out = true;
			return true;
		}
		return false;
	}

	bool FromCString(char const* in, Vector3& out)
	{
		std::istringstream iss(in);
		char open_parenthesis, comma1, comma2, close_parenthesis;
		if (iss >> open_parenthesis >> out.x >> comma1 >> out.y >> comma2 >> out.z >> close_parenthesis) 
		{
			return open_parenthesis == '(' && comma1 == ',' && comma2 == ',' && close_parenthesis == ')' && iss.eof();
		}
		return false;
	}

	std::string IntToString(int val)
	{
		return std::to_string(val);
	}
	std::string FloatToString(float val)
	{
		return std::to_string(val);
	}
	std::string CStrToString(char const* val)
	{
		return val;
	}
	std::string BoolToString(bool val)
	{
		return val ? "true" : "false";
	}

	std::string Vector3ToString(Vector3 const& val)
	{
		return "(" + std::to_string(val.x) + "," + std::to_string(val.y) + "," + std::to_string(val.z) + ")";
	}

	std::vector<std::string> SplitString(const std::string& text, char delimeter)
	{
		std::vector<std::string> tokens;
		size_t start = 0, end = 0;
		while ((end = text.find(delimeter, start)) != std::string::npos)
		{
			tokens.push_back(text.substr(start, end - start));
			start = end + 1;
		}
		tokens.push_back(text.substr(start));
		return tokens;
	}
}

