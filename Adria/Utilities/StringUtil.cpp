#include <sstream>
#include <codecvt>
#include <locale>
#include "StringUtil.h"

namespace adria
{

	std::wstring ToWideString(std::string const& str)
	{
		Sint num_chars = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (Sint)str.length(), NULL, 0);
		std::wstring wstr;
		if (num_chars)
		{
			wstr.resize(num_chars);
			MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (Sint)str.length(), &wstr[0], num_chars);
		}
		return wstr;
	}
	std::string ToString(std::wstring const& wstr)
	{
		Sint num_chars = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (Sint)wstr.length(), NULL, 0, NULL, NULL);
		std::string str;
		if (num_chars > 0)
		{
			str.resize(num_chars);
			WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (Sint)wstr.length(), &str[0], num_chars, NULL, NULL);
		}
		return str;
	}

	std::string ToLower(std::string const& str)
	{
		std::string out; out.resize(str.size());
		for (Uint32 i = 0; i < str.size(); ++i)
		{
			out[i] = std::tolower(str[i]);
		}
		return out;
	}
	std::string ToUpper(std::string const& str)
	{
		std::string out; out.resize(str.size());
		for (Uint32 i = 0; i < str.size(); ++i)
		{
			out[i] = std::toupper(str[i]);
		}
		return out;
	}
	
	Bool FromCString(const Char* in, int& out)
	{
		std::istringstream iss(in);
		iss >> out;
		return !iss.fail() && iss.eof();
	}

	Bool FromCString(const Char* in, Float& out)
	{
		std::istringstream iss(in);
		iss >> out;
		return !iss.fail() && iss.eof();
	}

	Bool FromCString(const Char* in, std::string& out)
	{
		out = in;
		return true;
	}

	Bool FromCString(const Char* in, Bool& out)
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

	Bool FromCString(Char const* in, Vector3& out)
	{
		std::istringstream iss(in);
		Char open_parenthesis, comma1, comma2, close_parenthesis;
		if (iss >> open_parenthesis >> out.x >> comma1 >> out.y >> comma2 >> out.z >> close_parenthesis) 
		{
			return open_parenthesis == '(' && comma1 == ',' && comma2 == ',' && close_parenthesis == ')' && iss.eof();
		}
		return false;
	}

	std::string IntToString(Sint val)
	{
		return std::to_string(val);
	}
	std::string FloatToString(Float val)
	{
		return std::to_string(val);
	}
	std::string CStrToString(Char const* val)
	{
		return val;
	}
	std::string BoolToString(Bool val)
	{
		return val ? "true" : "false";
	}

	std::string Vector3ToString(Vector3 const& val)
	{
		return "(" + std::to_string(val.x) + "," + std::to_string(val.y) + "," + std::to_string(val.z) + ")";
	}

	std::vector<std::string> SplitString(const std::string& text, Char delimeter)
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

