#pragma once


namespace adria
{

	enum class FileType : uint8
	{
		PNG,
		JPG,
		HDR,
		TGA,
		BMP
	};
	void WriteImageToFile(FileType type, std::string_view filename, uint32 width, uint32 height, void const* data, uint32 stride);
}