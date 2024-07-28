#include <algorithm>
#include <sstream>
#include "StringUtil.h"

namespace adria
{

	std::wstring ToWideString(std::string const& in)
	{
		std::wstring out{};
		out.reserve(in.length());
		const char* ptr = in.data();
		const char* const end = in.data() + in.length();

		mbstate_t state{};
		wchar_t wc;
		while (size_t len = mbrtowc(&wc, ptr, end - ptr, &state))
		{
			if (len == static_cast<size_t>(-1)) // bad encoding
				return std::wstring{};
			if (len == static_cast<size_t>(-2))
				break;
			out.push_back(wc);
			ptr += len;
		}
		return out;
	}
	std::string ToString(std::wstring const& in)
	{
		std::string out{};
		out.reserve(MB_CUR_MAX * in.length());

		mbstate_t state{};
		for (wchar_t wc : in)
		{
			char mb[8]{}; // ensure null-terminated for UTF-8 (maximum 4 byte)
			const auto len = wcrtomb(mb, wc, &state);
			out += std::string_view{ mb, len };
		}
		return out;
	}

	std::string ToLower(std::string const& in)
	{
		std::string out; out.resize(in.size());
		std::transform(std::begin(in), std::end(in), std::begin(out), [](char c) {return std::tolower(c); });
		return out;
	}
	std::string ToUpper(std::string const& in)
	{
		std::string out; out.resize(in.size());
		std::transform(std::begin(in), std::end(in), std::begin(out), [](char c) {return std::toupper(c); });
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

