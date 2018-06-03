#include "CopyTextureFile.h"
#include "CopyTextureFile_STB.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "CopyTextureFile_GdiPlus.h"
#endif


namespace kil
{
	static
	std::string GetExt(const std::string& filepath)
	{
		if (filepath.find_last_of(".") != std::string::npos)
			return filepath.substr(filepath.find_last_of("."));
		return "";
	}

	static
	bool CopyTextureFile_NonConvert(const std::string& orgPath, const std::string& dstPath)
	{
#ifdef _WIN32
		if (::CopyFileA(orgPath.c_str(), dstPath.c_str(), TRUE))
		{
			return true;
		}
		else
		{
			return false;
		}
#endif
	}


	bool CopyTextureFile(const std::string& orgPath, const std::string& dstPath)
	{
		std::string orgExt = GetExt(orgPath);
		std::string dstExt = GetExt(dstPath);
		if (orgExt == dstExt)
		{
			return CopyTextureFile_NonConvert(orgPath, dstPath);
		}
		else
		{
			if (
				orgExt == ".tiff" || orgExt == ".tif" || orgExt == ".bmp" ||
				dstExt == ".tiff" || dstExt == ".tif" || dstExt == ".bmp"
			)
			{
#ifdef _WIN32
				return CopyTextureFile_GdiPlus(orgPath, dstPath);
#else
				return CopyTextureFile_STB(orgPath, dstPath);
#endif
			}
			else
			{
				return CopyTextureFile_STB(orgPath, dstPath);
			}
		}
		return true;
	}
}
