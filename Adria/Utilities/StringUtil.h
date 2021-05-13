#pragma once

#include <string>



namespace adria
{


	inline std::wstring ConvertToWide(std::string const& in)
	{
		std::wstring out{};
		out.reserve(in.length());
		const char* ptr = in.data();
		const char* const end = in.data() + in.length();

		mbstate_t state{};
		wchar_t wc;
		while (size_t len = mbrtowc(&wc, ptr, end - ptr, &state)) {
			if (len == static_cast<size_t>(-1)) // bad encoding
				return std::wstring{};
			if (len == static_cast<size_t>(-2))
				break;                         
			out.push_back(wc);
			ptr += len; 
		}
		return out;
	}

	inline std::string ConvertToNarrow(std::wstring const& in)
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

}