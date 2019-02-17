#include "CopyTextureFile.h"
#include "CopyTextureFile_STB.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include "CopyTextureFile_GdiPlus.h"
#endif

#ifndef _WIN32 // linux / macOS

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
int copyfile(const char* from, const char* to)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;
    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char* out_ptr = buf;
        ssize_t nwritten;
        do
        {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

#endif

namespace kil
{
    static std::string GetExt(const std::string& filepath)
    {
        if (filepath.find_last_of(".") != std::string::npos)
            return filepath.substr(filepath.find_last_of("."));
        return "";
    }

    static bool CopyTextureFile_NonConvert(const std::string& orgPath, const std::string& dstPath)
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
#else
        if (copyfile(orgPath.c_str(), dstPath.c_str()))
        {
            return true;
        }
        else
        {
            return false;
        }
#endif
    }

    bool CopyTextureFile(const std::string& orgPath, const std::string& dstPath, float quality)
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
                dstExt == ".tiff" || dstExt == ".tif" || dstExt == ".bmp")
            {
#ifdef _WIN32
                return CopyTextureFile_GdiPlus(orgPath, dstPath);
#else
                return CopyTextureFile_STB(orgPath, dstPath, quality);
#endif
            }
            else
            {
                return CopyTextureFile_STB(orgPath, dstPath, quality);
            }
        }
        return true;
    }
} // namespace kil
