#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "GLTF2GLB.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

#include <stdio.h>

#include <picojson/picojson.h>

namespace kml
{
    typedef std::uint32_t uint32;
    typedef std::uint8_t uchar;

    using namespace picojson;

    namespace
    {
        struct GLBHeader
        {
            uint32 magic;
            uint32 version;
            uint32 length;
        };

        struct GLBChunk
        {
            uint32 chunkLength;
            uint32 chunkType;
        };

        struct BufferProperty
        {
            std::string path;
            uint32 length;
        };

        static int Get4BytesAlign(int x)
        {
            if ((x % 4) == 0)
            {
                return x;
            }
            else
            {
                return ((x / 4) + 1) * 4;
            }
        }

        class BufferManager
        {
        public:
            uint32 Add(const std::string& path)
            {
                FILE* fp = fopen(path.c_str(), "rb");
                if (fp)
                {
                    fseek(fp, 0, SEEK_END);
                    uint32 sz = (uint32)ftell(fp);
                    fclose(fp);
                    BufferProperty prop;
                    prop.path = path;
                    prop.length = sz;
                    props.push_back(prop);
                    return sz;
                }
                return 0;
            }
            uint32 GetLength() const
            {
                uint32 length = 0;
                for (size_t i = 0; i < props.size(); i++)
                {
                    length += props[i].length;
                }
                return length;
            }
            bool WriteBinary(FILE* wp)
            {
                for (size_t i = 0; i < props.size(); i++)
                {
                    FILE* rp = fopen(props[i].path.c_str(), "rb");
                    if (rp)
                    {
                        bool bRet = this->Transfer(rp, wp);
                        fclose(rp);
                        if (!bRet)
                        {
                            return bRet;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }

                uint32 len = GetLength();
                uint32 byte4 = Get4BytesAlign(len);
                if (byte4 != len)
                {
                    uint32 rem = byte4 - len;
                    for (int i = 0; i < rem; i++)
                    {
                        fputc(0, wp);
                    }
                }

                return true;
            }

        private:
            static bool Transfer(FILE* rp, FILE* wp)
            {
                char buffer[1024] = {};
                while (feof(rp) == 0)
                {
                    uint32 num = fread(buffer, 1, 1024, rp);
                    if (num)
                    {
                        fwrite(buffer, 1, num, wp);
                    }
                }
                return true;
            }

        private:
            std::vector<BufferProperty> props;
        };
    } // namespace

    static std::string GetDirectoryPath(const std::string& path)
    {
#ifdef _WIN32
        char szDrive[_MAX_DRIVE];
        char szDir[_MAX_DIR];
        _splitpath(path.c_str(), szDrive, szDir, NULL, NULL);
        std::string strRet1;
        strRet1 += szDrive;
        strRet1 += szDir;
        return strRet1;
#else
        return path.substr(0, path.find_last_of('/')) + "/";
#endif
    }

    static std::string GetExt(const std::string& filepath)
    {
        if (filepath.find_last_of(".") != std::string::npos)
            return filepath.substr(filepath.find_last_of("."));
        return "";
    }

    static std::string GetMimeType(const std::string& path)
    {
        std::string strExt = GetExt(path);
        if (strExt == ".jpg" || strExt == ".jpeg")
        {
            return "image/jpeg";
        }
        else if (strExt == ".png")
        {
            return "image/png";
        }
        else if (strExt == ".tiff" || strExt == ".tif")
        {
            return "image/tiff";
        }
        else if (strExt == ".tga")
        {
            return "image/tga";
        }
        else if (strExt == ".bmp")
        {
            return "image/bmp";
        }
        return "image/jpeg";
    }

    static bool GLTF2GLB_(object& root, FILE* fp, const std::string& dir_path)
    {
        BufferManager bm;

        uint32 bufferOffset = 0;
        {
            auto& buff_val = root["buffers"];
            if (!buff_val.is<picojson::array>())
                return false;
            auto& buff_array = buff_val.get<picojson::array>();
            if (buff_array.size() == 0)
                return false;
            if (!buff_array[0].is<picojson::object>())
                return false;
            auto& buff_obj = buff_array[0].get<object>();
            std::string uri = dir_path + buff_obj["uri"].get<std::string>();
            {
                const auto iter = buff_obj.find("uri");
                if (iter != buff_obj.end())
                {
                    buff_obj.erase(iter);
                }
            }
            uint32 sz = bm.Add(uri);
            bufferOffset = sz;
        }

        {
            auto& buffer_views = root["bufferViews"].get<picojson::array>();
            if (root.find("images") != root.end())
            {
                auto& imgs_val = root["images"];
                if (!imgs_val.is<picojson::array>())
                    return false;
                auto& imgs_array = imgs_val.get<picojson::array>();
                for (size_t i = 0; i < imgs_array.size(); i++)
                {
                    auto& img = imgs_array[i].get<picojson::object>();
                    std::string uri = dir_path + img["uri"].get<std::string>();
                    uint32 sz = bm.Add(uri);
                    if (sz)
                    {
                        std::string mimeType = GetMimeType(uri);
                        uint32 index = (uint32)buffer_views.size();
                        img["bufferView"] = value((double)index);
                        img["mimeType"] = value(mimeType);
                        {
                            const auto iter = img.find("uri");
                            if (iter != img.end())
                            {
                                img.erase(iter);
                            }
                        }

                        picojson::object bufferView;
                        bufferView["buffer"] = value((double)0);
                        bufferView["byteLength"] = value((double)sz);
                        bufferView["byteOffset"] = value((double)bufferOffset);

                        buffer_views.push_back(picojson::value(bufferView));

                        bufferOffset += sz;
                    }
                    else
                    {
                        std::cout << "???:" << uri << std::endl;
                    }
                    
                }
            }
        }

        {
            auto& buff_val = root["buffers"];
            if (!buff_val.is<picojson::array>())
                return false;
            auto& buff_array = buff_val.get<picojson::array>();
            if (buff_array.size() == 0)
                return false;
            if (!buff_array[0].is<picojson::object>())
                return false;
            auto& buff_obj = buff_array[0].get<object>();
            buff_obj["byteLength"] = value((double)bufferOffset);
        }

        //images
        std::string json_str = value(root).serialize(false);
        std::vector<uchar> json_buffer(Get4BytesAlign(json_str.size()));
        memset(&json_buffer[0], ' ', sizeof(uchar) * json_buffer.size());
        memcpy(&json_buffer[0], json_str.c_str(), sizeof(uchar) * json_str.size());

        GLBChunk chunk0;
        chunk0.chunkLength = json_buffer.size() * sizeof(uchar);
        chunk0.chunkType = (uint32)0x4E4F534A; //ASCII

        GLBChunk chunk1;
        chunk1.chunkLength = Get4BytesAlign(bm.GetLength()) * sizeof(uchar);
        chunk1.chunkType = (uint32)0x004E4942; //BIN

        GLBHeader header;
        header.magic = (uint32)0x46546C67; //"glTF"
        header.version = 2;                //
        header.length = sizeof(GLBHeader) + (16 * sizeof(uchar)) + chunk0.chunkLength + chunk1.chunkLength;

        //header
        fwrite(&header, sizeof(GLBHeader), 1, fp);

        //chunk0
        fwrite(&chunk0, sizeof(GLBChunk), 1, fp);
        fwrite(&json_buffer[0], sizeof(uchar), json_buffer.size(), fp);

        //chunk1
        fwrite(&chunk1, sizeof(GLBChunk), 1, fp);
        if (!bm.WriteBinary(fp))
        {
            return false;
        }

        return true;
    }

    bool GLTF2GLB(const std::string& src, const std::string& dst)
    {
        bool bRet = true;
        picojson::value root_value;
        {
            std::ifstream ifs(src);
            if (!ifs)
            {
                return false;
            }

            std::string err = picojson::parse(root_value, ifs);
            if (!err.empty())
            {
                return false;
            }
        }

        if (!root_value.is<object>())
        {
            return false;
        }

        FILE* fp = fopen(dst.c_str(), "wb");
        if (!fp)
        {
            return false;
        }

        std::string dir_path = GetDirectoryPath(src);
        bRet = GLTF2GLB_(root_value.get<object>(), fp, dir_path);

        {
            fclose(fp);
        }

        return bRet;
    }
} // namespace kml
