#define _CRT_SECURE_NO_WARNINGS
#include "CopyTextureFile_GdiPlus.h"

#ifdef _WIN32
#include <shlobj.h>
#include <gdiplus.h>
#include <Gdiplusinit.h>
#include <windows.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#endif

#include <assert.h>
#include <vector>
#include <string>
#include <memory>

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
	std::wstring GetExt(const std::wstring& filepath)
	{
		if (filepath.find_last_of(L".") != std::wstring::npos)
			return filepath.substr(filepath.find_last_of(L"."));
		return L"";
	}

#ifdef _WIN32
	using namespace Gdiplus;

	static BOOL GetEncoderClsid(LPCWSTR lpszFormat, CLSID* pClsid)
	{
		assert(lpszFormat);
		assert(pClsid);
		UINT nEncoders;
		UINT nSize;
		if (Gdiplus::GetImageEncodersSize(&nEncoders, &nSize) != Gdiplus::Ok)
		{
			return FALSE;
		}
		std::vector<BYTE> _scoped(nSize);
		Gdiplus::ImageCodecInfo* pEncoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(&_scoped[0]);
		if (Gdiplus::GetImageEncoders(nEncoders, nSize, pEncoders) != Gdiplus::Ok)
		{
			return FALSE;
		}
		std::wstring strFormat(lpszFormat);
		for (UINT i = 0; i < nEncoders; ++i)
		{
			if (strFormat == pEncoders[i].MimeType)
			{
				*pClsid = pEncoders[i].Clsid;
				return TRUE;
			}
		}
		return FALSE;
	}

	//L"image/tiff"
	static 
	BOOL SaveImage(LPCWSTR lpszFormat, LPCWSTR lpszFilePath, Gdiplus::Bitmap& img)
	{
		CLSID encoderClsid;
		GetEncoderClsid(lpszFormat, &encoderClsid);
		return (img.Save(lpszFilePath, &encoderClsid, NULL) == Gdiplus::Ok);
	}

	static
	std::wstring GetFormat(LPCWSTR szOutputPath)
	{
		std::wstring strExt = GetExt(szOutputPath);
		if (strExt == L".jpg" || strExt == L".jpeg")
		{
			return L"image/jpeg";
		}
		else if (strExt == L".png")
		{
			return L"image/png";
		}
		else if (strExt == L".tiff" || strExt == L".tif")
		{
			return L"image/tiff";
		}
		else if (strExt == L".tga")
		{
			return L"image/tga";
		}
		else if (strExt == L".bmp")
		{
			return L"image/bmp";
		}
		return L"image/jpeg";
	}

	static
	bool CopyTextureFile_GdiPlus_Imp(LPCWSTR szInputPath, LPCWSTR szOutputPath)
	{
		GdiplusStartupInput gdiplusStartupInput;
		ULONG_PTR gdiplusToken = NULL;
		if (Gdiplus::Ok != GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL))
		{
			return false;
		}

		bool bRet = true;
		{
			std::shared_ptr<Bitmap> bmp(Bitmap::FromFile(szInputPath));
			if (bmp.get())
			{
				std::wstring strFormat = GetFormat(szOutputPath);
				if (SaveImage(strFormat.c_str(), szOutputPath, *bmp))
				{
					bRet = true;
				}
				else
				{
					bRet = false;
				}
			}
			else
			{
				bRet = false;
			}
		}

		GdiplusShutdown(gdiplusToken);
		return bRet;
	}
#endif

	static
	bool CopyTextureFile_GdiPlus_Imp(const std::string& orgPath, const std::string& dstPath)
	{
#ifdef _WIN32
		WCHAR inBuffer [_MAX_PATH] = {};
		WCHAR outBuffer[_MAX_PATH] = {};
		int nRet = 0;
		nRet = ::MultiByteToWideChar(CP_OEMCP, 0, orgPath.c_str(), (int)orgPath.size() + 1, inBuffer, _MAX_PATH);
		if (nRet == 0)
		{
			return false;
		}
		nRet = ::MultiByteToWideChar(CP_OEMCP, 0, dstPath.c_str(), (int)dstPath.size() + 1, outBuffer, _MAX_PATH);
		if (nRet == 0)
		{
			return false;
		}
		return CopyTextureFile_GdiPlus_Imp(inBuffer, outBuffer);
#else
		return false;
#endif
	}

	bool CopyTextureFile_GdiPlus(const std::string& orgPath, const std::string& dstPath)
	{

		return CopyTextureFile_GdiPlus_Imp(orgPath, dstPath);
	}
}
