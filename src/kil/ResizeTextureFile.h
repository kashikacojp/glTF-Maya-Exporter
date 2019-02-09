#pragma once
#ifndef _KIL_RESIZE_TEXTURE_FILE_H_
#define _KIL_RESIZE_TEXTURE_FILE_H_

#include <string>

namespace kil
{
    bool ResizeTextureFile(const std::string& src_path, const std::string& dst_path, int maximum_size = 256, int resize_size = 256, float quality = 0.9);
}

#endif
