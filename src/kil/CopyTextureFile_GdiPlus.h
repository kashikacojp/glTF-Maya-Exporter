#pragma once
#ifndef _KIL_COPY_TEXTURE_FILE_GDIPLUS_H_
#define _KIL_COPY_TEXTURE_FILE_GDIPLUS_H_

#include <string>

namespace kil
{
	bool CopyTextureFile_GdiPlus(const std::string& src_path, const std::string& dst_path);
}

#endif
