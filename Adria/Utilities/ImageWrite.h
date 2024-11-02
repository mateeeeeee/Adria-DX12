#pragma once


namespace adria
{

	enum class FileType : Uint8
	{
		PNG,
		JPG,
		HDR,
		TGA,
		BMP
	};
	void WriteImageToFile(FileType type, std::string_view filename, Uint32 width, Uint32 height, void const* data, Uint32 stride);
}