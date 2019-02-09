#include "ResizeTextureFile.h"
#include "CopyTextureFile.h"

#include <iostream>
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

//#define STB_IMAGE_IMPLEMENTATION 1
//#define STB_IMAGE_RESIZE_IMPLEMENTATION 1
//#define STB_IMAGE_WRITE_IMPLEMENTATION 1

#include <stb/stb_image.h>
#include <stb/stb_image_resize.h>
#include <stb/stb_image_write.h>

namespace kil
{
	static
	std::string GetExt(const std::string& filepath)
	{
		if (filepath.find_last_of(".") != std::string::npos)
			return filepath.substr(filepath.find_last_of("."));
		return "";
	}

	bool ResizeTextureFile_STB(const std::string& orgPath, const std::string& dstPath, int maximum_size, int resize_size, float quality)
	{
		int width = 0;
		int height = 0;
		int channels = 1;
		stbi_uc* buffer = stbi_load(orgPath.c_str(), &width, &height, &channels, 0);
		if (buffer == NULL)
		{
			return false;
		}

		if (width <= maximum_size && height <= maximum_size)
		{
			if (buffer)
			{
				stbi_image_free(buffer);
			}
			return CopyTextureFile(orgPath, dstPath, quality);
		}

		float factor = resize_size / ((float)std::max<int>(width, height));

		int nw = (int)floor(width * factor);
		int nh = (int)floor(height * factor);
		stbi_uc* nbuffer = (unsigned char*)malloc(sizeof(unsigned char)*nw*nh*channels);
		if (!stbir_resize_uint8(buffer, width, height, 0, nbuffer, nw, nh, 0, channels))
		{
			if (nbuffer)
			{
				free(nbuffer);
			}
			if (buffer)
			{
				stbi_image_free(buffer);
			}
			return false;
		}

		std::string ext = GetExt(dstPath);
		if (ext == ".jpg" || ext == ".jpeg")
		{
            int q = std::max<int>(0, std::min<int>(int(quality * 100), 100));
			stbi_write_jpg(dstPath.c_str(), nw, nh, channels, nbuffer, q);//quality = 90
		}
		else if (ext == ".png")
		{
			stbi_write_png(dstPath.c_str(), nw, nh, channels, nbuffer, 0);
		}
		else if (ext == ".bmp")
		{
			stbi_write_bmp(dstPath.c_str(), nw, nh, channels, nbuffer);
		}
		else if (ext == ".gif")
		{
			if (nbuffer)
			{
				free(nbuffer);
			}

			if (buffer)
			{
				stbi_image_free(buffer);
			}
			return false;
		}


		if (nbuffer)
		{
			free(nbuffer);
		}

		if (buffer)
		{
			stbi_image_free(buffer);
		}
		return true;
	}

	static
	std::string GetPngTempPath()
	{
#ifdef _WIN32 
		char bufDir[_MAX_PATH] = {};
		DWORD nsz = ::GetTempPathA((DWORD)_MAX_PATH, bufDir);
		bufDir[nsz] = 0;
		return std::string(bufDir) + std::string("tmp.png");
#else // Linux and macOS
		const char* tmpdir = getenv("TMPDIR");
		return std::string(tmpdir) + std::string("tmp.png");
#endif
	}

	static
	void RemoveFile(const std::string& path)
	{
#ifdef _WIN32 
		::DeleteFileA(path.c_str());
#else // Linux and macOS
		 remove(path.c_str());
#endif
	}

	bool ResizeTextureFile(const std::string& orgPath, const std::string& dstPath, int maximum_size, int resize_size, float quality)
	{
		std::string ext = GetExt(orgPath);
		if (ext == ".tiff" || ext == ".tif")
		{
			std::string tmpPath = GetPngTempPath();
			bool bRet = true;
			bRet = CopyTextureFile(orgPath, tmpPath, quality);
			if (!bRet)return bRet;
			bRet = ResizeTextureFile_STB(tmpPath, dstPath, maximum_size, resize_size, quality);
			RemoveFile(tmpPath);
			return bRet;
		}
		else
		{
			return ResizeTextureFile_STB(orgPath, dstPath, maximum_size, resize_size, quality);
		}
	}
}
