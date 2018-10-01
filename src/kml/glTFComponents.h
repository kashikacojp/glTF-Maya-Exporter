#pragma once
#ifndef _KML_GLTF_COMPONENTS_H_
#define _KML_GLTF_COMPONENTS_H_


#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <fstream>

#include <glm/glm.hpp>
#include <picojson/picojson.h>

#include "glTFConstants.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace kml
{
    namespace gltf
    {
        class Node;

        class Buffer
        {
        public:
            Buffer(const std::string& name, int index)
                :name_(name), index_(index)
            {}
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            std::string GetURI()const
            {
                return name_ + ".bin";
            }
            void AddBytes(const unsigned char bytes[], size_t sz)
            {
                size_t offset = bytes_.size();
                bytes_.resize(offset + sz);
                memcpy(&bytes_[offset], bytes, sz);
            }
            size_t GetSize()const
            {
                return bytes_.size();
            }
            size_t GetByteLength()const
            {
                return GetSize();
            }
            const unsigned char* GetBytesPtr()const
            {
                return &bytes_[0];
            }
        protected:
            std::string name_;
            int index_;
            std::vector<unsigned char> bytes_;
        };

        class BufferView
        {
        public:
            BufferView(const std::string& name, int index)
                :name_(name), index_(index)
            {}
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            void SetBuffer(const std::shared_ptr<Buffer>& bv)
            {
                buffer_ = bv;
            }
            const std::shared_ptr<Buffer>& GetBuffer()const
            {
                return buffer_;
            }
            void SetByteOffset(size_t sz)
            {
                byteOffset_ = sz;
            }
            void SetByteLength(size_t sz)
            {
                byteLength_ = sz;
            }
            size_t GetByteOffset()const
            {
                return byteOffset_;
            }
            size_t GetByteLength()const
            {
                return byteLength_;
            }
            void SetTarget(int t)
            {
                target_ = t;
            }
            int GetTarget()const
            {
                return target_;
            }
        protected:
            std::string name_;
            int index_;
            std::shared_ptr<Buffer> buffer_;
            size_t byteOffset_;
            size_t byteLength_;
            int target_;
        };

        class DracoTemporaryBuffer
        {
        public:
            DracoTemporaryBuffer()
            {}
            DracoTemporaryBuffer(const unsigned char bytes[], size_t sz)
            {
                bytes_.resize(sz);
                memcpy(&bytes_[0], bytes, sz);
            }
            void AddBytes(const unsigned char bytes[], size_t sz)
            {
                bytes_.resize(sz);
                memcpy(&bytes_[0], bytes, sz);
            }
            size_t GetSize()const
            {
                return bytes_.size();
            }
            size_t GetByteLength()const
            {
                return GetSize();
            }
            void ClearBytes()
            {
                bytes_.clear();
            }
            const unsigned char* GetBytesPtr()const
            {
                return &bytes_[0];
            }
        protected:
            std::vector<unsigned char> bytes_;
        };

        class Accessor
        {
        public:
            Accessor(const std::string& name, int index)
                :name_(name), index_(index)
            {}
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            void SetBufferView(const std::shared_ptr<BufferView>& bv)
            {
                bufferView_ = bv;
            }
            std::shared_ptr<BufferView> GetBufferView()const
            {
                if (!dracoBuffer_.get())
                {
                    return bufferView_;
                }
                else
                {
                    return std::shared_ptr<BufferView>();
                }
            }

            void SetDracoTemporaryBuffer(const std::shared_ptr<DracoTemporaryBuffer>& bv)
            {
                dracoBuffer_ = bv;
            }

            std::shared_ptr<DracoTemporaryBuffer> GetDracoTemporaryBuffer()const
            {
                return dracoBuffer_;
            }

            void Set(const std::string& key, const picojson::value& v)
            {
                obj_[key] = v;
            }
            picojson::value Get(const std::string& key)const
            {
                picojson::object::const_iterator it = obj_.find(key);
                if (it != obj_.end())
                {
                    return it->second;
                }
                else
                {
                    return picojson::value();
                }
            }
        protected:
            std::string name_;
            int index_;
            picojson::object obj_;
            std::shared_ptr<BufferView> bufferView_;
            std::shared_ptr<DracoTemporaryBuffer> dracoBuffer_;
        };

        class MorphTarget
        {
        public:
            MorphTarget(const std::string& name, int index)
                :name_(name), index_(index)
            {
                weight_ = 0;
            }
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            void SetAccessor(const std::string& name, const std::shared_ptr<Accessor>& acc)
            {
                accessors_[name] = acc;
            }
            std::shared_ptr<Accessor> GetAccessor(const std::string& name)const
            {
                typedef std::map<std::string, std::shared_ptr<Accessor> > MapType;
                typedef MapType::const_iterator iterator;
                iterator it = accessors_.find(name);
                if (it != accessors_.end())
                {
                    return it->second;
                }
                else
                {
                    return std::shared_ptr<Accessor>();
                }
            }
            void SetWeight(float w)
            {
                weight_ = w;
            }
            float GetWeight()const
            {
                return weight_;
            }
        protected:
            std::string name_;
            int index_;
            float weight_;
            std::map<std::string, std::shared_ptr<Accessor> > accessors_;
        };

        class Mesh
        {
        public:
            Mesh(const std::string& name, int index)
                :name_(name), index_(index)
            {
                mode_ = GLTF_MODE_TRIANGLES;
            }
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            int GetMode()const
            {
                return mode_;
            }
            void SetMaterialID(int id)
            {
                materialID_ = id;
            }
            int GetMaterialID()
            {
                return materialID_;
            }
            std::shared_ptr<Accessor> GetIndices()const
            {
                return GetAccessor("indices");
            }
            void SetAccessor(const std::string& name, const std::shared_ptr<Accessor>& acc)
            {
                accessors_[name] = acc;
            }
            std::shared_ptr<Accessor> GetAccessor(const std::string& name)const
            {
                typedef std::map<std::string, std::shared_ptr<Accessor> > MapType;
                typedef MapType::const_iterator iterator;
                iterator it = accessors_.find(name);
                if (it != accessors_.end())
                {
                    return it->second;
                }
                else
                {
                    return std::shared_ptr<Accessor>();
                }
            }
            void SetBufferView(const std::string& name, const std::shared_ptr<BufferView>& bv)
            {
                bufferViews_[name] = bv;
            }
            std::shared_ptr<BufferView> GetBufferView(const std::string& name)const
            {
                typedef std::map<std::string, std::shared_ptr<BufferView> > MapType;
                typedef MapType::const_iterator iterator;
                iterator it = bufferViews_.find(name);
                if (it != bufferViews_.end())
                {
                    return it->second;
                }
                else
                {
                    return std::shared_ptr<BufferView>();
                }
            }
            void AddTarget(const std::shared_ptr<MorphTarget>& target)
            {
                morph_targets.push_back(target);
            }
            const std::vector<std::shared_ptr<MorphTarget> > GetTargets()const
            {
                return morph_targets;
            }
        protected:
            std::string name_;
            int index_;
            int mode_;
            int materialID_;
            std::map<std::string, std::shared_ptr<Accessor> > accessors_;
            std::map<std::string, std::shared_ptr<BufferView> > bufferViews_;
            std::vector<std::shared_ptr<MorphTarget> > morph_targets;
        };

        class Skin
        {
        public:
            typedef std::map<std::string, float> PathWeight;
            typedef std::vector<int, float> IndexWeight;
        public:
            Skin(const std::string& name, int index)
                :name_(name), index_(index)
            {
                ;
            }
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }

            const std::vector< std::shared_ptr<Node> > GetJoints()const
            {
                return joints_;
            }
            const std::shared_ptr<Node> GetRootJoint()const
            {
                return joints_[0];
            }
            void AddJoint(const std::shared_ptr<Node>& node)
            {
                joints_.push_back(node);
            }
            void SetAccessor(const std::string& name, const std::shared_ptr<Accessor>& acc)
            {
                accessors_[name] = acc;
            }
            std::shared_ptr<Accessor> GetAccessor(const std::string& name)const
            {
                typedef std::map<std::string, std::shared_ptr<Accessor> > MapType;
                typedef MapType::const_iterator iterator;
                iterator it = accessors_.find(name);
                if (it != accessors_.end())
                {
                    return it->second;
                }
                else
                {
                    return std::shared_ptr<Accessor>();
                }
            }
        protected:
            std::string name_;
            int index_;
            std::shared_ptr<Mesh> mesh_;
            std::vector< std::shared_ptr<Node> > joints_;
            std::map<std::string, std::shared_ptr<Accessor> > accessors_;
        };

        class Transform
        {
        public:
            Transform()
                :isTRS_(false)
            {
                mat_ = glm::mat4(1.0f);
            }
            const glm::mat4 GetMatrix()const
            {
                return mat_;
            }
            void SetMatrix(const glm::mat4& mat)
            {
                mat_ = mat;
                isTRS_ = false;
            }
            void SetTRS(const glm::vec3& T, const glm::quat& R, const glm::vec3& S)
            {
                T_ = T;
                R_ = R;
                S_ = S;
                glm::mat4 TT = glm::translate(glm::mat4(1.0f), T);
                glm::mat4 RR = glm::toMat4(R);
                glm::mat4 SS = glm::scale(glm::mat4(1.0f), S);

                mat_ = TT * RR * SS;

                isTRS_ = true;
            }
            bool IsTRS()const
            {
                return isTRS_;
            }
            glm::vec3 GetT()const { return T_; }
            glm::quat GetR()const { return R_; }
            glm::vec3 GetS()const { return S_; }
        protected:
            bool isTRS_;
            glm::mat4 mat_;
            glm::vec3 T_;
            glm::quat R_;
            glm::vec3 S_;
        };

        class AnimationSampler
        {
        public:
            AnimationSampler(const std::string& name, int index)
                :name_(name), index_(index)
            {
                ;
            }
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            void SetTargetPath(const std::string& path)
            {
                targetPath_ = path;
            }
            const std::string& GetTargetPath()
            {
                return targetPath_;
            }
            void SetInputAccessor(const std::shared_ptr< Accessor >& in)
            {
                in_ = in;
            }
            const std::shared_ptr< Accessor >& GetInputAccessor()const
            {
                return in_;
            }
            void SetOutputAccessor(const std::shared_ptr< Accessor >& out)
            {
                out_ = out;
            }
            const std::shared_ptr< Accessor >& GetOutputAccessor()const
            {
                return out_;
            }
            void SetInterpolation(const std::string& inter)
            {
                interpolation_ = inter;
            }
            std::string GetInterpolation()const
            {
                return interpolation_;
            }
        protected:
            std::string name_;
            int index_;
            std::string targetPath_;
            std::string interpolation_;
            std::shared_ptr< Accessor > in_;    //key
            std::shared_ptr< Accessor > out_;   //value
        };

        class AnimationChannel
        {
        public:
            AnimationChannel(const std::string& name, int index)
                :name_(name), index_(index)
            {
                ;
            }
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            const std::string& GetTargetPath()
            {
                return sampler_->GetTargetPath();
            }
            void SetTargetNode(const std::shared_ptr<Node>& node)
            {
                targetNode_ = node;
            }
            const std::shared_ptr<Node>& GetTargetNode()const
            {
                return targetNode_;
            }
            void SetSampler(const std::shared_ptr<AnimationSampler>& s)
            {
                sampler_ = s;
            }
            const std::shared_ptr<AnimationSampler>& GetSampler()const
            {
                return sampler_;
            }
        protected:
            std::string name_;
            int index_;
            std::string targetPath_;
            std::shared_ptr<Node> targetNode_;
            std::shared_ptr<AnimationSampler> sampler_;
        };

        class Animation
        {
        public:
            Animation(const std::string& name, int index)
                :name_(name), index_(index)
            {
                ;
            }
            const std::string& GetName()const
            {
                return name_;
            }
            int GetIndex()const
            {
                return index_;
            }
            void AddChannel(const std::shared_ptr<AnimationChannel>& chan)
            {
                channels_.push_back(chan);
            }
            void AddSampler(const std::shared_ptr<AnimationSampler>& s)
            {
                samplers_.push_back(s);
            }
            const std::vector<std::shared_ptr<AnimationChannel> >& GetChannels()const
            {
                return channels_;
            }
            const std::vector<std::shared_ptr<AnimationSampler> > GetSamplers()const
            {
                return samplers_;
            }
        private:
            std::string name_;
            int index_;
            std::vector<std::shared_ptr<AnimationChannel> > channels_;
            std::vector<std::shared_ptr<AnimationSampler> > samplers_;
        };

        class Node
        {
        public:
            Node(const std::string& name, int index)
                :name_(name), index_(index)
            {
                trans_.reset(new Transform());
            }
            const std::string& GetName()const
            {
                return name_;
            }
            void SetPath(const std::string& path)
            {
                path_ = path;
            }
            const std::string& GetPath()const
            {
                return path_;
            }
            int GetIndex()const
            {
                return index_;
            }
            void SetMesh(const std::shared_ptr<Mesh>& mesh)
            {
                mesh_ = mesh;
            }
            const std::shared_ptr<Mesh>& GetMesh()const
            {
                return mesh_;
            }
            void SetSkin(const std::shared_ptr<Skin>& skin)
            {
                skin_ = skin;
            }
            const std::shared_ptr<Skin>& GetSkin()const
            {
                return skin_;
            }
            void AddChild(const std::shared_ptr<Node>& node)
            {
                children_.push_back(node);
            }
            std::vector< std::shared_ptr<Node> >& GetChildren()
            {
                return children_;
            }
            const std::vector< std::shared_ptr<Node> >& GetChildren()const
            {
                return children_;
            }

            std::shared_ptr<Transform>& GetTransform()
            {
                return trans_;
            }
            const std::shared_ptr<Transform>& GetTransform()const
            {
                return trans_;
            }

            glm::mat4 GetMatrix()const
            {
                return trans_->GetMatrix();
            }
        protected:
            std::string name_;
            int index_;
            std::string path_;
            std::shared_ptr<Transform> trans_;
            std::shared_ptr<Mesh> mesh_;
            std::shared_ptr<Skin> skin_;
            std::vector< std::shared_ptr<Node> > children_;
        };
    }
}

#endif

