#define GLM_ENABLE_EXPERIMENTAL 1

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#define NOMINMAX
#include <windows.h>
#endif

#include "glTFComponents.h"
#include "glTFConstants.h"
#include "glTFExporter.h"

#include "Options.h"
#include "SaveToDraco.h"
#include "Texture.h"
#include "TriangulateMesh.h"

#include <climits>
#include <fstream>
#include <set>
#include <vector>

#include <picojson/picojson.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <stdlib.h>
#include <string.h>

namespace
{
    enum ImageFormat
    {
        FORMAT_JPEG = 0,
        FORMAT_PNG,
        FORMAT_BMP,
        FORMAT_GIF
    };
}

namespace kml
{
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

    void Pad4BytesAlign(std::vector<unsigned char>& bytes)
    {
        int len = bytes.size();
        int byte4 = Get4BytesAlign(len);
        if (byte4 != len)
        {
            int rem = byte4 - len;
            for (int i = 0; i < rem; i++)
            {
                bytes.push_back(0);
            }
        }
    }

    static std::string IToS(int n)
    {
        char buffer[16] = {};
#ifdef _WIN32
        _snprintf(buffer, 16, "%03d", n);
#else
        snprintf(buffer, 16, "%03d", n);
#endif
        return buffer;
    }

    static std::string GetBaseDir(const std::string& filepath)
    {
#ifdef _WIN32
        char dir[MAX_PATH + 1] = {};
        char drv[MAX_PATH + 1] = {};
        _splitpath(filepath.c_str(), drv, dir, NULL, NULL);
        return std::string(drv) + std::string(dir);
#else
        if (filepath.find_last_of("/") != std::string::npos)
            return filepath.substr(0, filepath.find_last_of("/")) + "/";
#endif
        return "";
    }

    static std::string RemoveExt(const std::string& filepath)
    {
        if (filepath.find_last_of(".") != std::string::npos)
            return filepath.substr(0, filepath.find_last_of("."));
        return filepath;
    }

    static std::string GetBaseName(const std::string& filepath)
    {
#ifdef _WIN32
        char fname[MAX_PATH + 1] = {};
        _splitpath(filepath.c_str(), NULL, NULL, fname, NULL);
        return fname;
#else
        if (filepath.find_last_of("/") != std::string::npos)
            return RemoveExt(filepath.substr(filepath.find_last_of("/") + 1));
#endif
        return filepath;
    }

    static std::string GetImageID(const std::string& imagePath)
    {
        return GetBaseName(imagePath);
    }

    static std::string GetTextureID(const std::string& imagePath)
    {
        return "texture_" + GetImageID(imagePath);
    }

    static std::string GetFileExtName(const std::string& path)
    {
#ifdef _WIN32
        char szFname[_MAX_FNAME];
        char szExt[_MAX_EXT];
        _splitpath(path.c_str(), NULL, NULL, szFname, szExt);
        std::string strRet1;
        strRet1 += szFname;
        strRet1 += szExt;
        return strRet1;
#else
        int i = path.find_last_of('/');
        if (i != std::string::npos)
        {
            return path.substr(i + 1);
        }
        else
        {
            return path;
        }
#endif
    }

    static void GetTextures(std::vector<std::shared_ptr<kml::Texture> >& texture_vec, const std::vector<std::shared_ptr< ::kml::Material> >& materials)
    {
        std::set<const kml::Texture*> tex_set;
        for (size_t j = 0; j < materials.size(); j++)
        {
            const auto& mat = materials[j];
            auto keys = mat->GetTextureKeys();
            for (int i = 0; i < keys.size(); i++)
            {
                std::shared_ptr<kml::Texture> tex = mat->GetTexture(keys[i]);
                if (tex->FileExists())
                {
                    if (tex_set.find(tex.get()) == tex_set.end())
                    {
                        tex_set.insert(tex.get());
                        texture_vec.push_back(tex);
                    }
                }
            }
        }
    }

    static void GetImages(std::vector<std::string>& image_vec, const std::vector<std::shared_ptr<kml::Texture> >& texture_vec)
    {
        std::set<std::string> image_set;
        for (size_t j = 0; j < texture_vec.size(); j++)
        {
            std::string path = texture_vec[j]->GetFilePath();
            if (image_set.find(path) == image_set.end())
            {
                image_set.insert(path);
                image_vec.push_back(path);
            }
        }
    }

    static void GetCacheImages(std::vector<std::string>& image_vec, const std::vector<std::shared_ptr<kml::Texture> >& texture_vec)
    {
        std::set<std::string> image_set;
        for (size_t j = 0; j < texture_vec.size(); j++)
        {
            std::string path = texture_vec[j]->GetFilePath();
            if (image_set.find(path) == image_set.end())
            {
                image_set.insert(path);
                image_vec.push_back(texture_vec[j]->GetCacheFilePath());
            }
        }
    }

    static std::string GetExt(const std::string& filepath)
    {
        if (filepath.find_last_of(".") != std::string::npos)
            return filepath.substr(filepath.find_last_of("."));
        return "";
    }

    static unsigned int GetImageFormat(const std::string& path)
    {
        std::string ext = GetExt(path);
        if (ext == ".jpg" || ext == ".jpeg")
        {
            return FORMAT_JPEG;
        }
        else if (ext == ".png")
        {
            return FORMAT_PNG;
        }
        else if (ext == ".bmp")
        {
            return FORMAT_BMP;
        }
        else if (ext == ".gif")
        {
            return FORMAT_GIF;
        }
        return FORMAT_JPEG;
    }

    static bool IsZero(const float* p, int sz, float eps = 1e-15f)
    {
        for (int i = 0; i < sz; i++)
        {
            if (std::fabs(p[i]) > eps)
            {
                return false;
            }
        }
        return true;
    }

    static bool IsZero(const glm::vec3& p, float eps = 1e-15)
    {
        return IsZero(glm::value_ptr(p), 3, eps);
    }

    static bool IsZero(const glm::quat& p, float eps = 1e-15)
    {
        return IsZero(glm::value_ptr(p), 4, eps);
    }

    static bool IsZero(const glm::mat4& p, float eps = 1e-15)
    {
        return IsZero(glm::value_ptr(p), 16, eps);
    }

    namespace gltf
    {
        static void GetMinMax(float min[], float max[], const std::vector<float>& verts, int n)
        {
            for (int i = 0; i < n; i++)
            {
                min[i] = +1e+16;
                max[i] = -1e+16;
            }
            size_t sz = verts.size() / n;
            for (size_t i = 0; i < sz; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    min[j] = std::min<float>(min[j], verts[n * i + j]);
                    max[j] = std::max<float>(max[j], verts[n * i + j]);
                }
            }
        }

        static void GetMinMax(unsigned int& min, unsigned int& max, const std::vector<unsigned int>& verts)
        {
            {
                min = std::numeric_limits<unsigned int>::max();
                max = 0;
            }
            size_t sz = verts.size();
            for (size_t i = 0; i < sz; i++)
            {
                min = std::min<unsigned int>(min, verts[i]);
                max = std::max<unsigned int>(max, verts[i]);
            }
        }

        static void GetMinMax(unsigned short min[], unsigned short max[], const std::vector<unsigned short>& verts, int n)
        {
            for (int i = 0; i < n; i++)
            {
                min[i] = std::numeric_limits<unsigned short>::max();
                max[i] = 0;
            }
            size_t sz = verts.size() / n;
            for (size_t i = 0; i < sz; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    min[j] = std::min<unsigned short>(min[j], verts[n * i + j]);
                    max[j] = std::max<unsigned short>(max[j], verts[n * i + j]);
                }
            }
        }

        template <class T>
        static picojson::array ConvertToArray(T v[], int n)
        {
            picojson::array a;
            for (int j = 0; j < n; j++)
            {
                a.push_back(picojson::value((double)v[j]));
            }
            return a;
        }

        static picojson::array ConvertToArray(std::vector<float>& v)
        {
            picojson::array a;
            for (int j = 0; j < v.size(); j++)
            {
                a.push_back(picojson::value((double)v[j]));
            }
            return a;
        }

        static int ConvertTextureFilterType(int type)
        {
            switch (type)
            {
            case ::kml::Texture::FilterType::FILTER_NEAREST:
                return GLTF_TEXTURE_FILTER_NEAREST;
            case ::kml::Texture::FilterType::FILTER_LINEAR:
                return GLTF_TEXTURE_FILTER_LINEAR;
            default:
                return GLTF_TEXTURE_FILTER_LINEAR;
            }
        }

        static int ConvertTextureWrapType(int type)
        {
            switch (type)
            {
            case ::kml::Texture::WrapType::WRAP_REPEAT:
                return GLTF_TEXTURE_WRAP_REPEAT;
            case ::kml::Texture::WrapType::WRAP_CLAMP:
                return GLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
            case ::kml::Texture::WrapType::WRAP_MIRROR:
                return GLTF_TEXTURE_WRAP_MIRRORED_REPEAT;
            default:
                return GLTF_TEXTURE_WRAP_CLAMP_TO_EDGE;
            }
        }

        class ObjectRegisterer
        {
        public:
            ObjectRegisterer(const std::string& basename)
            {
                basename_ = basename;
            }

            std::shared_ptr<Node> CreateNode(const std::shared_ptr< ::kml::Node>& in_node)
            {
                int nNode = nodes_.size();
                std::shared_ptr<Node> node(new Node(in_node->GetName(), nNode));
                node->SetPath(in_node->GetPath());
                if (!in_node->GetTransform()->IsTRS())
                {
                    node->GetTransform()->SetMatrix(in_node->GetTransform()->GetMatrix());
                }
                else
                {
                    auto T = in_node->GetTransform()->GetT();
                    auto R = in_node->GetTransform()->GetR();
                    auto S = in_node->GetTransform()->GetS();
                    node->GetTransform()->SetTRS(T, R, S);
                }

                this->AddNode(node);
                return node;
            }

            std::shared_ptr<Skin> RegisterSkin(const std::shared_ptr< ::kml::Skin>& in_skin)
            {
                int nSkin = this->skins_.size();
                std::string skinName = in_skin->GetName();
                std::shared_ptr<Skin> skin(new Skin(skinName, nSkin));

                {
                    const auto& in_joints = in_skin->GetJoints();
                    for (size_t i = 0; i < in_joints.size(); i++)
                    {
                        const auto& node = nodeMap_[in_joints[i]->GetPath()];
                        std::shared_ptr<Joint> joint(new Joint());
                        joint->SetNode(node);
                        joint->SetSkin(skin);
                        joint->SetIndexInSkin(i);

                        node->SetJoint(joint);
                        skin->AddJoint(joint);
                    }
                }

                {
                    std::vector<float> inverseMatrices;
                    const auto& in_mats = in_skin->GetJointBindMatrices();
                    for (size_t i = 0; i < in_mats.size(); i++)
                    {
                        glm::mat4 mat = in_mats[i];
                        const float* ptr = glm::value_ptr(mat);
                        for (size_t k = 0; k < 16; k++)
                        {
                            inverseMatrices.push_back(ptr[k]);
                        }
                    }

                    int nAcc = accessors_.size();
                    //indices
                    std::string accName = "accessor_" + IToS(nAcc); //
                    std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                    const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(inverseMatrices, -1);
                    acc->SetBufferView(bufferView);
                    acc->SetCount(inverseMatrices.size() / 16);
                    acc->SetType("MAT4");
                    acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                    acc->SetByteOffset(0);
                    //acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));
                    /*
                    float min[16] = {}, max[16] = {};
                    GetMinMax(min, max, inverseMatrices, 16);
                    acc->Set("min", picojson::value(ConvertToArray(min, 16)));
                    acc->Set("max", picojson::value(ConvertToArray(max, 16)));
                    */

                    accessors_.push_back(acc);
                    skin->SetAccessor("inverseBindMatrices", acc);
                    nAcc++;
                }

                this->skins_.push_back(skin);
                return skin;
            }

            std::shared_ptr<TextureSampler> RegisterTextureSampler(const std::shared_ptr< ::kml::Texture>& in_texture)
            {
                int minFilter = ConvertTextureFilterType(in_texture->GetFilterTypeU());
                int magFilter = ConvertTextureFilterType(in_texture->GetFilterTypeV());
                int wrapS = ConvertTextureWrapType(in_texture->GetWrapTypeU());
                int wrapT = ConvertTextureWrapType(in_texture->GetWrapTypeV());
                if (this->texture_samplers_.empty())
                {
                    this->texture_samplers_.push_back(std::shared_ptr<TextureSampler>(new TextureSampler(0, minFilter, magFilter, wrapS, wrapT)));
                    return this->texture_samplers_.back();
                }
                else
                {
                    int nTexSamplers = (int)this->texture_samplers_.size();
                    int nFind = -1;
                    for (int i = 0; i < nTexSamplers; i++)
                    {
                        if (this->texture_samplers_[i]->Equal(minFilter, magFilter, wrapS, wrapT))
                        {
                            nFind = i;
                            break;
                        }
                    }
                    if (nFind >= 0)
                    {
                        return this->texture_samplers_[nFind];
                    }
                    else
                    {
                        this->texture_samplers_.push_back(std::shared_ptr<TextureSampler>(new TextureSampler(nTexSamplers, minFilter, magFilter, wrapS, wrapT)));
                        return this->texture_samplers_.back();
                    }
                }
            }

            std::vector<std::shared_ptr<MorphTarget> > RegisterMorphTargets(const std::shared_ptr< ::kml::Mesh>& in_mesh)
            {
                int nAcc = accessors_.size();
                int nTar = morph_targets_.size();
                std::vector<std::shared_ptr<MorphTarget> > targets;
                std::shared_ptr< ::kml::MorphTargets> in_targets = in_mesh->morph_targets;
                if (in_targets.get())
                {
                    size_t tsz = in_targets->targets.size();
                    for (size_t j = 0; j < tsz; j++)
                    {
                        const std::shared_ptr< ::kml::MorphTarget>& in_target = in_targets->targets[j];
                        std::vector<float> pos(in_target->positions.size() * 3);
                        std::vector<float> nor(in_target->normals.size() * 3);
                        for (size_t k = 0; k < in_target->positions.size(); k++)
                        {
                            pos[3 * k + 0] = in_target->positions[k][0] - in_mesh->positions[k][0];
                            pos[3 * k + 1] = in_target->positions[k][1] - in_mesh->positions[k][1];
                            pos[3 * k + 2] = in_target->positions[k][2] - in_mesh->positions[k][2];
                        }
                        for (size_t k = 0; k < in_target->normals.size(); k++)
                        {
                            nor[3 * k + 0] = in_mesh->normals[k][0];
                            nor[3 * k + 1] = in_mesh->normals[k][1];
                            nor[3 * k + 2] = in_mesh->normals[k][2];
                        }

                        std::string tarName = in_targets->names[j]; // "target_" + IToS(nTar);
                        std::shared_ptr<MorphTarget> target(new MorphTarget(tarName, nTar));
                        target->SetWeight(in_targets->weights[j]);
                        //normal
                        {
                            std::string accName = "accessor_" + IToS(nAcc); //
                            std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                            const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(nor);
                            acc->SetBufferView(bufferView);
                            acc->SetCount(nor.size() / 3);
                            acc->SetType("VEC3");
                            acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                            acc->SetByteOffset(0);
                            //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                            std::vector<float> min(3);
                            std::vector<float> max(3);
                            GetMinMax(&min[0], &max[0], nor, 3);
                            acc->SetMin(min);
                            acc->SetMax(max);

                            accessors_.push_back(acc);
                            target->SetAccessor("NORMAL", acc);
                            nAcc++;
                        }
                        //position
                        {
                            std::string accName = "accessor_" + IToS(nAcc); //
                            std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                            const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(pos);
                            acc->SetBufferView(bufferView);
                            acc->SetCount(pos.size() / 3);
                            acc->SetType("VEC3");
                            acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                            acc->SetByteOffset(0);
                            //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                            std::vector<float> min(3);
                            std::vector<float> max(3);
                            GetMinMax(&min[0], &max[0], pos, 3);
                            acc->SetMin(min);
                            acc->SetMax(max);

                            accessors_.push_back(acc);
                            target->SetAccessor("POSITION", acc);
                            nAcc++;
                        }
                        targets.push_back(target);
                        this->morph_targets_.push_back(target);
                        nTar++;
                    }
                }
                return targets;
            }

            void RegisterAnimation(const std::shared_ptr< ::kml::Animation>& in_animation)
            {
                int nAcc = this->accessors_.size();
                int nAS = 0;
                int nAC = 0;
                int nAn = this->animations_.size();
                const auto& in_instructions = in_animation->GetInstructions();
                if (in_instructions.size() > 0)
                {
                    std::shared_ptr<Animation> animation(new Animation(in_animation->GetName(), nAC));

                    for (size_t i = 0; i < in_instructions.size(); i++)
                    {
                        const auto& in_instruction = in_instructions[i];

                        std::vector<std::shared_ptr<AnimationSampler> > samplers;
                        const auto& in_paths = in_instruction->GetPaths();
                        for (size_t j = 0; j < in_paths.size(); j++)
                        {
                            const auto& in_path = in_paths[j];

                            const auto key_curve = in_path->GetCurve("k");

                            std::string path_type = in_path->GetPathType();
                            std::shared_ptr<AnimationSampler> sampler(new AnimationSampler(in_animation->GetName(), nAS));
                            sampler->SetTargetPath(path_type);
                            sampler->SetInterpolation("LINEAR");
                            switch (key_curve->GetInterpolationType())
                            {
                            case kml::AnimationInterporationType::LINEAR:
                                sampler->SetInterpolation("LINEAR");
                                break;
                            case kml::AnimationInterporationType::STEP:
                                sampler->SetInterpolation("STEP");
                                break;
                            case kml::AnimationInterporationType::CUBICSPLINE:
                                sampler->SetInterpolation("CUBICSPLINE");
                                break;
                            }

                            {
                                std::vector<float> values = key_curve->GetValues();
                                std::string accName = "accessor_" + IToS(nAcc); //
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(values, -1);
                                acc->SetBufferView(bufferView);
                                acc->SetCount(values.size());
                                acc->SetType("SCALAR");
                                acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                                acc->SetByteOffset(0);
                                //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                                std::vector<float> min(1);
                                std::vector<float> max(1);
                                GetMinMax(&min[0], &max[0], values, 1);
                                acc->SetMin(min);
                                acc->SetMax(max);

                                accessors_.push_back(acc);
                                sampler->SetInputAccessor(acc);
                                nAcc++;
                            }

                            if (path_type == "translation" || path_type == "scale")
                            {
                                const std::vector<float>& x = in_path->GetCurve("x")->GetValues();
                                const std::vector<float>& y = in_path->GetCurve("y")->GetValues();
                                const std::vector<float>& z = in_path->GetCurve("z")->GetValues();
                                std::vector<float> values(3 * x.size());
                                for (size_t j = 0; j < x.size(); j++)
                                {
                                    values[3 * j + 0] = x[j];
                                    values[3 * j + 1] = y[j];
                                    values[3 * j + 2] = z[j];
                                }

                                std::string accName = "accessor_" + IToS(nAcc); //
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(values, -1);
                                acc->SetBufferView(bufferView);
                                acc->SetCount(values.size() / 3);
                                acc->SetType("VEC3");
                                acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                                acc->SetByteOffset(0);
                                //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                                std::vector<float> min(3);
                                std::vector<float> max(3);
                                GetMinMax(&min[0], &max[0], values, 3);
                                acc->SetMin(min);
                                acc->SetMax(max);

                                accessors_.push_back(acc);
                                sampler->SetOutputAccessor(acc);
                                nAcc++;
                            }
                            else if (path_type == "rotation")
                            {
                                const std::vector<float>& x = in_path->GetCurve("x")->GetValues();
                                const std::vector<float>& y = in_path->GetCurve("y")->GetValues();
                                const std::vector<float>& z = in_path->GetCurve("z")->GetValues();
                                const std::vector<float>& w = in_path->GetCurve("w")->GetValues();

                                std::vector<float> values(4 * x.size());
                                for (size_t j = 0; j < x.size(); j++)
                                {
                                    values[4 * j + 0] = x[j];
                                    values[4 * j + 1] = y[j];
                                    values[4 * j + 2] = z[j];
                                    values[4 * j + 3] = w[j];
                                }

                                std::string accName = "accessor_" + IToS(nAcc); //
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(values, -1);
                                acc->SetBufferView(bufferView);
                                acc->SetCount(values.size() / 4);
                                acc->SetType("VEC4");
                                acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                                acc->SetByteOffset(0);
                                //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                                std::vector<float> min(4);
                                std::vector<float> max(4);
                                GetMinMax(&min[0], &max[0], values, 4);
                                acc->SetMin(min);
                                acc->SetMax(max);

                                accessors_.push_back(acc);
                                sampler->SetOutputAccessor(acc);
                                nAcc++;
                            }
                            else if (path_type == "weights")
                            {
                                std::vector<float> values = in_path->GetCurve("w")->GetValues();

                                std::string accName = "accessor_" + IToS(nAcc); //
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(values, -1);
                                acc->SetBufferView(bufferView);
                                acc->SetCount(values.size());
                                acc->SetType("SCALAR");
                                acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                                acc->SetByteOffset(0);
                                //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                                std::vector<float> min(1);
                                std::vector<float> max(1);
                                GetMinMax(&min[0], &max[0], values, 1);
                                acc->SetMin(min);
                                acc->SetMax(max);

                                accessors_.push_back(acc);
                                sampler->SetOutputAccessor(acc);
                                nAcc++;
                            }

                            if (sampler->GetOutputAccessor().get())
                            {
                                samplers.push_back(sampler);
                                nAS++;
                            }
                        }

                        std::vector<std::shared_ptr<AnimationChannel> > channels;
                        const auto& targets = in_instruction->GetTargets();
                        for (size_t j = 0; j < targets.size(); j++)
                        {
                            for (size_t k = 0; k < samplers.size(); k++)
                            {
                                std::shared_ptr<AnimationChannel> channel(new AnimationChannel(in_animation->GetName(), nAC));
                                channel->SetTargetNode(nodeMap_[targets[j]->GetPath()]);
                                channel->SetSampler(samplers[k]);
                                channels.push_back(channel);
                                nAC++;
                            }
                        }

                        for (size_t j = 0; j < channels.size(); j++)
                        {
                            animation->AddChannel(channels[j]);
                        }
                        for (size_t j = 0; j < samplers.size(); j++)
                        {
                            animation->AddSampler(samplers[j]);
                        }
                    }

                    this->animations_.push_back(animation);
                }
            }

            void RegisterMesh(std::shared_ptr<Node>& node, const std::shared_ptr< ::kml::Node>& in_node, bool isDraco = false)
            {
                const std::shared_ptr< ::kml::Mesh>& in_mesh = in_node->GetMesh();
                if (in_mesh.get())
                {
                    if (in_mesh->morph_targets.get())
                    {
                        isDraco = false; //TODO:
                    }

                    int nMesh = meshes_.size();
                    std::string meshName = in_mesh->name;
                    std::shared_ptr<Mesh> mesh(new Mesh(meshName, nMesh));

                    if (in_mesh->materials.size())
                    {
                        int material_id = in_mesh->materials[0];
                        mesh->SetMaterialID(material_id);
                    }
                    else
                    {
                        int material_id = 0;
                        mesh->SetMaterialID(material_id);
                    }

                    std::vector<unsigned int> indices(in_mesh->pos_indices.size());
                    for (size_t i = 0; i < indices.size(); i++)
                    {
                        indices[i] = (unsigned int)in_mesh->pos_indices[i];
                    }

                    std::vector<float> positions(in_mesh->positions.size() * 3);
                    for (size_t i = 0; i < in_mesh->positions.size(); i++)
                    {
                        positions[3 * i + 0] = (float)in_mesh->positions[i][0];
                        positions[3 * i + 1] = (float)in_mesh->positions[i][1];
                        positions[3 * i + 2] = (float)in_mesh->positions[i][2];
                    }

                    std::vector<float> normals(in_mesh->normals.size() * 3);
                    for (size_t i = 0; i < in_mesh->normals.size(); i++)
                    {
                        normals[3 * i + 0] = (float)in_mesh->normals[i][0];
                        normals[3 * i + 1] = (float)in_mesh->normals[i][1];
                        normals[3 * i + 2] = (float)in_mesh->normals[i][2];
                    }
                    for (size_t i = 0; i < in_mesh->normals.size(); i++)
                    {
                        float x = normals[3 * i + 0];
                        float y = normals[3 * i + 1];
                        float z = normals[3 * i + 2];
                        float l = (x * x + y * y + z * z);
                        if (fabs(l) > 1e-6f)
                        {
                            l = 1.0f / std::sqrt(l);
                            normals[3 * i + 0] = x * l;
                            normals[3 * i + 1] = y * l;
                            normals[3 * i + 2] = z * l;
                        }
                    }

                    std::vector<float> texcoords;
                    if (in_mesh->texcoords.size() > 0)
                    {
                        texcoords.resize(in_mesh->texcoords.size() * 2);
                        for (size_t i = 0; i < in_mesh->texcoords.size(); i++)
                        {
                            texcoords[2 * i + 0] = (float)in_mesh->texcoords[i][0];
                            texcoords[2 * i + 1] = (float)in_mesh->texcoords[i][1];
                        }
                    }

                    int nAcc = accessors_.size();
                    {
                        //indices
                        std::string accName = "accessor_" + IToS(nAcc); //
                        std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                        if (!isDraco)
                        {
                            std::shared_ptr<BufferView> bv = this->AddBufferView(indices, GLTF_TARGET_ELEMENT_ARRAY_BUFFER);
                            acc->SetBufferView(bv);
                        }
                        else
                        {
                            std::shared_ptr<DracoTemporaryBuffer> bv(new DracoTemporaryBuffer((unsigned char*)(&indices[0]), sizeof(unsigned int) * indices.size()));
                            acc->SetDracoTemporaryBuffer(bv);
                        }
                        acc->SetCount(indices.size());
                        acc->SetType("SCALAR");
                        acc->SetComponentType(GLTF_COMPONENT_TYPE_UNSIGNED_INT); //5126
                        acc->SetByteOffset(0);
                        //acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));

                        unsigned int imin, imax;
                        GetMinMax(imin, imax, indices);
                        std::vector<float> min = {(float)imin};
                        std::vector<float> max = {(float)imax};
                        acc->SetMin(min);
                        acc->SetMax(max);

                        accessors_.push_back(acc);
                        mesh->SetAccessor("indices", acc);
                        nAcc++;
                    }
                    {
                        //normal
                        std::string accName = "accessor_" + IToS(nAcc); //
                        std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                        if (!isDraco)
                        {
                            std::shared_ptr<BufferView> bv = this->AddBufferView(normals, GLTF_TARGET_ARRAY_BUFFER);
                            acc->SetBufferView(bv);
                        }
                        else
                        {
                            std::shared_ptr<DracoTemporaryBuffer> bv(new DracoTemporaryBuffer((unsigned char*)(&normals[0]), sizeof(float) * normals.size()));
                            acc->SetDracoTemporaryBuffer(bv);
                        }
                        acc->SetCount(normals.size() / 3);
                        acc->SetType("VEC3");
                        acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                        acc->SetByteOffset(0);
                        //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                        std::vector<float> min(3);
                        std::vector<float> max(3);
                        GetMinMax(&min[0], &max[0], normals, 3);
                        acc->SetMin(min);
                        acc->SetMax(max);

                        accessors_.push_back(acc);
                        mesh->SetAccessor("NORMAL", acc);
                        nAcc++;
                    }
                    {
                        //position
                        std::string accName = "accessor_" + IToS(nAcc); //
                        std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                        if (!isDraco)
                        {
                            std::shared_ptr<BufferView> bv = this->AddBufferView(positions, GLTF_TARGET_ARRAY_BUFFER);
                            acc->SetBufferView(bv);
                        }
                        else
                        {
                            std::shared_ptr<DracoTemporaryBuffer> bv(new DracoTemporaryBuffer((unsigned char*)(&positions[0]), sizeof(float) * positions.size()));
                            acc->SetDracoTemporaryBuffer(bv);
                        }
                        acc->SetCount(positions.size() / 3);
                        acc->SetType("VEC3");
                        acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                        acc->SetByteOffset(0);
                        //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

                        std::vector<float> min(3);
                        std::vector<float> max(3);
                        GetMinMax(&min[0], &max[0], positions, 3);
                        acc->SetMin(min);
                        acc->SetMax(max);

                        accessors_.push_back(acc);
                        mesh->SetAccessor("POSITION", acc);
                        nAcc++;
                    }
                    if (texcoords.size() > 0)
                    {
                        //texcoord
                        std::string accName = "accessor_" + IToS(nAcc); //
                        std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                        if (!isDraco)
                        {
                            std::shared_ptr<BufferView> bv = this->AddBufferView(texcoords, GLTF_TARGET_ARRAY_BUFFER);
                            acc->SetBufferView(bv);
                        }
                        else
                        {
                            std::shared_ptr<DracoTemporaryBuffer> bv(new DracoTemporaryBuffer((unsigned char*)(&texcoords[0]), sizeof(float) * texcoords.size()));
                            acc->SetDracoTemporaryBuffer(bv);
                        }
                        acc->SetCount(texcoords.size() / 2);
                        acc->SetType("VEC2");
                        acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                        acc->SetByteOffset(0);
                        //acc->Set("byteStride", picojson::value((double)2 * sizeof(float)));

                        std::vector<float> min(2);
                        std::vector<float> max(2);
                        GetMinMax(&min[0], &max[0], texcoords, 2);
                        acc->SetMin(min);
                        acc->SetMax(max);

                        accessors_.push_back(acc);
                        mesh->SetAccessor("TEXCOORD_0", acc);
                        nAcc++;
                    }

                    std::shared_ptr< ::kml::SkinWeight> in_skin = in_mesh->skin_weight;
                    if (in_skin.get())
                    {
                        auto n = nodeMap_[in_skin->GetJointPaths()[0]];
                        auto joint = n->GetJoint();
                        auto skin = joint->GetSkin();
                        if (skin.get())
                        {
                            struct WeightSorter
                            {
                                bool operator()(const std::pair<int, float>& a, const std::pair<int, float>& b) const
                                {
                                    return a.second > b.second;
                                }
                            };

                            typedef ::kml::SkinWeight::WeightVertex WeightVertex;
                            typedef WeightVertex::const_iterator WeightIterator;
                            std::vector<unsigned short> joints;
                            std::vector<float> weights;
                            for (int i = 0; i < in_skin->weights.size(); i++)
                            {
                                std::vector<std::pair<int, float> > ww;
                                WeightIterator it = in_skin->weights[i].begin();
                                for (; it != in_skin->weights[i].end(); it++)
                                {
                                    std::string path = it->first;
                                    float weight = it->second;
                                    auto nn = nodeMap_[path];
                                    auto jj = nn->GetJoint();
                                    int index = std::max<int>(0, jj->GetIndexInSkin());
                                    ww.push_back(std::make_pair(index, weight));
                                }
                                std::sort(ww.begin(), ww.end(), WeightSorter());
                                unsigned short jx[4] = {};
                                float wx[4] = {};
                                int nweights = std::min<int>(4, ww.size());
                                for (int j = 0; j < nweights; j++)
                                {
                                    jx[j] = ww[j].first;
                                    wx[j] = ww[j].second;
                                }
                                float l = 0.0f;
                                for (int j = 0; j < 4; j++)
                                {
                                    l += wx[j];
                                }
                                l = 1.0 / std::max<float>(1e-16f, l);
                                for (int j = 0; j < 4; j++)
                                {
                                    wx[j] *= l;
                                }
                                for (int j = 0; j < 4; j++)
                                {
                                    joints.push_back(jx[j]);
                                    weights.push_back(wx[j]);
                                }
                            }

                            if (joints.size() > 0)
                            {
                                std::string accName = "accessor_" + IToS(nAcc); //
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                if (!isDraco)
                                {
                                    const std::shared_ptr<BufferView> bv = this->AddBufferView(joints, GLTF_TARGET_ARRAY_BUFFER);
                                    acc->SetBufferView(bv);
                                }
                                else
                                {
                                    std::shared_ptr<DracoTemporaryBuffer> bv(new DracoTemporaryBuffer((unsigned char*)(&joints[0]), sizeof(unsigned short) * joints.size()));
                                    acc->SetDracoTemporaryBuffer(bv);
                                }
                                acc->SetCount(joints.size() / 4);
                                acc->SetType("VEC4");
                                acc->SetComponentType(GLTF_COMPONENT_TYPE_UNSIGNED_SHORT); //5126
                                acc->SetByteOffset(0);
                                //acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));
                                /*
                                unsigned short min[4] = {}, max[4] = {};
                                GetMinMax(min, max, joints, 4);
                                acc->Set("min", picojson::value(ConvertToArray(min, 4)));
                                acc->Set("max", picojson::value(ConvertToArray(max, 4)));
                                */
                                accessors_.push_back(acc);
                                mesh->SetAccessor("JOINTS_0", acc);
                                nAcc++;
                            }

                            if (weights.size() > 0)
                            {
                                std::string accName = "accessor_" + IToS(nAcc); //
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                if (!isDraco)
                                {
                                    const std::shared_ptr<BufferView> bv = this->AddBufferView(weights, GLTF_TARGET_ARRAY_BUFFER);
                                    acc->SetBufferView(bv);
                                }
                                else
                                {
                                    std::shared_ptr<DracoTemporaryBuffer> bv(new DracoTemporaryBuffer((unsigned char*)(&weights[0]), sizeof(float) * weights.size()));
                                    acc->SetDracoTemporaryBuffer(bv);
                                }
                                acc->SetCount(weights.size() / 4);
                                acc->SetType("VEC4");
                                acc->SetComponentType(GLTF_COMPONENT_TYPE_FLOAT); //5126
                                acc->SetByteOffset(0);
                                //acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));
                                /*
                                float min[4] = {}, max[4] = {};
                                GetMinMax(min, max, weights, 4);
                                acc->Set("min", picojson::value(ConvertToArray(min, 4)));
                                acc->Set("max", picojson::value(ConvertToArray(max, 4)));
                                */

                                accessors_.push_back(acc);
                                mesh->SetAccessor("WEIGHTS_0", acc);
                                nAcc++;
                            }
                            node->SetSkin(skin);
                        }
                    }

                    {
                        std::vector<std::shared_ptr<MorphTarget> > targets = this->RegisterMorphTargets(in_mesh);
                        if (!targets.empty())
                        {
                            for (size_t i = 0; i < targets.size(); i++)
                            {
                                mesh->AddTarget(targets[i]);
                            }
                        }
                    }

                    if (isDraco)
                    {
                        std::shared_ptr<BufferView> bufferView = this->AddBufferViewDraco(mesh);
                        mesh->SetBufferView("draco", bufferView);
                        {
                            //clear temporay
                            static const char* ATTRS[] = {
                                "POSITION", "TEXCOORD_0", "NORMAL", "JOINTS_0", "WEIGHTS_0", NULL};
                            int i = 0;
                            while (ATTRS[i])
                            {
                                std::shared_ptr<Accessor> acc = mesh->GetAccessor(ATTRS[i]);
                                if (acc.get())
                                {
                                    std::shared_ptr<DracoTemporaryBuffer> bv = acc->GetDracoTemporaryBuffer();
                                    if (bv.get())
                                    {
                                        bv->ClearBytes();
                                    }
                                }
                                i++;
                            }
                        }
                    }

                    node->SetMesh(mesh);
                    this->meshes_.push_back(mesh);
                }
            }

        public:
            const std::vector<std::shared_ptr<Node> >& GetNodes() const
            {
                return nodes_;
            }
            const std::vector<std::shared_ptr<Mesh> >& GetMeshes() const
            {
                return meshes_;
            }
            const std::vector<std::shared_ptr<Accessor> >& GetAccessors() const
            {
                return accessors_;
            }
            const std::vector<std::shared_ptr<BufferView> >& GetBufferViews() const
            {
                return bufferViews_;
            }
            const std::vector<std::shared_ptr<Buffer> >& GetBuffers() const
            {
                return buffers_;
            }
            const std::vector<std::shared_ptr<TextureSampler> >& GetTextureSamplers() const
            {
                return texture_samplers_;
            }
            const std::vector<std::shared_ptr<Skin> >& GetSkins() const
            {
                return skins_;
            }
            std::vector<std::shared_ptr<Animation> > GetAnimations() const
            {
                return animations_;
            }

        public:
            void AddSkin(const std::shared_ptr<Skin>& skin)
            {
                skins_.push_back(skin);
            }
            void AddAccessor(const std::shared_ptr<Accessor>& acc)
            {
                accessors_.push_back(acc);
            }

        public:
            std::shared_ptr<Buffer> GetLastBuffer()
            {
                if (buffers_.empty())
                {
                    buffers_.push_back(std::shared_ptr<Buffer>(new Buffer(basename_, 0)));
                }
                return buffers_.back();
            }
            void AddNode(const std::shared_ptr<Node>& node)
            {
                nodeMap_[node->GetPath()] = node;
                nodes_.push_back(node);
            }

            const std::shared_ptr<BufferView>& AddBufferView(const std::vector<float>& vec, int target = GLTF_TARGET_ARRAY_BUFFER)
            {
                std::shared_ptr<Buffer> buffer = this->GetLastBuffer();
                int nBV = bufferViews_.size();
                std::string name = "bufferView_" + IToS(nBV); //
                std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
                size_t offset = buffer->GetSize();
                size_t length = sizeof(float) * vec.size();
                buffer->AddBytes((unsigned char*)(&vec[0]), length);
                bufferView->SetByteOffset(offset);
                bufferView->SetByteLength(length);
                bufferView->SetBuffer(buffers_[0]);
                bufferView->SetTarget(target);
                bufferViews_.push_back(bufferView);
                return bufferViews_.back();
            }

            const std::shared_ptr<BufferView>& AddBufferView(const std::vector<unsigned int>& vec, int target = GLTF_TARGET_ELEMENT_ARRAY_BUFFER)
            {
                std::shared_ptr<Buffer> buffer = this->GetLastBuffer();
                int nBV = bufferViews_.size();
                std::string name = "bufferView_" + IToS(nBV); //
                std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
                size_t offset = buffer->GetSize();
                size_t length = sizeof(unsigned int) * vec.size();
                buffer->AddBytes((unsigned char*)(&vec[0]), length);
                bufferView->SetByteOffset(offset);
                bufferView->SetByteLength(length);
                bufferView->SetBuffer(buffer);
                bufferView->SetTarget(target);
                bufferViews_.push_back(bufferView);
                return bufferViews_.back();
            }

            const std::shared_ptr<BufferView>& AddBufferView(const std::vector<unsigned short>& vec, int target = GLTF_TARGET_ARRAY_BUFFER)
            {
                std::shared_ptr<Buffer> buffer = this->GetLastBuffer();
                int nBV = bufferViews_.size();
                std::string name = "bufferView_" + IToS(nBV); //
                std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
                size_t offset = buffer->GetSize();
                size_t length = sizeof(unsigned short) * vec.size();
                buffer->AddBytes((unsigned char*)(&vec[0]), length);
                bufferView->SetByteOffset(offset);
                bufferView->SetByteLength(length);
                bufferView->SetBuffer(buffer);
                bufferView->SetTarget(target);
                bufferViews_.push_back(bufferView);
                return bufferViews_.back();
            }

            std::shared_ptr<BufferView> AddBufferViewDraco(const std::shared_ptr<Mesh>& mesh)
            {
                std::vector<unsigned char> bytes;
                if (!SaveToDraco(bytes, mesh))
                {
                    return std::shared_ptr<BufferView>();
                }

                std::shared_ptr<Buffer> buffer = this->GetLastBuffer();
                int nBV = bufferViews_.size();
                std::string name = "bufferView_" + IToS(nBV); //
                std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
                size_t offset = buffer->GetSize();
                size_t length = bytes.size();

                //4bytes
                Pad4BytesAlign(bytes);
                buffer->AddBytes(&bytes[0], bytes.size());

                bufferView->SetByteOffset(offset);
                bufferView->SetByteLength(length);
                bufferView->SetBuffer(buffer);
                bufferView->SetTarget(GLTF_TARGET_ARRAY_BUFFER);
                bufferViews_.push_back(bufferView);

                return bufferViews_.back();
            }

        protected:
            std::map<std::string, std::shared_ptr<Node> > nodeMap_;
            std::vector<std::shared_ptr<Node> > nodes_;
            std::vector<std::shared_ptr<Mesh> > meshes_;
            std::vector<std::shared_ptr<Accessor> > accessors_;
            std::vector<std::shared_ptr<BufferView> > bufferViews_;
            std::vector<std::shared_ptr<Buffer> > buffers_;
            std::vector<std::shared_ptr<Skin> > skins_;
            std::vector<std::shared_ptr<MorphTarget> > morph_targets_;
            std::vector<std::shared_ptr<Animation> > animations_;
            std::vector<std::shared_ptr<TextureSampler> > texture_samplers_;

            std::string basename_;
        };

        static int FindTextureIndex(const std::vector<std::shared_ptr<kml::Texture> >& texture_vec, const std::shared_ptr<kml::Texture>& tex)
        {
            for (int i = 0; i < (int)texture_vec.size(); i++)
            {
                if (texture_vec[i].get() == tex.get())
                {
                    return i;
                }
            }
            return -1;
        }

        static int FindImageIndex(const std::vector< std::string >& image_vec, const std::string& image_path)
        {
            for (int i = 0; i < (int)image_vec.size(); i++)
            {
                if (image_vec[i] == image_path)
                {
                    return i;
                }
            }
            return -1;
        }

        static std::shared_ptr<Node> CreateNodes(
            std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr< ::kml::Node> > >& node_pairs,
            ObjectRegisterer& reg,
            const std::shared_ptr< ::kml::Node>& in_node)
        {
            std::shared_ptr<Node> ret_node = reg.CreateNode(in_node);
            node_pairs.push_back(std::make_pair(ret_node, in_node));
            {
                auto& children = in_node->GetChildren();
                for (size_t i = 0; i < children.size(); i++)
                {
                    auto child_node = CreateNodes(node_pairs, reg, children[i]);
                    if (ret_node.get() && child_node.get())
                    {
                        ret_node->AddChild(child_node);
                    }
                }
            }
            return ret_node;
        }

        static void RegisterObjects(
            ObjectRegisterer& reg,
            const std::shared_ptr< ::kml::Node>& node,
            bool IsOutputBin,
            bool IsOutputDraco)
        {
            std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr< ::kml::Node> > > node_pairs;
            CreateNodes(node_pairs, reg, node);

            {
                const auto& skins = node->GetSkins();
                for (size_t i = 0; i < skins.size(); i++)
                {
                    reg.RegisterSkin(skins[i]);
                }
            }

            for (size_t i = 0; i < node_pairs.size(); i++)
            {
                if (IsOutputBin)
                {
                    reg.RegisterMesh(node_pairs[i].first, node_pairs[i].second, false);
                }
                else if (IsOutputDraco)
                {
                    reg.RegisterMesh(node_pairs[i].first, node_pairs[i].second, true);
                }
            }
            {
                const auto& animations = node->GetAnimations();
                for (size_t i = 0; i < animations.size(); i++)
                {
                    reg.RegisterAnimation(animations[i]);
                }
            }
        }

        static picojson::array GetFloatAsArray(const float* p, int sz)
        {
            picojson::array ar;
            for (int i = 0; i < sz; i++)
            {
                ar.push_back(picojson::value(p[i]));
            }

            return ar;
        }

        static bool AddTextureIfPresent(picojson::object &LTE_pbr_material /* inout */, const std::string &texName, const std::string &matName, const std::shared_ptr<kml::Material> &mat, const std::vector<std::shared_ptr<kml::Texture> >& texture_vec)
        {
            std::shared_ptr<kml::Texture> tex = mat->GetTexture(texName);
            if (tex)
            {
                const int nIndex = FindTextureIndex(texture_vec, tex);
                if (nIndex >= 0)
                {
                    picojson::object texObject;
                    texObject["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material[matName] = picojson::value(texObject);
                    return true;
                }
            }
            return false;
        }

        static picojson::value createLTE_pbr_material(const std::shared_ptr<kml::Material> mat, const std::vector<std::shared_ptr<kml::Texture> >& texture_vec)
        {
            picojson::object LTE_pbr_material;

            // base
            picojson::array baseColorFactor;
            baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorR")));
            baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorG")));
            baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorB")));
            LTE_pbr_material["baseWeight"] = picojson::value(mat->GetFloat("ai_baseWeight"));
            LTE_pbr_material["baseColor"] = picojson::value(baseColorFactor);
            LTE_pbr_material["diffuseRoughness"] = picojson::value(mat->GetFloat("ai_diffuseRoughness"));
            LTE_pbr_material["metalness"] = picojson::value(mat->GetFloat("ai_metalness"));
            std::shared_ptr<kml::Texture> ai_baseTex = mat->GetTexture("ai_baseColor");
            if (ai_baseTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_baseTex);
                if (nIndex >= 0)
                {
                    picojson::object baseColorTexture;
                    baseColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material["baseColorTexture"] = picojson::value(baseColorTexture);
                }
            }
            AddTextureIfPresent(LTE_pbr_material, "ai_baseWeightTex", "baseWeightTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_diffuseRoughnessTex", "diffuseRoughnessTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_metalnessTex", "metalnessTexture", mat, texture_vec);

            // specular
            picojson::array specularColorFactor;
            specularColorFactor.push_back(picojson::value(mat->GetFloat("ai_specularColorR")));
            specularColorFactor.push_back(picojson::value(mat->GetFloat("ai_specularColorG")));
            specularColorFactor.push_back(picojson::value(mat->GetFloat("ai_specularColorB")));
            LTE_pbr_material["specularWeight"] = picojson::value(mat->GetFloat("ai_specularWeight"));
            LTE_pbr_material["specularColor"] = picojson::value(specularColorFactor);
            LTE_pbr_material["specularRoughness"] = picojson::value(mat->GetFloat("ai_specularRoughness"));
            LTE_pbr_material["specularIOR"] = picojson::value(mat->GetFloat("ai_specularIOR"));
            LTE_pbr_material["specularRotation"] = picojson::value(mat->GetFloat("ai_specularRotation"));
            LTE_pbr_material["specularAnisotropy"] = picojson::value(mat->GetFloat("ai_specularAnisotropy"));
            std::shared_ptr<kml::Texture> ai_specularTex = mat->GetTexture("ai_specularColor");
            if (ai_specularTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_specularTex);
                if (nIndex >= 0)
                {
                    picojson::object specularColorTexture;
                    specularColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material["specularColorTexture"] = picojson::value(specularColorTexture);
                }
            }
            AddTextureIfPresent(LTE_pbr_material, "ai_specularWeightTex", "specularWeightTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_specularRoughnessTex", "specularRoughnessTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_specularIORTex", "specularIORTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_specularRotationTex", "specularRotationTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_specularAnisotropyTex", "specularAnisotropyTexture", mat, texture_vec);

            // transmission
            picojson::array transmissionColorFactor;
            transmissionColorFactor.push_back(picojson::value(mat->GetFloat("ai_transmissionColorR")));
            transmissionColorFactor.push_back(picojson::value(mat->GetFloat("ai_transmissionColorG")));
            transmissionColorFactor.push_back(picojson::value(mat->GetFloat("ai_transmissionColorB")));
            picojson::array transmissionScatter;
            transmissionScatter.push_back(picojson::value(mat->GetFloat("ai_transmissionScatterR")));
            transmissionScatter.push_back(picojson::value(mat->GetFloat("ai_transmissionScatterG")));
            transmissionScatter.push_back(picojson::value(mat->GetFloat("ai_transmissionScatterB")));
            LTE_pbr_material["transmissionWeight"] = picojson::value(mat->GetFloat("ai_transmissionWeight"));
            LTE_pbr_material["transmissionColor"] = picojson::value(transmissionColorFactor);
            LTE_pbr_material["transmissionDepth"] = picojson::value(mat->GetFloat("ai_transmissionDepth"));
            LTE_pbr_material["transmissionScatter"] = picojson::value(transmissionScatter);
            LTE_pbr_material["transmissionScatterAnisotropy"] = picojson::value(mat->GetFloat("ai_transmissionScatterAnisotropy"));
            LTE_pbr_material["transmissionExtraRoughness"] = picojson::value(mat->GetFloat("ai_transmissionExtraRoughness"));
            LTE_pbr_material["transmissionDispersion"] = picojson::value(mat->GetFloat("ai_transmissionDispersion"));
            LTE_pbr_material["transmissionAovs"] = picojson::value(mat->GetFloat("ai_transmissionAovs"));
            std::shared_ptr<kml::Texture> ai_transmissionTex = mat->GetTexture("ai_transmissionColor");
            if (ai_transmissionTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_transmissionTex);
                if (nIndex >= 0)
                {
                    picojson::object transmissionColorTexture;
                    transmissionColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material["transmissionColorTexture"] = picojson::value(transmissionColorTexture);
                }
            }
            std::shared_ptr<kml::Texture> ai_transmissionScatterTex = mat->GetTexture("ai_transmissionScatter");
            if (ai_transmissionScatterTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_transmissionScatterTex);
                if (nIndex >= 0)
                {
                    picojson::object transmissionScatterTexture;
                    transmissionScatterTexture["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material["transmissionScatterTexture"] = picojson::value(transmissionScatterTexture);
                }
            }

            AddTextureIfPresent(LTE_pbr_material, "ai_transmissionWeightTex", "transmissionWeightTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_transmissionDepthTex", "transmissionDepthTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_transmissionScatterAnisotropyTex", "transmissionScatterAnisotropyTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_transmissionExtraRoughnessTex", "transmissionExtraRoughnessTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_transmissionDispersionTex", "transmissionDispersionTexture", mat, texture_vec);

            // subsurface
            picojson::array subsurfaceColorFactor;
            subsurfaceColorFactor.push_back(picojson::value(mat->GetFloat("ai_subsurfaceColorR")));
            subsurfaceColorFactor.push_back(picojson::value(mat->GetFloat("ai_subsurfaceColorG")));
            subsurfaceColorFactor.push_back(picojson::value(mat->GetFloat("ai_subsurfaceColorB")));
            picojson::array subsurfaceRadius;
            subsurfaceRadius.push_back(picojson::value(mat->GetFloat("ai_subsurfaceRadiusR")));
            subsurfaceRadius.push_back(picojson::value(mat->GetFloat("ai_subsurfaceRadiusG")));
            subsurfaceRadius.push_back(picojson::value(mat->GetFloat("ai_subsurfaceRadiusB")));
            static const std::string stype[] = {
                std::string("diffusion"),
                std::string("randomwalk")};
            LTE_pbr_material["subsurfaceWeight"] = picojson::value(mat->GetFloat("ai_subsurfaceWeight"));
            LTE_pbr_material["subsurfaceColor"] = picojson::value(subsurfaceColorFactor);
            LTE_pbr_material["subsurfaceRadius"] = picojson::value(subsurfaceRadius);
            LTE_pbr_material["subsurfaceType"] = picojson::value(stype[mat->GetInteger("ai_subsurfaceType")]);
            LTE_pbr_material["subsurfaceScale"] = picojson::value(mat->GetFloat("ai_subsurfaceScale"));
            LTE_pbr_material["subsurfaceAnisotropy"] = picojson::value(mat->GetFloat("ai_subsurfaceAnisotropy"));
            {
              std::shared_ptr<kml::Texture> ai_subsurfaceColorTex = mat->GetTexture("ai_subsurfaceColor");
              if (ai_subsurfaceColorTex)
              {
                  const int nIndex = FindTextureIndex(texture_vec, ai_subsurfaceColorTex);
                  if (nIndex >= 0)
                  {
                      picojson::object subsurfaceColorTexture;
                      subsurfaceColorTexture["index"] = picojson::value((double)nIndex);
                      LTE_pbr_material["subsurfaceColorTexture"] = picojson::value(subsurfaceColorTexture);
                  }
              }
            }
            AddTextureIfPresent(LTE_pbr_material, "ai_subsurfaceWeightTex", "subsurfaceWeightTexture", mat, texture_vec);
            // no `Tex` prefix for input name.
            AddTextureIfPresent(LTE_pbr_material, "ai_subsurfaceRadius", "subsurfaceRadiusTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_subsurfaceScaleTex", "subsurfaceScaleTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_subsurfaceAnisotropyTex", "subsurfaceAnisotropyTexture", mat, texture_vec);


            // Coat
            picojson::array coatColor;
            coatColor.push_back(picojson::value(mat->GetFloat("ai_coatColorR")));
            coatColor.push_back(picojson::value(mat->GetFloat("ai_coatColorG")));
            coatColor.push_back(picojson::value(mat->GetFloat("ai_coatColorB")));
            picojson::array coatNormal;
            coatNormal.push_back(picojson::value(mat->GetFloat("ai_coatNormalX")));
            coatNormal.push_back(picojson::value(mat->GetFloat("ai_coatNormalY")));
            coatNormal.push_back(picojson::value(mat->GetFloat("ai_coatNormalZ")));
            LTE_pbr_material["coatWeight"] = picojson::value(mat->GetFloat("ai_coatWeight"));
            LTE_pbr_material["coatColor"] = picojson::value(coatColor);
            LTE_pbr_material["coatRoughness"] = picojson::value(mat->GetFloat("ai_coatRoughness"));
            LTE_pbr_material["coatIOR"] = picojson::value(mat->GetFloat("ai_coatIOR"));
            LTE_pbr_material["coatNormal"] = picojson::value(coatNormal);

            std::shared_ptr<kml::Texture> ai_coatColorTex = mat->GetTexture("ai_coatColor");
            if (ai_coatColorTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_coatColorTex);
                if (nIndex >= 0)
                {
                    picojson::object ai_coatColorTexture;
                    ai_coatColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material["coatColorTexture"] = picojson::value(ai_coatColorTexture);
                }
            }

            AddTextureIfPresent(LTE_pbr_material, "ai_coatWeightTex", "coatWeightTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_coatRoughnessTex", "coatRoughnessTexture", mat, texture_vec);
            AddTextureIfPresent(LTE_pbr_material, "ai_coatIORTex", "coatIORTexture", mat, texture_vec);

            // Emissive
            picojson::array emissiveColor;
            emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorR")));
            emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorG")));
            emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorB")));
            LTE_pbr_material["emissionWeight"] = picojson::value(mat->GetFloat("ai_emissionWeight"));
            LTE_pbr_material["emissionColor"] = picojson::value(emissiveColor);
            std::shared_ptr<kml::Texture> ai_emissionColorTex = mat->GetTexture("ai_emissionColor");
            if (ai_emissionColorTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_emissionColorTex);
                if (nIndex >= 0)
                {
                    picojson::object ai_emissionColorTexture;
                    ai_emissionColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material["emissionColorTexture"] = picojson::value(ai_emissionColorTexture);
                }
            }
            AddTextureIfPresent(LTE_pbr_material, "ai_emissionWeightTex", "emissionWeightTexture", mat, texture_vec);

            // Opacity map
            std::shared_ptr<kml::Texture> ai_opacityTex = mat->GetTexture("ai_opacity");
            if (ai_opacityTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_opacityTex);
                if (nIndex >= 0)
                {
                    picojson::object opacityColorTexture;
                    opacityColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_pbr_material["opacityTexture"] = picojson::value(opacityColorTexture);
                }
            }

            return picojson::value(LTE_pbr_material);
        }

        static picojson::value createLTE_hair_material(const std::shared_ptr<kml::Material> mat, const std::vector<std::shared_ptr<kml::Texture> >& texture_vec)
        {
            picojson::object LTE_hair_material;

            // base
            picojson::array baseColorFactor;
            baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorR")));
            baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorG")));
            baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorB")));
            LTE_hair_material["baseWeight"] = picojson::value(mat->GetFloat("ai_baseWeight"));
            LTE_hair_material["baseColor"] = picojson::value(baseColorFactor);
            std::shared_ptr<kml::Texture> ai_baseTex = mat->GetTexture("ai_baseColor");
            if (ai_baseTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_baseTex);
                if (nIndex >= 0)
                {
                    picojson::object baseColorTexture;
                    baseColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_hair_material["baseColorTexture"] = picojson::value(baseColorTexture);
                }
            }
            LTE_hair_material["melanin"] = picojson::value(mat->GetFloat("ai_melanin"));
            LTE_hair_material["melaninRedness"] = picojson::value(mat->GetFloat("ai_melaninRedness"));
            LTE_hair_material["melaninRandomize"] = picojson::value(mat->GetFloat("ai_melaninRandomize"));

            // specular
            LTE_hair_material["roughness"] = picojson::value(mat->GetFloat("ai_roughness"));
            LTE_hair_material["ior"] = picojson::value(mat->GetFloat("ai_ior"));
            LTE_hair_material["shift"] = picojson::value(mat->GetFloat("ai_shift"));

            // tint
            {
              picojson::array specularTintFactor;
              specularTintFactor.push_back(picojson::value(mat->GetFloat("ai_specularTintR")));
              specularTintFactor.push_back(picojson::value(mat->GetFloat("ai_specularTintG")));
              specularTintFactor.push_back(picojson::value(mat->GetFloat("ai_specularTintB")));
              LTE_hair_material["specularTint"] = picojson::value(specularTintFactor);
            }
            {
              picojson::array specular2TintFactor;
              specular2TintFactor.push_back(picojson::value(mat->GetFloat("ai_specular2TintR")));
              specular2TintFactor.push_back(picojson::value(mat->GetFloat("ai_specular2TintG")));
              specular2TintFactor.push_back(picojson::value(mat->GetFloat("ai_specular2TintB")));
              LTE_hair_material["specular2Tint"] = picojson::value(specular2TintFactor);
            }
            {
              picojson::array transmissionTintFactor;
              transmissionTintFactor.push_back(picojson::value(mat->GetFloat("ai_transmissionTintR")));
              transmissionTintFactor.push_back(picojson::value(mat->GetFloat("ai_transmissionTintG")));
              transmissionTintFactor.push_back(picojson::value(mat->GetFloat("ai_transmissionTintB")));
              LTE_hair_material["transmissionTint"] = picojson::value(transmissionTintFactor);
            }

            // Emissive
            picojson::array emissiveColor;
            emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorR")));
            emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorG")));
            emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorB")));
            LTE_hair_material["emissionWeight"] = picojson::value(mat->GetFloat("ai_emissionWeight"));
            LTE_hair_material["emissionColor"] = picojson::value(emissiveColor);
            std::shared_ptr<kml::Texture> ai_emissionColorTex = mat->GetTexture("ai_emissionColor");
            if (ai_emissionColorTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_emissionColorTex);
                if (nIndex >= 0)
                {
                    picojson::object ai_emissionColorTexture;
                    ai_emissionColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_hair_material["emissionColorTexture"] = picojson::value(ai_emissionColorTexture);
                }
            }

            // Opacity map
            std::shared_ptr<kml::Texture> ai_opacityTex = mat->GetTexture("ai_opacity");
            if (ai_opacityTex)
            {
                const int nIndex = FindTextureIndex(texture_vec, ai_opacityTex);
                if (nIndex >= 0)
                {
                    picojson::object opacityColorTexture;
                    opacityColorTexture["index"] = picojson::value((double)nIndex);
                    LTE_hair_material["opacityTexture"] = picojson::value(opacityColorTexture);
                }
            }

            return picojson::value(LTE_hair_material);
        }

        static picojson::value createLTE_material_extensions(const std::shared_ptr<kml::Material> mat, const std::vector<std::shared_ptr<kml::Texture> >& texture_vec)
        {
            // Output LTE_PBR_material
            picojson::object extensions;

            bool isHairMaterial = (mat->GetInteger("aiStandardHair") == 1) ? true : false;

            if (isHairMaterial) {
              extensions["LTE_aiStandardHair_material"] = createLTE_hair_material(mat, texture_vec);
            } else {
              // Fallback to default PBR material.
              extensions["LTE_PBR_material"] = createLTE_pbr_material(mat, texture_vec);
            }

            return picojson::value(extensions);
        }

        static picojson::array GetMatrixAsArray(const glm::mat4& mat)
        {
            return GetFloatAsArray(glm::value_ptr(mat), 16);
        }

        static bool NodeToGLTF(
            picojson::object& root,
            ObjectRegisterer& reg,
            const std::shared_ptr< ::kml::Node>& node,
            bool IsOutputBin,
            bool IsOutputDraco)
        {
            {
                RegisterObjects(reg, node, IsOutputBin, IsOutputDraco);
            }

            {
                root["scene"] = picojson::value((double)0);
            }

            // Scenes
            {
                picojson::array ar;

                picojson::object scene;
                picojson::array nodes_;
                const std::vector<std::shared_ptr<Node> >& nodes = reg.GetNodes();
                if (!nodes.empty())
                {
                    nodes_.push_back(picojson::value((double)0));
                }
                scene["nodes"] = picojson::value(nodes_);
                ar.push_back(picojson::value(scene));

                root["scenes"] = picojson::value(ar);
            }

            // Nodes
            {
                const std::vector<std::shared_ptr<Node> >& nodes = reg.GetNodes();
                picojson::array ar;
                for (size_t i = 0; i < nodes.size(); i++)
                {
                    const std::shared_ptr<Node>& n = nodes[i];
                    picojson::object nd;

                    std::string name = n->GetName();
                    nd["name"] = picojson::value(name);

                    if (n->GetTransform()->IsTRS())
                    {
                        glm::vec3 T = n->GetTransform()->GetT();
                        glm::quat R = n->GetTransform()->GetR();
                        glm::vec3 S = n->GetTransform()->GetS();
                        if (!IsZero(T))
                        {
                            nd["translation"] = picojson::value(GetFloatAsArray(glm::value_ptr(T), 3));
                        }
                        if (!IsZero(R - glm::quat(1, 0, 0, 0))) //wxyz
                        {
                            nd["rotation"] = picojson::value(GetFloatAsArray(glm::value_ptr(R), 4)); //xyzw
                        }
                        if (!IsZero(S - glm::vec3(1, 1, 1)))
                        {
                            nd["scale"] = picojson::value(GetFloatAsArray(glm::value_ptr(S), 3));
                        }
                    }
                    else
                    {
                        glm::mat4 mat = n->GetMatrix();
                        if (!IsZero(mat - glm::mat4(1)))
                        {
                            nd["matrix"] = picojson::value(GetMatrixAsArray(mat));
                        }
                    }

                    const auto& children = n->GetChildren();
                    if (children.size() > 0)
                    {
                        picojson::array ar_child;
                        for (int j = 0; j < children.size(); j++)
                        {
                            ar_child.push_back(picojson::value((double)children[j]->GetIndex()));
                        }

                        nd["children"] = picojson::value(ar_child);
                    }

                    const std::shared_ptr<Mesh>& mesh = n->GetMesh();
                    if (mesh.get() != NULL)
                    {
                        nd["mesh"] = picojson::value((double)mesh->GetIndex());
                    }

                    const std::shared_ptr<Skin>& skin = n->GetSkin();
                    if (skin.get() != NULL)
                    {
                        nd["skin"] = picojson::value((double)skin->GetIndex());
                    }

                    ar.push_back(picojson::value(nd));
                }
                root["nodes"] = picojson::value(ar);
            }

            // Meshes
            {
                const std::vector<std::shared_ptr<Mesh> >& meshes = reg.GetMeshes();
                //std::cout << meshes.size() << std::endl;
                picojson::array ar;
                for (size_t i = 0; i < meshes.size(); i++)
                {
                    const std::shared_ptr<Mesh>& mesh = meshes[i];
                    picojson::object nd;
                    nd["name"] = picojson::value(mesh->GetName());

                    picojson::object attributes;
                    {
                        attributes["NORMAL"] = picojson::value((double)mesh->GetAccessor("NORMAL")->GetIndex()); //picojson::value(mesh->GetAccessor("NORMAL")->GetName());
                        attributes["POSITION"] = picojson::value((double)mesh->GetAccessor("POSITION")->GetIndex());
                        std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
                        if (tex.get())
                        {
                            attributes["TEXCOORD_0"] = picojson::value((double)tex->GetIndex());
                        }

                        std::shared_ptr<Accessor> joints = mesh->GetAccessor("JOINTS_0");
                        std::shared_ptr<Accessor> weights = mesh->GetAccessor("WEIGHTS_0");
                        if (joints.get() && weights.get())
                        {
                            attributes["JOINTS_0"] = picojson::value((double)joints->GetIndex());
                            attributes["WEIGHTS_0"] = picojson::value((double)weights->GetIndex());
                        }
                    }

                    picojson::object primitive;
                    primitive["attributes"] = picojson::value(attributes);
                    primitive["indices"] = picojson::value((double)mesh->GetIndices()->GetIndex());
                    primitive["mode"] = picojson::value((double)mesh->GetMode());
                    primitive["material"] = picojson::value((double)mesh->GetMaterialID());

                    picojson::object extras;

                    std::vector<std::shared_ptr<MorphTarget> > targets = mesh->GetTargets();
                    if (!targets.empty())
                    {
                        picojson::array tar;
                        picojson::array war;
                        picojson::array nar;
                        for (size_t j = 0; j < targets.size(); j++)
                        {
                            picojson::object tnd;
                            tnd["NORMAL"] = picojson::value((double)targets[j]->GetAccessor("NORMAL")->GetIndex());
                            tnd["POSITION"] = picojson::value((double)targets[j]->GetAccessor("POSITION")->GetIndex());
                            tar.push_back(picojson::value(tnd));
                            war.push_back(picojson::value((double)targets[j]->GetWeight()));
                            nar.push_back(picojson::value(targets[j]->GetName()));
                        }
                        primitive["targets"] = picojson::value(tar);
                        nd["weights"] = picojson::value(war);

                        //nd["targetNames"] = picojson::value(nar);
                        extras["targetNames"] = picojson::value(nar);
                    }

                    std::shared_ptr<BufferView> bufferView = mesh->GetBufferView("draco");
                    if (bufferView.get())
                    {
                        picojson::object KHR_draco_mesh_compression;
                        KHR_draco_mesh_compression["bufferView"] = picojson::value((double)bufferView->GetIndex());

                        int nOrder = 0;
                        picojson::object attributes;
                        attributes["POSITION"] = picojson::value((double)nOrder++);
                        std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
                        if (tex.get())
                        {
                            attributes["TEXCOORD_0"] = picojson::value((double)nOrder++);
                        }
                        attributes["NORMAL"] = picojson::value((double)nOrder++);

                        std::shared_ptr<Accessor> joints = mesh->GetAccessor("JOINTS_0");
                        std::shared_ptr<Accessor> weights = mesh->GetAccessor("WEIGHTS_0");
                        if (joints.get() && weights.get())
                        {
                            attributes["JOINTS_0"] = picojson::value((double)nOrder++);
                            attributes["WEIGHTS_0"] = picojson::value((double)nOrder++);
                        }

                        KHR_draco_mesh_compression["attributes"] = picojson::value(attributes);

                        picojson::object extensions;
                        extensions["KHR_draco_mesh_compression"] = picojson::value(KHR_draco_mesh_compression);
                        primitive["extensions"] = picojson::value(extensions);
                    }

                    picojson::array primitives;
                    primitives.push_back(picojson::value(primitive));

                    nd["primitives"] = picojson::value(primitives);

                    if (!extras.empty())
                    {
                        nd["extras"] = picojson::value(extras);
                    }

                    ar.push_back(picojson::value(nd));
                }
                root["meshes"] = picojson::value(ar);
            }

            // Accessors
            {
                const std::vector<std::shared_ptr<Accessor> >& accessors = reg.GetAccessors();
                picojson::array ar;
                for (size_t i = 0; i < accessors.size(); i++)
                {
                    const std::shared_ptr<Accessor>& accessor = accessors[i];
                    picojson::object nd;
                    //nd["name"] = picojson::value(accessor->GetName());
                    std::shared_ptr<BufferView> bufferView = accessor->GetBufferView();
                    if (bufferView.get())
                    {
                        nd["bufferView"] = picojson::value((double)bufferView->GetIndex());
                    }
                    if (!accessor->IsDraco())
                    {
                        nd["byteOffset"] = picojson::value((double)accessor->GetByteOffset());
                    }
                    //nd["byteStride"] = accessor->Get("byteStride");
                    nd["componentType"] = picojson::value((double)accessor->GetComponentType());
                    nd["count"] = picojson::value((double)accessor->GetCount());
                    nd["type"] = picojson::value(accessor->GetType());
                    auto min_ = accessor->GetMin();
                    if (!min_.empty())
                    {
                        nd["min"] = picojson::value(ConvertToArray(min_));
                    }
                    auto max_ = accessor->GetMax();
                    if (!max_.empty())
                    {
                        nd["max"] = picojson::value(ConvertToArray(max_));
                    }
                    ar.push_back(picojson::value(nd));
                }
                root["accessors"] = picojson::value(ar);
            }

            // BufferViews
            {
                const std::vector<std::shared_ptr<BufferView> >& bufferViews = reg.GetBufferViews();
                picojson::array ar;
                for (size_t i = 0; i < bufferViews.size(); i++)
                {
                    const std::shared_ptr<BufferView>& bufferView = bufferViews[i];
                    picojson::object nd;
                    //nd["name"] = picojson::value(bufferView->GetName());
                    nd["buffer"] = picojson::value((double)bufferView->GetBuffer()->GetIndex());
                    nd["byteOffset"] = picojson::value((double)bufferView->GetByteOffset());
                    nd["byteLength"] = picojson::value((double)bufferView->GetByteLength());
                    int nBufferView = bufferView->GetTarget();
                    if (nBufferView >= 0)
                    {
                        nd["target"] = picojson::value((double)bufferView->GetTarget());
                    }
                    ar.push_back(picojson::value(nd));
                }
                root["bufferViews"] = picojson::value(ar);
            }

            // Buffers
            {
                picojson::array ar;
                const std::vector<std::shared_ptr<Buffer> >& buffers = reg.GetBuffers();
                for (size_t i = 0; i < buffers.size(); i++)
                {
                    const std::shared_ptr<Buffer>& buffer = buffers[i];
                    picojson::object nd;
                    //nd["name"] = picojson::value(buffer->GetName());
                    nd["byteLength"] = picojson::value((double)buffer->GetByteLength());
                    nd["uri"] = picojson::value(buffer->GetURI());
                    ar.push_back(picojson::value(nd));
                }
                root["buffers"] = picojson::value(ar);
            }

            // Skins
            {
                const std::vector<std::shared_ptr<Skin> >& skins = reg.GetSkins();
                picojson::array ar;
                for (size_t i = 0; i < skins.size(); i++)
                {
                    const std::shared_ptr<Skin>& skin = skins[i];
                    const std::vector<std::shared_ptr<Joint> >& joints = skin->GetJoints();
                    if (!joints.empty())
                    {
                        picojson::object nd;
                        nd["name"] = picojson::value(skin->GetName());
                        {
                            picojson::array nj;
                            for (size_t i = 0; i < joints.size(); i++)
                            {
                                int index = joints[i]->GetNode()->GetIndex();
                                nj.push_back(picojson::value((double)index));
                            }
                            nd["joints"] = picojson::value(nj);
                        }

                        nd["skeleton"] = picojson::value((double)skin->GetRootJoint()->GetNode()->GetIndex());
                        std::shared_ptr<Accessor> inverseBindMatricesAccessor = skin->GetAccessor("inverseBindMatrices");
                        if (inverseBindMatricesAccessor.get())
                        {
                            nd["inverseBindMatrices"] = picojson::value((double)inverseBindMatricesAccessor->GetIndex());
                        }

                        ar.push_back(picojson::value(nd));
                    }
                    else
                    {
                        continue;
                    }
                }
                if (!ar.empty())
                {
                    root["skins"] = picojson::value(ar);
                }
            }

            // Animations
            {
                const auto& animations = reg.GetAnimations();
                picojson::array ar;
                for (size_t i = 0; i < animations.size(); i++)
                {
                    const std::shared_ptr<Animation>& animation = animations[i];
                    picojson::object nd;
                    nd["name"] = picojson::value(animation->GetName());
                    {
                        picojson::array nchans;
                        const auto& chans = animation->GetChannels();
                        for (size_t j = 0; j < chans.size(); j++)
                        {
                            picojson::object n;
                            n["sampler"] = picojson::value((double)chans[j]->GetSampler()->GetIndex());
                            {
                                picojson::object ntarget;
                                ntarget["node"] = picojson::value((double)chans[j]->GetTargetNode()->GetIndex());
                                ntarget["path"] = picojson::value(chans[j]->GetTargetPath());
                                n["target"] = picojson::value(ntarget);
                            }
                            nchans.push_back(picojson::value(n));
                        }

                        picojson::array nsamps;
                        const auto& samplers = animation->GetSamplers();
                        for (size_t j = 0; j < samplers.size(); j++)
                        {
                            picojson::object n;
                            n["input"] = picojson::value((double)samplers[j]->GetInputAccessor()->GetIndex());
                            n["output"] = picojson::value((double)samplers[j]->GetOutputAccessor()->GetIndex());
                            n["interpolation"] = picojson::value(samplers[j]->GetInterpolation());
                            nsamps.push_back(picojson::value(n));
                        }

                        nd["channels"] = picojson::value(nchans);
                        nd["samplers"] = picojson::value(nsamps);
                    }
                    ar.push_back(picojson::value(nd));
                }
                if (!ar.empty())
                {
                    root["animations"] = picojson::value(ar);
                }
            }

            std::vector<std::shared_ptr<kml::Texture> > texture_vec;
            std::vector<std::string> image_vec;
            std::vector<std::string> cache_image_vec;
            GetTextures(texture_vec, node->GetMaterials());
            GetImages(image_vec, texture_vec);
            GetCacheImages(cache_image_vec, texture_vec);

            // Textures
            {
                picojson::array textures;
                for (size_t i = 0; i < texture_vec.size(); i++)
                {
                    std::shared_ptr<kml::Texture> in_tex = texture_vec[i];
                    std::shared_ptr<TextureSampler> sampler = reg.RegisterTextureSampler(in_tex);
                    std::string imagePath = in_tex->GetFilePath();

                    int nIndex = FindImageIndex(image_vec, imagePath);
                    if(nIndex >= 0)
                    {
                        picojson::object texture;
                        texture["sampler"] = picojson::value((double)sampler->GetIndex());
                        texture["source"] = picojson::value((double)nIndex);

                        // LTE ext: colorspace as extra attribute
                        picojson::object extras;
                        extras["colorSpace"] = picojson::value(in_tex->GetColorSpace());
                        texture["extras"] = picojson::value(extras);

                        textures.push_back(picojson::value(texture));
                    }
                }
                if (!textures.empty())
                {
                    root["textures"] = picojson::value(textures);
                }
            }

            // Images
            {
                picojson::array images;
                for (size_t i = 0; i < image_vec.size(); i++)
                {
                    std::string imagePath = image_vec[i];
                    std::string imageId = GetImageID(imagePath);
                    picojson::object image;
                    image["name"] = picojson::value(imageId);
                    image["uri"] = picojson::value(imagePath);

                    {
                        std::string cacheFilePath = cache_image_vec[i];
                        if (!cacheFilePath.empty())
                        {
                            //"extensions": {"KSK_preloadUri":{"uri":"Default_baseColor_pre.jpg"}}
                            picojson::object extensions;
                            picojson::object KSK_preloadUri;
                            KSK_preloadUri["uri"] = picojson::value(cacheFilePath);
                            extensions["KSK_preloadUri"] = picojson::value(KSK_preloadUri);
                            image["extensions"] = picojson::value(extensions);
                        }
                    }

                    images.push_back(picojson::value(image));
                }
                if (!images.empty())
                {
                    root["images"] = picojson::value(images);
                }
            }

            // Samplers
            {
                picojson::array samplers;
                const std::vector<std::shared_ptr<TextureSampler> >& tsamplers = reg.GetTextureSamplers();
                for (size_t i = 0; i < tsamplers.size(); i++)
                {
                    picojson::object sampler;
                    sampler["minFilter"] = picojson::value((double)tsamplers[i]->GetMinFilter());
                    sampler["magFilter"] = picojson::value((double)tsamplers[i]->GetMagFilter());
                    sampler["wrapS"] = picojson::value((double)tsamplers[i]->GetWrapS());
                    sampler["wrapT"] = picojson::value((double)tsamplers[i]->GetWrapT());
                    samplers.push_back(picojson::value(sampler));
                }
                if (!samplers.empty())
                {
                    root["samplers"] = picojson::value(samplers);
                }
            }

            // Materials
            {
                const auto& materials = node->GetMaterials();
                picojson::array ar;
                for (size_t i = 0; i < materials.size(); i++)
                {
                    const auto& mat = materials[i];
                    picojson::object nd;
                    nd["name"] = picojson::value(mat->GetName());

                    {
                        picojson::array emissiveFactor;
                        float R = mat->GetValue("Emission.R");
                        float G = mat->GetValue("Emission.G");
                        float B = mat->GetValue("Emission.B");

                        emissiveFactor.push_back(picojson::value(R));
                        emissiveFactor.push_back(picojson::value(G));
                        emissiveFactor.push_back(picojson::value(B));
                        nd["emissiveFactor"] = picojson::value(emissiveFactor);
                    }

                    picojson::object pbrMetallicRoughness;

                    std::shared_ptr<kml::Texture> tex = mat->GetTexture("BaseColor");
                    if (tex)
                    {
                        int nIndex = FindTextureIndex(texture_vec, tex);
                        if (nIndex >= 0)
                        {
                            picojson::object baseColorTexture;
                            baseColorTexture["index"] = picojson::value((double)nIndex);
                            pbrMetallicRoughness["baseColorTexture"] = picojson::value(baseColorTexture);
                        }
                    }

                    std::shared_ptr<kml::Texture> normaltex = mat->GetTexture("Normal");
                    if (normaltex)
                    {
                        int nIndex = FindTextureIndex(texture_vec, normaltex);
                        if (nIndex >= 0)
                        {
                            picojson::object normalTexture;
                            normalTexture["index"] = picojson::value((double)nIndex);
                            nd["normalTexture"] = picojson::value(normalTexture);
                        }
                    }

                    {
                        picojson::array colorFactor;
                        float R = mat->GetValue("BaseColor.R");
                        float G = mat->GetValue("BaseColor.G");
                        float B = mat->GetValue("BaseColor.B");
                        float A = mat->GetValue("BaseColor.A");

                        colorFactor.push_back(picojson::value(R));
                        colorFactor.push_back(picojson::value(G));
                        colorFactor.push_back(picojson::value(B));
                        colorFactor.push_back(picojson::value(A));
                        pbrMetallicRoughness["baseColorFactor"] = picojson::value(colorFactor);

                        if (A >= 1.0f)
                        {
                            nd["alphaMode"] = picojson::value("OPAQUE");
                        }
                        else
                        {
                            nd["alphaMode"] = picojson::value("BLEND");
                        }
                    }

                    pbrMetallicRoughness["metallicFactor"] = picojson::value(mat->GetFloat("metallicFactor"));
                    pbrMetallicRoughness["roughnessFactor"] = picojson::value(mat->GetFloat("roughnessFactor"));
                    nd["pbrMetallicRoughness"] = picojson::value(pbrMetallicRoughness);

                    // LTE extenstion
                    nd["extensions"] = createLTE_material_extensions(mat, texture_vec);

                    ar.push_back(picojson::value(nd));
                }
                root["materials"] = picojson::value(ar);
            }

            return true;
        }

        static picojson::value makeVec4Value(float x, float y, float z, float w)
        {
            picojson::array v4;
            v4.push_back(picojson::value(x));
            v4.push_back(picojson::value(y));
            v4.push_back(picojson::value(z));
            v4.push_back(picojson::value(w));
            return picojson::value(v4);
        }

        typedef const char* PCTR;
        struct JointMap
        {
            PCTR szVRMJointKey;
            PCTR szSubStrs[5];
        };

        static int FindJointKeyIndex(const JointMap JointMaps[], const char* key)
        {
            int i = 0;
            while (JointMaps[i].szVRMJointKey)
            {
                if (strcmp(JointMaps[i].szVRMJointKey, key) == 0)
                {
                    return i;
                }
                i++;
            }
            return -1;
        }

        static int FindVRNJointIndex(const std::vector<std::string>& joint_names, const std::string& key)
        {
            static const JointMap JointMaps[] = {
                {"hips", {"hip", "pelvis", NULL, NULL, NULL}},
                {"leftUpperLeg", {"upperleg", "upleg", NULL, NULL, NULL}},
                {"rightUpperLeg", {"upperleg", "upleg", NULL, NULL, NULL}},
                {"leftLowerLeg", {"lowerleg", "leftleg", NULL, NULL, NULL}},
                {"rightLowerLeg", {"lowerleg", "rightleg", NULL, NULL, NULL}},
                {"leftFoot", {"foot", NULL, NULL, NULL, NULL}},
                {"rightFoot", {"foot", NULL, NULL, NULL, NULL}},
                {"spine", {"spine", NULL, NULL, NULL, NULL}},
                {"chest", {"chest", "spine1", NULL, NULL, NULL}},
                {"neck", {"neck", NULL, NULL, NULL, NULL}},
                {"head", {"head", NULL, NULL, NULL, NULL}},
                {"leftShoulder", {"shoulder", NULL, NULL, NULL, NULL}},
                {"rightShoulder", {"shoulder", NULL, NULL, NULL, NULL}},
                {"leftUpperArm", {"upperarm", "leftarm", NULL, NULL, NULL}},
                {"rightUpperArm", {"upperarm", "rightarm", NULL, NULL, NULL}},
                {"leftLowerArm", {"lowerarm", "forearm", NULL, NULL, NULL}},
                {"rightLowerArm", {"lowerarm", "forearm", NULL, NULL, NULL}},
                {"leftHand", {"hand", NULL, NULL, NULL, NULL}},
                {"rightHand", {"hand", NULL, NULL, NULL, NULL}},
                {"leftToes", {"toe", NULL, NULL, NULL, NULL}},
                {"rightToes", {"toe", NULL, NULL, NULL, NULL}},
                {"leftEye", {"eye", NULL, NULL, NULL, NULL}},
                {"rightEye", {"eye", NULL, NULL, NULL, NULL}},
                {"jaw", {"jaw", NULL, NULL, NULL, NULL}},

                {"leftThumbProximal", {"thumbproximal", "thumb1", NULL, NULL, NULL}},
                {"leftThumbIntermediate", {"thumbintermediate", "thumb2", NULL, NULL, NULL}},
                {"leftThumbDistal", {"thumbdistal", "thumb3", NULL, NULL, NULL}},

                {"leftIndexProximal", {"indexproximal", "index1", NULL, NULL, NULL}},
                {"leftIndexIntermediate", {"indexintermediate", "index2", NULL, NULL, NULL}},
                {"leftIndexDistal", {"indexdistal", "index3", NULL, NULL, NULL}},

                {"leftMiddleProximal", {"middleproximal", "middle1", NULL, NULL, NULL}},
                {"leftMiddleIntermediate", {"middleintermediate", "middle2", NULL, NULL, NULL}},
                {"leftMiddleDistal", {"middledistal", "middle3", NULL, NULL, NULL}},

                {"leftRingProximal", {"ringproximal", "ring1", NULL, NULL, NULL}},
                {"leftRingIntermediate", {"ringintermediate", "ring2", NULL, NULL, NULL}},
                {"leftRingDistal", {"ringbdistal", "ring3", NULL, NULL, NULL}},

                {"leftLittleProximal", {"littleproximal", "little1", "pinky1", NULL, NULL}},
                {"leftLittleIntermediate", {"littleintermediate", "little2", "pinky2", NULL, NULL}},
                {"leftLittleDistal", {"littledistal", "little3", "pinky3", NULL, NULL}},

                {"rightThumbProximal", {"thumbproximal", "thumb1", NULL, NULL, NULL}},
                {"rightThumbIntermediate", {"thumbintermediate", "thumb2", NULL, NULL, NULL}},
                {"rightThumbDistal", {"thumbdistal", "thumb3", NULL, NULL, NULL}},

                {"rightIndexProximal", {"indexproximal", "index1", NULL, NULL, NULL}},
                {"rightIndexIntermediate", {"indexintermediate", "index2", NULL, NULL, NULL}},
                {"rightIndexDistal", {"indexdistal", "index3", NULL, NULL, NULL}},

                {"rightMiddleProximal", {"middleproximal", "middle1", NULL, NULL, NULL}},
                {"rightMiddleIntermediate", {"middleintermediate", "middle2", NULL, NULL, NULL}},
                {"rightMiddleDistal", {"middledistal", "middle3", NULL, NULL, NULL}},

                {"rightRingProximal", {"ringproximal", "ring1", NULL, NULL, NULL}},
                {"rightRingIntermediate", {"ringintermediate", "ring2", NULL, NULL, NULL}},
                {"rightRingDistal", {"ringbdistal", "ring3", NULL, NULL, NULL}},

                {"rightLittleProximal", {"littleproximal", "little1", "pinky1", NULL, NULL}},
                {"rightLittleIntermediate", {"littleintermediate", "little2", "pinky2", NULL, NULL}},
                {"rightLittleDistal", {"littledistal", "little3", "pinky3", NULL, NULL}},

                {"upperChest", {"upperchest", "spine2", NULL, NULL, NULL}},

                {NULL, {NULL, NULL, NULL, NULL, NULL}}};

            static const PCTR LeftKeys[] = {"l_", "left", NULL};
            static const PCTR RightKeys[] = {"r_", "right", NULL};

            int key_index = FindJointKeyIndex(JointMaps, key.c_str());
            const PCTR* subStrs = JointMaps[key_index].szSubStrs;

            for (size_t i = 0; i < joint_names.size(); i++)
            {
                std::string joint_name = joint_names[i];
                if (strstr(key.c_str(), "left") != NULL)
                {
                    bool bContinue = false;
                    int k = 0;
                    while (LeftKeys[k] && !bContinue)
                    {
                        if (strstr(joint_name.c_str(), LeftKeys[k]) != NULL)
                        {
                            bContinue = true;
                            break;
                        }
                        k++;
                    }
                    if (!bContinue)
                    {
                        continue;
                    }
                }
                else if (strstr(key.c_str(), "right") != NULL)
                {
                    bool bContinue = false;
                    int k = 0;
                    while (RightKeys[k] && !bContinue)
                    {
                        if (strstr(joint_name.c_str(), RightKeys[k]) != NULL)
                        {
                            bContinue = true;
                            break;
                        }
                        k++;
                    }
                    if (!bContinue)
                    {
                        continue;
                    }
                }

                {
                    int j = 0;
                    while (subStrs[j])
                    {
                        std::string subKey = std::string(subStrs[j]);
                        if (strstr(joint_name.c_str(), subKey.c_str()) != NULL)
                        {
                            if (key == "spine")
                            {
                                if (strstr(joint_name.c_str(), "spine1") != NULL)
                                    continue;
                                if (strstr(joint_name.c_str(), "spine2") != NULL)
                                    continue;
                            }

                            return i;
                        }
                        j++;
                    }
                }
            }

            return -1;
        }

        static bool WriteVRMMetaInfo(picojson::object& root_object, const std::shared_ptr< ::kml::Node>& node, const std::shared_ptr<Options>& opts)
        {
            picojson::object extensions;
            auto ext = root_object.find("extensions");
            if (ext == root_object.end())
            {
                extensions = picojson::object();
            }
            else
            {
                extensions = root_object["extensions"].get<picojson::object>();
            }

            picojson::object VRM;
            {
                auto asset = root_object["asset"].get<picojson::object>();
                VRM["exporterVersion"] = asset["generator"];
            }

            {
                picojson::object meta;
                {
                    std::string s = opts->GetString("vrm_product_title", "");
                    meta["title"] = picojson::value(s);
                }
                {
                    std::string s = opts->GetString("vrm_product_version", "");
                    meta["version"] = picojson::value(s);
                }
                {
                    std::string s = opts->GetString("vrm_product_author", "");
                    meta["author"] = picojson::value(s);
                }
                {
                    std::string s = opts->GetString("vrm_product_contact_information", "");
                    meta["contactInformation"] = picojson::value(s);
                }
                {
                    std::string s = opts->GetString("vrm_product_reference", "");
                    meta["reference"] = picojson::value(s);
                }

                //TODO: This is a reference of a thumbnail.
                meta["texture"] = picojson::value((double)-1);

                {
                    int vrm_license_allowed_user_name = opts->GetInt("vrm_license_allowed_user_name", 2);
                    switch (vrm_license_allowed_user_name)
                    {
                    case 0:
                        meta["allowedUserName"] = picojson::value("OnlyAuthor");
                        break;
                    case 1:
                        meta["allowedUserName"] = picojson::value("ExplictlyLicensedPerson");
                        break;
                    case 2:
                        meta["allowedUserName"] = picojson::value("Everyone");
                        break;
                    }

                    int vrm_license_violent_usage = opts->GetInt("vrm_license_violent_usage", 1);
                    switch (vrm_license_violent_usage)
                    {
                    case 0:
                        meta["violentUsageName"] = meta["violentUssageName"] = picojson::value("Disallow");
                        break;
                    case 1:
                        meta["violentUsageName"] = meta["violentUssageName"] = picojson::value("Allow");
                        break;
                    }

                    int vrm_license_sexual_usage = opts->GetInt("vrm_license_sexual_usage", 1);
                    switch (vrm_license_sexual_usage)
                    {
                    case 0:
                        meta["sexualUsageName"] = meta["sexualUssageName"] = picojson::value("Disallow");
                        break;
                    case 1:
                        meta["sexualUsageName"] = meta["sexualUssageName"] = picojson::value("Allow");
                        break;
                    }

                    int vrm_license_commercial_usage = opts->GetInt("vrm_license_commercial_usage", 1);
                    switch (vrm_license_commercial_usage)
                    {
                    case 0:
                        meta["commercialUsageName"] = meta["commercialUssageName"] = picojson::value("Disallow");
                        break;
                    case 1:
                        meta["commercialUsageName"] = meta["commercialUssageName"] = picojson::value("Allow");
                        break;
                    }
                }
                {
                    std::string url = opts->GetString("vrm_license_other_permission_url", "");
                    meta["otherPermissionUrl"] = picojson::value(url);
                }
                {
                    std::string type = opts->GetString("vrm_license_license_type", "");
                    meta["licenseName"] = picojson::value(type);
                }
                {
                    std::string url = opts->GetString("vrm_license_other_license_url", "");
                    meta["otherLicenseUrl"] = picojson::value(url);
                }

                VRM["meta"] = picojson::value(meta);
            }
            {
                picojson::object humanoid;
                picojson::array humanBones;
                static const char* boneNames[] = {
                    "hips", "leftUpperLeg", "rightUpperLeg", "leftLowerLeg", "rightLowerLeg",
                    "leftFoot", "rightFoot", "spine", "chest", "neck", "head", "leftShoulder",
                    "rightShoulder", "leftUpperArm", "rightUpperArm", "leftLowerArm", "rightLowerArm",
                    "leftHand", "rightHand", "leftToes", "rightToes", "leftEye", "rightEye", "jaw",
                    "leftThumbProximal", "leftThumbIntermediate", "leftThumbDistal", "leftIndexProximal",
                    "leftIndexIntermediate", "leftIndexDistal", "leftMiddleProximal", "leftMiddleIntermediate",
                    "leftMiddleDistal", "leftRingProximal", "leftRingIntermediate", "leftRingDistal",
                    "leftLittleProximal", "leftLittleIntermediate", "leftLittleDistal", "rightThumbProximal",
                    "rightThumbIntermediate", "rightThumbDistal", "rightIndexProximal", "rightIndexIntermediate",
                    "rightIndexDistal", "rightMiddleProximal", "rightMiddleIntermediate", "rightMiddleDistal",
                    "rightRingProximal", "rightRingIntermediate", "rightRingDistal", "rightLittleProximal",
                    "rightLittleIntermediate", "rightLittleDistal", "upperChest"};

                std::vector<std::string> joint_names;
                {
                    auto& nodes = root_object["nodes"].get<picojson::array>();
                    for (size_t i = 0; i < nodes.size(); i++)
                    {
                        auto& node = nodes[i].get<picojson::object>();
                        std::string name = node["name"].get<std::string>();
                        std::transform(name.cbegin(), name.cend(), name.begin(), tolower);
                        joint_names.push_back(name);
                    }
                }

                for (int i = 0; i < sizeof(boneNames) / sizeof(const char*); ++i)
                {
                    int idx = FindVRNJointIndex(joint_names, boneNames[i]);
                    if (idx >= 0)
                    {
                        picojson::object info;
                        info["bone"] = picojson::value(boneNames[i]);
                        info["node"] = picojson::value((double)idx);
                        info["useDefaultValues"] = picojson::value(true); // what's this?
                        humanBones.push_back(picojson::value(info));
                    }
                }
                humanoid["humanBones"] = picojson::value(humanBones);
                /*
                "armStretch": 0.05,
                "legStretch": 0.05,
                "upperArmTwist": 0.5,
                "lowerArmTwist": 0.5,
                "upperLegTwist": 0.5,
                "lowerLegTwist": 0.5,
                "feetSpacing": 0,
                "hasTranslationDoF": false
                */
                humanoid["armStretch"] = picojson::value((double)0.05);
                humanoid["legStretch"] = picojson::value((double)0.05);
                humanoid["upperArmTwist"] = picojson::value((double)0.05);
                humanoid["lowerArmTwist"] = picojson::value((double)0.05);
                humanoid["upperLegTwist"] = picojson::value((double)0.05);
                humanoid["lowerLegTwist"] = picojson::value((double)0.05);
                humanoid["feetSpacing"] = picojson::value((double)0);
                humanoid["hasTranslationDoF"] = picojson::value((bool)false);

                VRM["humanoid"] = picojson::value(humanoid);
                /*}

            {*/
                picojson::object firstPerson;
                firstPerson["firstPersonBone"] = picojson::value((double)FindVRNJointIndex(joint_names, "head")); //picojson::value(-1.0);
                picojson::object firstPersonBoneOffset;
                firstPersonBoneOffset["x"] = picojson::value(0.0);
                firstPersonBoneOffset["y"] = picojson::value(0.0); //head->GetGlobalMatrix()[3][1];
                firstPersonBoneOffset["z"] = picojson::value(0.0);
                firstPerson["firstPersonBoneOffset"] = picojson::value(firstPersonBoneOffset);
                picojson::array meshAnnotations;
                {
                    const auto meshes = root_object["meshes"].get<picojson::array>();
                    for (size_t i = 0; i < meshes.size(); i++)
                    {
                        picojson::object ann;
                        ann["mesh"] = picojson::value((double)i);
                        ann["firstPersonFlag"] = picojson::value("Auto");
                        meshAnnotations.push_back(picojson::value(ann));
                    }
                }

                firstPerson["meshAnnotations"] = picojson::value(meshAnnotations);
                firstPerson["lookAtTypeName"] = picojson::value("Bone");

                const float curve_floats[] = {0, 0, 0, 1, 1, 1, 1, 0};
                picojson::array curve = ConvertToArray(curve_floats, 8);

                picojson::object lookAtHorizontalInner;
                lookAtHorizontalInner["xRange"] = picojson::value(90.0);
                lookAtHorizontalInner["yRange"] = picojson::value(10.0);
                lookAtHorizontalInner["curve"] = picojson::value(curve);
                firstPerson["lookAtHorizontalInner"] = picojson::value(lookAtHorizontalInner);

                picojson::object lookAtHorizontalOuter;
                lookAtHorizontalOuter["xRange"] = picojson::value(90.0);
                lookAtHorizontalOuter["yRange"] = picojson::value(10.0);
                lookAtHorizontalOuter["curve"] = picojson::value(curve);
                firstPerson["lookAtHorizontalOuter"] = picojson::value(lookAtHorizontalOuter);

                picojson::object lookAtVerticalDown;
                lookAtVerticalDown["xRange"] = picojson::value(90.0);
                lookAtVerticalDown["yRange"] = picojson::value(10.0);
                lookAtVerticalDown["curve"] = picojson::value(curve);
                firstPerson["lookAtVerticalDown"] = picojson::value(lookAtVerticalDown);

                picojson::object lookAtVerticalUp;
                lookAtVerticalUp["xRange"] = picojson::value(90.0);
                lookAtVerticalUp["yRange"] = picojson::value(10.0);
                lookAtVerticalUp["curve"] = picojson::value(curve);
                firstPerson["lookAtVerticalUp"] = picojson::value(lookAtVerticalUp);

                VRM["firstPerson"] = picojson::value(firstPerson);
            }

            {
                picojson::object blendShapeMaster;

                picojson::array blendShapeGroups;
                picojson::object shapeinfo;
                static const char* names[] = {"Neutral", "A", "I", "U", "E", "O"};
                for (int i = 0; i < sizeof(names) / sizeof(const char*); ++i)
                {
                    shapeinfo["name"] = picojson::value(names[i]);
                    shapeinfo["presetName"] = picojson::value("unknown");
                    shapeinfo["binds"] = picojson::value(picojson::array());
                    shapeinfo["materialValues"] = picojson::value(picojson::array());
                    blendShapeGroups.push_back(picojson::value(shapeinfo));
                }
                blendShapeMaster["blendShapeGroups"] = picojson::value(blendShapeGroups);
                VRM["blendShapeMaster"] = picojson::value(blendShapeMaster);
            }

            {
                picojson::object secondaryAnimation;
                secondaryAnimation["boneGroups"] = picojson::value(picojson::array());
                secondaryAnimation["colliderGroups"] = picojson::value(picojson::array());
                VRM["secondaryAnimation"] = picojson::value(secondaryAnimation);
            }

            {
                const auto& nmaterials = root_object["materials"].get<picojson::array>();
                const auto& materials = node->GetMaterials();
                picojson::array materialProperties;
                for (size_t i = 0; i < materials.size(); ++i)
                {
                    auto nmat = nmaterials[i].get<picojson::object>();
                    const auto& imat = materials[i];
                    picojson::object mat;
                    mat["name"] = picojson::value(materials[i]->GetName());
                    mat["renderQueue"] = picojson::value(2000.0);
                    mat["shader"] = picojson::value("Standard");
                    //mat["shader"] = picojson::value("VRM/MToon");
                    picojson::object floatProperties;

                    mat["floatProperties"] = picojson::value(floatProperties);

                    /*
                    floatProperties["_Cutoff"] = picojson::value(0.5);
                    floatProperties["_BumpScale"] = picojson::value(1.0);
                    floatProperties["_ReceiveShadowRate"] = picojson::value(1.0);
                    floatProperties["_ShadeShift"] = picojson::value(-0.3);
                    floatProperties["_ShadeToony"] = picojson::value(0.0);
                    floatProperties["_LightColorAttenuation"] = picojson::value(0.0);
                    floatProperties["_OutlineWidth"] = picojson::value(0.172);
                    floatProperties["_OutlineScaledMaxDistance"] = picojson::value(2.0);// scale?
                    floatProperties["_OutlineLightingMix"] = picojson::value(1.0);
                    floatProperties["_DebugMode"] = picojson::value(0.0);
                    floatProperties["_BlendMode"] = picojson::value(1.0);
                    floatProperties["_OutlineWidthMode"] = picojson::value(0.0);
                    floatProperties["_OutlineColorMode"] = picojson::value(0.0);
                    floatProperties["_CullMode"] = picojson::value(0.0);
                    floatProperties["_OutlineCullMode"] = picojson::value(1.0);
                    floatProperties["_SrcBlend"] = picojson::value(1.0);
                    floatProperties["_DstBlend"] = picojson::value(0.0);
                    floatProperties["_ZWrite"] = picojson::value(1.0);
                    floatProperties["_IsFirstSetup"] = picojson::value(0.0);
                    */

                    picojson::object vectorProperties;
                    {
                        float R = imat->GetValue("BaseColor.R");
                        float G = imat->GetValue("BaseColor.G");
                        float B = imat->GetValue("BaseColor.B");
                        float A = imat->GetValue("BaseColor.A");
                        vectorProperties["_Color"] = makeVec4Value(R, G, B, A);
                    }
                    {
                        float R = imat->GetValue("Emission.R");
                        float G = imat->GetValue("Emission.G");
                        float B = imat->GetValue("Emission.B");
                        float A = 1.0f;
                        vectorProperties["_EmissionColor"] = makeVec4Value(R, G, B, A);
                    }
                    /*
                    vectorProperties["_ShadeColor"] = makeVec4Value(0.2, 0.2, 0.2, 1.0);
                    vectorProperties["_MainTex"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    vectorProperties["_ShadeTexture"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    vectorProperties["_BumpMap"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    vectorProperties["_ReceiveShadowTexture"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    vectorProperties["_SphereAdd"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    vectorProperties["_EmissionMap"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    vectorProperties["_OutlineWidthTexture"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    vectorProperties["_OutlineColor"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
                    */
                    mat["vectorProperties"] = picojson::value(vectorProperties);

                    {
                        picojson::object textureProperties;
                        //textureProperties["_MainTex"] = picojson::value(0.0);
                        /*    
                            "_MainTex": 0,                 // TODO
                            "_ShadeTexture" : 0,
                            "_SphereAdd" : 1,
                            "_EmissionMap" : 2
                        },*/

                        std::shared_ptr<kml::Texture> tex = imat->GetTexture("BaseColor");
                        if (tex)
                        {
                            auto npbr = nmat["pbrMetallicRoughness"].get<picojson::object>();
                            auto ntex = npbr["baseColorTexture"].get<picojson::object>();

                            int nIndex = (int)(ntex["index"].get<double>());
                            if (nIndex >= 0)
                            {
                                textureProperties["_MainTex"] = picojson::value((double)nIndex);
                            }
                        }
                        mat["textureProperties"] = picojson::value(textureProperties);
                    }

                    picojson::object keywordMap;
                    keywordMap["_ALPHATEST_ON"] = picojson::value(true);
                    keywordMap["_NORMALMAP"] = picojson::value(true);
                    mat["keywordMap"] = picojson::value(keywordMap);

                    picojson::object tagMap;
                    tagMap["RenderType"] = picojson::value("TransparentCutout");
                    mat["tagMap"] = picojson::value(tagMap);

                    materialProperties.push_back(picojson::value(mat));
                }
                VRM["materialProperties"] = picojson::value(materialProperties);
            }

            extensions["VRM"] = picojson::value(VRM);
            root_object["extensions"] = picojson::value(extensions);
            return true;
        }
    } // namespace gltf
    //-----------------------------------------------------------------------------

    static bool ExportGLTF(const std::string& path, const std::shared_ptr<Node>& node, const std::shared_ptr<Options>& opts, bool prettify = true)
    {
        bool output_bin = true;
        bool output_draco = false;

        //std::shared_ptr<Options> opts = Options::GetGlobalOptions();
        bool vrm_export = opts->GetInt("vrm_export") > 0;
        int output_buffer = opts->GetInt("output_buffer");

        if (output_buffer == 0)
        {
            output_bin = true;
            output_draco = false;
        }
        else if (output_buffer == 1)
        {
            output_bin = false;
            output_draco = true;
        }
        else
        {
            output_bin = true;
            output_draco = true;
        }

        bool make_preload_texture = opts->GetInt("make_preload_texture") > 0;

        std::string base_dir = GetBaseDir(path);
        std::string base_name = GetBaseName(path);
        gltf::ObjectRegisterer reg(base_name);
        picojson::object root_object;

        {
            picojson::object asset;
            std::string generator_name = opts->GetString("generator_name");
            std::string generator_version = opts->GetString("generator_version");
            std::string generator = generator_name;
            if (!generator_version.empty())
            {
                generator += " " + generator_version;
            }
            asset["generator"] = picojson::value(generator);
            asset["version"] = picojson::value("2.0");
            root_object["asset"] = picojson::value(asset);
        }

        {
            picojson::array extensionsUsed;
            picojson::array extensionsRequired;
            if (output_draco)
            {
                extensionsUsed.push_back(picojson::value("KHR_draco_mesh_compression"));
                extensionsRequired.push_back(picojson::value("KHR_draco_mesh_compression"));
            }
            if (make_preload_texture)
            {
                extensionsUsed.push_back(picojson::value("KSK_preloadUri"));
                extensionsRequired.push_back(picojson::value("KSK_preloadUri"));
            }
            if (vrm_export)
            {
                extensionsUsed.push_back(picojson::value("VRM"));
            }

            // LTE extention
            extensionsUsed.push_back(picojson::value("LTE_PBR_material"));
            extensionsUsed.push_back(picojson::value("LTE_aiStandardHair_material"));
            extensionsUsed.push_back(picojson::value("LTE_UDIM_texture"));

            if (!extensionsUsed.empty())
            {
                root_object["extensionsUsed"] = picojson::value(extensionsUsed);
            }
            if (!extensionsRequired.empty())
            {
                root_object["extensionsRequired"] = picojson::value(extensionsRequired);
            }
        }

        if (!gltf::NodeToGLTF(root_object, reg, node, output_bin, output_draco))
        {
            return false;
        }

        if (vrm_export)
        {
            if (!gltf::WriteVRMMetaInfo(root_object, node, opts))
            {
                return false;
            }
        }

        {
            std::ofstream ofs(path.c_str());
            if (!ofs)
            {
                std::cerr << "Couldn't write glTF outputfile : " << path << std::endl;
                return false;
            }

            picojson::value(root_object).serialize(std::ostream_iterator<char>(ofs), prettify);
        }

        {
            const std::vector<std::shared_ptr<kml::gltf::Buffer> >& buffers = reg.GetBuffers();
            if (buffers.empty())
            {
                return false;
            }
            std::string binfile = base_dir + buffers[0]->GetURI();
            std::ofstream ofs(binfile.c_str(), std::ofstream::binary);
            if (!ofs)
            {
                std::cerr << "Couldn't write bin outputfile :" << binfile << std::endl;
                return false;
            }
            for (size_t j = 0; j < buffers.size(); j++)
            {
                const std::shared_ptr<kml::gltf::Buffer>& buffer = buffers[j];
                ofs.write((const char*)buffer->GetBytesPtr(), buffer->GetByteLength());
            }
        }

        return true;
    }

    bool glTFExporter::Export(const std::string& path, const std::shared_ptr<Node>& node, const std::shared_ptr<Options>& opts) const
    {
        return ExportGLTF(path, node, opts);
    }

} // namespace kml
