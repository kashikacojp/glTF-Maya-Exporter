#define GLM_ENABLE_EXPERIMENTAL 1

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#define NOMINMAX
#include <windows.h>
#endif

#include "glTFExporter.h"

#include "TriangulateMesh.h"
#include "Options.h"
#include "SaveToDraco.h"

#include <climits>
#include <vector>
#include <set>
#include <fstream>

#include "gltfConstants.h"

#include <picojson/picojson.h>


#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string.h>
#include <stdlib.h>


namespace {
	enum ImageFormat {
		FORMAT_JPEG = 0,
		FORMAT_PNG,
		FORMAT_BMP,
		FORMAT_GIF
	};
}

namespace kml
{
	static
	std::string IToS(int n)
	{
		char buffer[16] = {};
#ifdef _WIN32
		_snprintf(buffer, 16, "%03d", n);
#else
		snprintf(buffer, 16, "%03d", n);
#endif
		return buffer;
	}

	static
	std::string GetBaseDir(const std::string& filepath)
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

	static
	std::string RemoveExt(const std::string& filepath)
	{
		if (filepath.find_last_of(".") != std::string::npos)
			return filepath.substr(0, filepath.find_last_of("."));
		return filepath;
	}

	static
    std::string GetBaseName(const std::string& filepath)
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

	static
	std::string GetImageID(const std::string& imagePath)
	{
		return GetBaseName(imagePath);
	}

	static
	std::string GetTextureID(const std::string& imagePath)
	{
		return "texture_" + GetImageID(imagePath);
	}

	static
	std::string GetFileExtName(const std::string& path)
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

	static
    void GetTextures(std::map<std::string, std::shared_ptr<kml::Texture> >& texture_set, const std::vector< std::shared_ptr<::kml::Material> >& materials)
	{
		for (size_t j = 0; j < materials.size(); j++)
		{
			const auto& mat = materials[j];
			auto keys = mat->GetTextureKeys();
			for (int i = 0; i < keys.size(); i++)
			{
				std::shared_ptr<kml::Texture> tex = mat->GetTexture(keys[i]);
				texture_set[tex->GetFilePath()] = tex;
			}
		}
	}

	static
    std::string GetExt(const std::string& filepath)
	{
		if (filepath.find_last_of(".") != std::string::npos)
			return filepath.substr(filepath.find_last_of("."));
		return "";
	}

	static
        unsigned int GetImageFormat(const std::string& path)
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

    static
    bool IsZero(const float* p, int sz, float eps = 1e-15f)
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

    static
    bool IsZero(const glm::vec3& p, float eps = 1e-15)
    {
        return IsZero(glm::value_ptr(p), 3, eps);
    }

    static
    bool IsZero(const glm::quat& p, float eps = 1e-15)
    {
        return IsZero(glm::value_ptr(p), 4, eps);
    }

    static
    bool IsZero(const glm::mat4& p, float eps = 1e-15)
    {
        return IsZero(glm::value_ptr(p), 16, eps);
    }

	namespace gltf
	{
        class Node;

		class Buffer
		{
		public:
			Buffer(const std::string& name, int index, bool is_draco = false, bool is_union_buffer_draco = false)
				:name_(name), index_(index), is_draco_(is_draco), is_union_buffer_draco_(is_union_buffer_draco)
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
				if (!is_draco_)
				{
					return name_ + ".bin";
				}
				else
				{
					return this->GetCompressDracoURI();
				}
			}
			std::string GetCompressDracoURI()const
			{
				if (is_union_buffer_draco_)
				{
					return name_ + ".bin";
				}
				else
				{
					return name_ + std::string("_") + IToS(index_) + ".bin";
				}
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
			bool IsDraco()const
			{
				return is_draco_;
			}
		protected:
			std::string name_;
			int index_;
			bool is_draco_;
			bool is_union_buffer_draco_;
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
			const std::shared_ptr<BufferView>& GetBufferView()const
			{
				return bufferView_;
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
			std::shared_ptr<BufferView> bufferView_;
			picojson::object obj_;
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
			std::shared_ptr<Accessor> GetIndices()
			{
				return accessors_["indices"];
			}
			void SetAccessor(const std::string& name, const std::shared_ptr<Accessor>& acc)
			{
				accessors_[name] = acc;
			}
			std::shared_ptr<Accessor> GetAccessor(const std::string& name)
			{
				return accessors_[name];
			}
		protected:
			std::string name_;
			int index_;
			int mode_;
			int materialID_;
			std::map<std::string, std::shared_ptr<Accessor> > accessors_;
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
            std::shared_ptr<Accessor> GetAccessor(const std::string& name)
            {
                return accessors_[name];
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

		static
		void GetMinMax(float min[], float max[], const std::vector<float>& verts, int n)
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

		static
		void GetMinMax(unsigned int& min, unsigned int& max, const std::vector<unsigned int>& verts)
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

        static
        void GetMinMax(unsigned short min[], unsigned short max[], const std::vector<unsigned short>& verts, int n)
        {
            for (int i = 0; i < n; i++)
            {
                min[i] = std::numeric_limits<float>::max();
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

        template<class T>
		static
		picojson::array ConvertToArray(T v[], int n)
		{
			picojson::array a;
			for (int j = 0; j < n; j++)
			{
				a.push_back(picojson::value((double)v[j]));
			}
			return a;
		}

        static
        int GetIndexOfJoint(const std::shared_ptr<Skin>& skin, const std::string& path)
        {
            const auto& joints = skin->GetJoints();
            for (int i = 0; i < (int)joints.size(); i++)
            {
                if (joints[i]->GetPath() == path)
                {
                    return i;
                }
            }
            return -1;
        }

		class ObjectRegisterer
		{
		public:
            ObjectRegisterer(const std::string& basename)
			{
				basename_ = basename;
			}

            std::shared_ptr<Node> CreateNode(const std::shared_ptr<::kml::Node>& in_node)
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

			void RegisterComponents(std::shared_ptr<Node>& node, const std::shared_ptr<::kml::Node>& in_node)
			{
				const std::shared_ptr<::kml::Mesh>& in_mesh = in_node->GetMesh();

				if (in_mesh.get() != NULL)
				{
					int nMesh = meshes_.size();
					//std::string meshName = "mesh_" + IToS(nMesh);
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
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(indices, GLTF_TARGET_ELEMENT_ARRAY_BUFFER);
						acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(indices.size())));
						acc->Set("type", picojson::value("SCALAR"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_UNSIGNED_INT));//5125
						acc->Set("byteOffset", picojson::value((double)0));
						//acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));

						unsigned int min, max;
						GetMinMax(min, max, indices);
						picojson::array amin, amax;
						amin.push_back(picojson::value((double)min));
						amax.push_back(picojson::value((double)max));
						acc->Set("min", picojson::value(amin));
						acc->Set("max", picojson::value(amax));

						accessors_.push_back(acc);
						mesh->SetAccessor("indices", acc);
						nAcc++;
					}
					{
						//normal
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(normals);
						acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(normals.size() / 3)));
						acc->Set("type", picojson::value("VEC3"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
						acc->Set("byteOffset", picojson::value((double)0));
						//acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

						float min[3] = {}, max[3] = {};
						if (normals.size())
						{
							GetMinMax(min, max, normals, 3);
						}
						acc->Set("min", picojson::value(ConvertToArray(min, 3)));
						acc->Set("max", picojson::value(ConvertToArray(max, 3)));

						accessors_.push_back(acc);
						mesh->SetAccessor("NORMAL", acc);
						nAcc++;
					}
					{
						//position
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(positions);
						acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(positions.size() / 3)));
						acc->Set("type", picojson::value("VEC3"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
						acc->Set("byteOffset", picojson::value((double)0));
						//acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

						float min[3] = {}, max[3] = {};
						GetMinMax(min, max, positions, 3);
						acc->Set("min", picojson::value(ConvertToArray(min, 3)));
						acc->Set("max", picojson::value(ConvertToArray(max, 3)));

						accessors_.push_back(acc);
						mesh->SetAccessor("POSITION", acc);
						nAcc++;
					}
					if (texcoords.size() > 0)
					{
						//texcoord
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(texcoords);
						acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(texcoords.size() / 2)));
						acc->Set("type", picojson::value("VEC2"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
						acc->Set("byteOffset", picojson::value((double)0));
						//acc->Set("byteStride", picojson::value((double)2 * sizeof(float)));

						float min[3] = {}, max[3] = {};
						GetMinMax(min, max, texcoords, 2);
						acc->Set("min", picojson::value(ConvertToArray(min, 2)));
						acc->Set("max", picojson::value(ConvertToArray(max, 2)));

						accessors_.push_back(acc);
						mesh->SetAccessor("TEXCOORD_0", acc);
						nAcc++;
					}
					
                    const std::shared_ptr<::kml::SkinWeights> in_skin = in_mesh->skin_weights;
                    if (in_skin.get() && this->skins_.size() > 0)
                    {
                        std::shared_ptr<Skin> skin = skins_[0];

                        {
                            typedef std::map<std::string, std::shared_ptr<Node> > JointMap;
                            typedef JointMap::iterator JointIterator;

                            struct WeightSorter
                            {
                                bool operator()(const std::pair<int, float>& a, const std::pair<int, float>& b)const
                                {
                                    return a.second > b.second;
                                }
                            };

                            typedef ::kml::SkinWeights::WeightVertex WeightVertex;
                            typedef WeightVertex::const_iterator WeightIterator;
                            std::vector<unsigned short> joints;
                            std::vector<float> weights;
                            for (int i = 0; i < in_skin->weights.size(); i++)
                            {
                                std::vector< std::pair<int, float> > ww;
                                WeightIterator it = in_skin->weights[i].begin();
                                for (; it != in_skin->weights[i].end(); it++)
                                {
                                    std::string path = it->first;
                                    float weight = it->second;
                                    int index = std::max<int>(0, GetIndexOfJoint(skin, path));
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

                            if(joints.size() > 0)
                            {
                                //indices
                                std::string accName = "accessor_" + IToS(nAcc);//
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(joints, GLTF_TARGET_ARRAY_BUFFER);
                                acc->SetBufferView(bufferView);
                                acc->Set("count", picojson::value((double)(joints.size() / 4)));
                                acc->Set("type", picojson::value("VEC4"));
                                acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_UNSIGNED_SHORT));//5123
                                acc->Set("byteOffset", picojson::value((double)0));
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
                                //indices
                                std::string accName = "accessor_" + IToS(nAcc);//
                                std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                                const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(weights, GLTF_TARGET_ARRAY_BUFFER);
                                acc->SetBufferView(bufferView);
                                acc->Set("count", picojson::value((double)(weights.size() / 4)));
                                acc->Set("type", picojson::value("VEC4"));
                                acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
                                acc->Set("byteOffset", picojson::value((double)0));
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
                            
                        }

                        node->SetSkin(skin);
                    }

                    node->SetMesh(mesh);
                    this->meshes_.push_back(mesh);
				}
			}

			void RegisterComponentsDraco(std::shared_ptr<Node>& node, const std::shared_ptr<::kml::Node>& in_node, bool is_union_buffer = false)
			{
				const std::shared_ptr<::kml::Mesh>& in_mesh = in_node->GetMesh();

				if (in_mesh.get() != NULL)
				{
					int nMesh = meshes_.size();
					std::string meshName = "mesh_" + IToS(nMesh);
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

					std::shared_ptr<BufferView> bufferView;
					{
						size_t offset = 0;
						if (is_union_buffer)
						{
							if (!dracoBuffers_.empty())
							{
								offset = dracoBuffers_[0]->GetSize();
							}
						}
						std::shared_ptr<Buffer> buffer = this->AddBufferDraco(in_node->GetMesh(), is_union_buffer);
						if (buffer.get())
						{
							bufferView = this->AddBufferViewDraco(buffer, offset);
						}
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
						float l = (x*x + y * y + z * z);
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
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						//acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(indices.size())));
						acc->Set("type", picojson::value("SCALAR"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_UNSIGNED_INT));//5125
																											 //acc->Set("byteOffset", picojson::value((double)0));
																											 //acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));

						unsigned int min, max;
						GetMinMax(min, max, indices);
						picojson::array amin, amax;
						amin.push_back(picojson::value((double)min));
						amax.push_back(picojson::value((double)max));
						acc->Set("min", picojson::value(amin));
						acc->Set("max", picojson::value(amax));

						accessors_.push_back(acc);
						mesh->SetAccessor("indices", acc);
						nAcc++;
					}
					{
						//normal
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						//acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(normals.size() / 3)));
						acc->Set("type", picojson::value("VEC3"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
																									  //acc->Set("byteOffset", picojson::value((double)0));
																									  //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

						float min[3] = {}, max[3] = {};
						if (normals.size())
						{
							GetMinMax(min, max, normals, 3);
						}
						acc->Set("min", picojson::value(ConvertToArray(min, 3)));
						acc->Set("max", picojson::value(ConvertToArray(max, 3)));

						accessors_.push_back(acc);
						mesh->SetAccessor("NORMAL", acc);
						nAcc++;
					}
					{
						//position
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						//acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(positions.size() / 3)));
						acc->Set("type", picojson::value("VEC3"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
																									  //acc->Set("byteOffset", picojson::value((double)0));
																									  //acc->Set("byteStride", picojson::value((double)3 * sizeof(float)));

						float min[3] = {}, max[3] = {};
						GetMinMax(min, max, positions, 3);
						acc->Set("min", picojson::value(ConvertToArray(min, 3)));
						acc->Set("max", picojson::value(ConvertToArray(max, 3)));

						accessors_.push_back(acc);
						mesh->SetAccessor("POSITION", acc);
						nAcc++;
					}
					if (texcoords.size() > 0)
					{
						//texcoord
						std::string accName = "accessor_" + IToS(nAcc);//
						std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
						//acc->SetBufferView(bufferView);
						acc->Set("count", picojson::value((double)(texcoords.size() / 2)));
						acc->Set("type", picojson::value("VEC2"));
						acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
																									  //acc->Set("byteOffset", picojson::value((double)0));
																									  //acc->Set("byteStride", picojson::value((double)2 * sizeof(float)));

						float min[3] = {}, max[3] = {};
						GetMinMax(min, max, texcoords, 2);
						acc->Set("min", picojson::value(ConvertToArray(min, 2)));
						acc->Set("max", picojson::value(ConvertToArray(max, 2)));

						accessors_.push_back(acc);
						mesh->SetAccessor("TEXCOORD_0", acc);
						nAcc++;
					}

                    const std::shared_ptr<::kml::SkinWeights> in_skin = in_mesh->skin_weights;
                    if (in_skin.get())
                    {
                        //int nSkin = skins_.size();
                        //std::string skinName = in_skin->name;
                        //std::shared_ptr<Skin> skin(new Skin(skinName, nSkin));

                        //node->SetSkin(skin);
                        //this->skins_.push_back(skin);
                    }

					node->SetMesh(mesh);
					this->meshes_.push_back(mesh);
				}
			}
		public:
			const std::vector<std::shared_ptr<Node> >& GetNodes()const
			{
				return nodes_;
			}
			const std::vector<std::shared_ptr<Mesh> >& GetMeshes()const
			{
				return meshes_;
			}
			const std::vector<std::shared_ptr<Accessor> >& GetAccessors()const
			{
				return accessors_;
			}
			const std::vector<std::shared_ptr<BufferView> >& GetBufferViews()const
			{
				return bufferViews_;
			}
			const std::vector<std::shared_ptr<Buffer> >& GetBuffers()const
			{
				return buffers_;
			}
        public:
            std::vector<std::shared_ptr<Skin> >& GetSkins()
            {
                return skins_;
            }
            const std::vector<std::shared_ptr<Skin> >& GetSkins()const
            {
                return skins_;
            }
		public:
			const std::vector<std::shared_ptr<Buffer> >& GetBuffersDraco()const
			{
				return dracoBuffers_;
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
			std::shared_ptr<Buffer> GetLastBinBuffer()
			{
				if (buffers_.empty())
				{
					buffers_.push_back(std::shared_ptr<Buffer>(new Buffer(basename_, 0)));
				}
				return buffers_.back();
			}
			void AddNode(const std::shared_ptr<Node>& node)
			{
				nodes_.push_back(node);
			}

			const std::shared_ptr<BufferView>& AddBufferView(const std::vector<float>& vec, int target = GLTF_TARGET_ARRAY_BUFFER)
			{
				std::shared_ptr<Buffer> buffer = this->GetLastBinBuffer();
				int nBV = bufferViews_.size();
				std::string name = "bufferView_" + IToS(nBV);//
				std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
				size_t offset = buffer->GetSize();
				size_t length = sizeof(float)*vec.size();
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
				std::shared_ptr<Buffer> buffer = this->GetLastBinBuffer();
				int nBV = bufferViews_.size();
				std::string name = "bufferView_" + IToS(nBV);//
				std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
				size_t offset = buffer->GetSize();
				size_t length = sizeof(unsigned int)*vec.size();
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
                std::shared_ptr<Buffer> buffer = this->GetLastBinBuffer();
                int nBV = bufferViews_.size();
                std::string name = "bufferView_" + IToS(nBV);//
                std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
                size_t offset = buffer->GetSize();
                size_t length = sizeof(unsigned short)*vec.size();
                buffer->AddBytes((unsigned char*)(&vec[0]), length);
                bufferView->SetByteOffset(offset);
                bufferView->SetByteLength(length);
                bufferView->SetBuffer(buffer);
                bufferView->SetTarget(target);
                bufferViews_.push_back(bufferView);
                return bufferViews_.back();
            }

			const std::shared_ptr<BufferView>& AddBufferViewDraco(const std::shared_ptr<Buffer>& buffer, size_t offset = 0)
			{
				int nBV = bufferViews_.size();
				std::string name = "bufferView_" + IToS(nBV);//
				std::shared_ptr<BufferView> bufferView(new BufferView(name, nBV));
				size_t length = (size_t)((int)buffer->GetSize() - (int)offset);
				bufferView->SetByteOffset(offset);
				bufferView->SetByteLength(length);
				bufferView->SetBuffer(buffer);
				bufferView->SetTarget(GLTF_TARGET_ARRAY_BUFFER);
				bufferViews_.push_back(bufferView);
				return bufferViews_.back();
			}

			std::shared_ptr<Buffer> AddBufferDraco(const std::shared_ptr<::kml::Mesh>& mesh, bool is_union_buffer = false)
			{
				std::vector<unsigned char> bytes;

				if (!SaveToDraco(bytes, mesh))
				{
					return std::shared_ptr<Buffer>();
				}

				if (!is_union_buffer)
				{
					dracoBuffers_.push_back(std::shared_ptr<Buffer>(new Buffer(basename_, buffers_.size(), true)));
				}
				else
				{
					if (dracoBuffers_.empty())
					{
						dracoBuffers_.push_back(std::shared_ptr<Buffer>(new Buffer(basename_, buffers_.size(), true, true)));
					}
				}
				std::shared_ptr<Buffer>& buffer = dracoBuffers_.back();
				buffer->AddBytes(&bytes[0], bytes.size());
				return buffer;
			}
		protected:
			std::vector<std::shared_ptr<Node> > nodes_;
			std::vector<std::shared_ptr<Mesh> > meshes_;
			std::vector<std::shared_ptr<Accessor> > accessors_;
			std::vector<std::shared_ptr<BufferView> > bufferViews_;
			std::vector<std::shared_ptr<Buffer> > buffers_;
			std::vector<std::shared_ptr<Buffer> > dracoBuffers_;
            std::vector< std::shared_ptr<Skin> > skins_;
			std::string basename_;
		};

		static
		int FindTextureIndex(const std::vector<std::string>& v, const std::string& s)
		{
			std::vector<std::string>::const_iterator it = std::find(v.begin(), v.end(), s);
			if (it != v.end())
			{
				return std::distance(v.begin(), it);
			}
			return -1;
		}

        static
        std::shared_ptr<Node> CreateNodes(
            std::vector<std::pair<std::shared_ptr<Node>, std::shared_ptr<::kml::Node> > >& node_pairs,
            ObjectRegisterer& reg,
            const std::shared_ptr<::kml::Node>& in_node)
        {
            std::shared_ptr<Node> ret_node = reg.CreateNode(in_node);
            node_pairs.push_back( std::make_pair(ret_node, in_node));
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

        static
        void GetSkinWeights(std::vector< std::shared_ptr<::kml::SkinWeights> >& skin_weights, const std::shared_ptr<::kml::Node>& in_node)
        {
            std::shared_ptr<::kml::Mesh> in_mesh = in_node->GetMesh();
            if (in_mesh.get())
            {
                std::shared_ptr<::kml::SkinWeights> in_skin = in_mesh->skin_weights;
                if (in_skin.get())
                {
                    skin_weights.push_back(in_skin);
                }
            }

            {
                auto& children = in_node->GetChildren();
                for (size_t i = 0; i < children.size(); i++)
                {
                    GetSkinWeights(skin_weights, children[i]);
                }
            }
        }

        void RegisterSkins(
            ObjectRegisterer& reg,
            const std::shared_ptr<::kml::Node>& in_node)
        {
            std::vector< std::shared_ptr<::kml::SkinWeights> > skin_weights;
            GetSkinWeights(skin_weights, in_node);

            std::map<std::string, glm::mat4> path_matrix_map;
            if(!skin_weights.empty())
            {
                int nSkin = reg.GetSkins().size();
                std::string skinName = "skin_" + IToS(nSkin);
                std::shared_ptr<Skin> skin(new Skin(skinName, nSkin));


                typedef std::map<std::string, std::shared_ptr<Node> > JointMap;
                typedef JointMap::iterator JointIterator;

                typedef std::pair<size_t, std::shared_ptr<Node> > JointPair;

                struct JointSorter
                {
                    bool operator()(const JointPair& a, const JointPair& b)const
                    {
                        return a.first < b.first;
                    }
                };

                const std::vector< std::shared_ptr<Node> >& nodes = reg.GetNodes();
                std::map<std::string, std::shared_ptr<Node> > joint_map;
                for (size_t i = 0; i < nodes.size(); i++)
                {
                    std::string path = nodes[i]->GetPath();
                    joint_map[path] = nodes[i];
                }

                std::map<std::string, std::shared_ptr<Node> > joint_map2;
                
                for (size_t j = 0; j < skin_weights.size(); j++)
                {
                    std::shared_ptr<::kml::SkinWeights>& in_skin = skin_weights[j];

                    typedef ::kml::SkinWeights::WeightVertex WeightVertex;
                    typedef WeightVertex::const_iterator WeightIterator;
                    std::vector<unsigned short> joints;
                    std::vector<float> weights;
                    for (int i = 0; i < in_skin->weights.size(); i++)
                    {
                        WeightIterator it = in_skin->weights[i].begin();
                        for (; it != in_skin->weights[i].end(); it++)
                        {
                            std::string path = it->first;
                            float weight = it->second;
                            JointIterator jit = joint_map.find(path);
                            if (jit != joint_map.end())
                            {
                                std::string path = jit->first;
                                joint_map2[path] = jit->second;
                            }
                        }
                    }

                    for (int i = 0; i < in_skin->joint_paths.size(); i++)
                    {
                        path_matrix_map[in_skin->joint_paths[i]] = in_skin->joint_bind_matrices[i];
                    }
                }

                std::vector < std::pair<int, std::shared_ptr<Node> > > joint_nodes;
                {
                    for (JointIterator it = joint_map2.begin(); it != joint_map2.end(); it++)
                    {
                        joint_nodes.push_back( std::make_pair(it->first.size(), it->second) );
                    }
                }

                std::sort(joint_nodes.begin(), joint_nodes.end(), JointSorter());
                for (size_t i = 0; i < joint_nodes.size(); i++)
                {
                    skin->AddJoint(joint_nodes[i].second);
                }

                if (skin->GetJoints().size() > 0)
                {
                    const std::vector<std::shared_ptr<Node> >& skin_joints = skin->GetJoints();
                    
                    std::vector<float> inverseMatrices;
                    for (size_t i = 0; i < skin_joints.size(); i++)
                    {
                        glm::mat4 mat = path_matrix_map[skin_joints[i]->GetPath()];
                        const float* ptr = glm::value_ptr(mat);
                        for (size_t k = 0; k < 16; k++)
                        {
                            inverseMatrices.push_back(ptr[k]);
                        }
                    }

                    int nAcc = reg.GetAccessors().size();
                    //indices
                    std::string accName = "accessor_" + IToS(nAcc);//
                    std::shared_ptr<Accessor> acc(new Accessor(accName, nAcc));
                    const std::shared_ptr<BufferView>& bufferView = reg.AddBufferView(inverseMatrices, -1);
                    acc->SetBufferView(bufferView);
                    acc->Set("count", picojson::value((double)(inverseMatrices.size() / 16)));
                    acc->Set("type", picojson::value("MAT4"));
                    acc->Set("componentType", picojson::value((double)GLTF_COMPONENT_TYPE_FLOAT));//5126
                    acc->Set("byteOffset", picojson::value((double)0));
                    //acc->Set("byteStride", picojson::value((double)sizeof(unsigned int)));
                    /*
                    float min[16] = {}, max[16] = {};
                    GetMinMax(min, max, inverseMatrices, 16);
                    acc->Set("min", picojson::value(ConvertToArray(min, 16)));
                    acc->Set("max", picojson::value(ConvertToArray(max, 16)));
                    */

                    reg.AddAccessor(acc);
                    skin->SetAccessor("inverseBindMatrices", acc);

                    reg.AddSkin(skin);
                }
            }
        }

        static
        void RegisterObjects(
            ObjectRegisterer& reg,
            const std::shared_ptr<::kml::Node>& node,
            bool IsOutputBin,
            bool IsOutputDraco, bool IsUnionBufferDraco)
        {
            std::vector< std::pair<std::shared_ptr<Node>, std::shared_ptr<::kml::Node> > > node_pairs;
            CreateNodes(node_pairs, reg, node);
            RegisterSkins(reg, node);
            for (size_t i = 0; i < node_pairs.size(); i++)
            {
                if (IsOutputBin)
                {
                    reg.RegisterComponents(node_pairs[i].first, node_pairs[i].second);
                }
                else if (IsOutputDraco)
                {
                    reg.RegisterComponentsDraco(node_pairs[i].first, node_pairs[i].second, IsUnionBufferDraco);
                }
            }
        }

        static
        picojson::array GetFloatAsArray(const float* p, int sz)
        {
            picojson::array ar;
            for (int i = 0; i < sz; i++)
            {
                ar.push_back(picojson::value(p[i]));
            }

            return ar;
        }

		static
		picojson::array GetMatrixAsArray(const glm::mat4& mat)
		{
			return GetFloatAsArray(glm::value_ptr(mat), 16);
		}

		static
		bool NodeToGLTF(
			picojson::object& root,
			ObjectRegisterer& reg,
			const std::shared_ptr<::kml::Node>& node,
			bool IsOutputBin,
			bool IsOutputDraco,
			bool IsUnionBufferDraco)
		{
			{
				picojson::object sampler;
				sampler["magFilter"] = picojson::value((double)GLTF_TEXTURE_FILTER_LINEAR);//WebGLConstants.LINEAR
				sampler["minFilter"] = picojson::value((double)GLTF_TEXTURE_FILTER_LINEAR);//WebGLConstants.NEAREST_MIPMAP_LINEAR
				sampler["wrapS"] = picojson::value((double)GLTF_TEXTURE_WRAP_CLAMP_TO_EDGE);
				sampler["wrapT"] = picojson::value((double)GLTF_TEXTURE_WRAP_CLAMP_TO_EDGE);
				picojson::array samplers;
				samplers.push_back(picojson::value(sampler));
				root["samplers"] = picojson::value(samplers);
			}
			typedef std::map<std::string, std::string> CacheMapType;
			std::vector<std::string> texture_vec;
			std::map<std::string, std::string> cache_map;
			std::map<std::string, std::shared_ptr<kml::Texture>> texture_set;
			{
				GetTextures(texture_set, node->GetMaterials());

				static const std::string t = "_s0.";
				for (std::map<std::string, std::shared_ptr<kml::Texture>>::const_iterator it = texture_set.begin(); it != texture_set.end(); ++it)
				{
					std::shared_ptr<kml::Texture> tex = it->second;
					std::string texname = tex->GetFilePath();

					if (texname.find(t) == std::string::npos)
					{
						texture_vec.push_back(texname);
					}
					else
					{
						std::string orgPath = texname;
						orgPath.replace(texname.find(t), t.size(), ".");
						orgPath = RemoveExt(orgPath);
						cache_map.insert(CacheMapType::value_type(orgPath, texname));
					}

				}
			}
			{
				int level = 0;
				picojson::array images;
				picojson::array textures;
				for (size_t i = 0; i < texture_vec.size(); i++)
				{
					std::string imagePath = texture_vec[i];
					std::string imageId = GetImageID(imagePath);
					std::string texuteId = GetTextureID(imagePath);

					picojson::object image;
					image["name"] = picojson::value(imageId);
					image["uri"] = picojson::value(imagePath);
					{
						CacheMapType::const_iterator it = cache_map.find(RemoveExt(imagePath));
						if (it != cache_map.end())
						{
							//"extensions": {"KSK_preloadUri":{"uri":"Default_baseColor_pre.jpg"}}
							picojson::object extensions;
							picojson::object KSK_preloadUri;
							KSK_preloadUri["uri"] = picojson::value(it->second);
							extensions["KSK_preloadUri"] = picojson::value(KSK_preloadUri);
							image["extensions"] = picojson::value(extensions);
						}
					}
					images.push_back(picojson::value(image));

					picojson::object texture;
					int nFormat = GetImageFormat(imagePath);
					texture["format"] = picojson::value((double)nFormat);         //image.format;
					texture["internalFormat"] = picojson::value((double)nFormat); //image.format;
					texture["sampler"] = picojson::value((double)0);
					texture["source"] = picojson::value((double)i);   //imageId;
					texture["target"] = picojson::value((double)GLTF_TEXTURE_TARGET_TEXTURE2D); //WebGLConstants.TEXTURE_2D;
					texture["type"] = picojson::value((double)GLTF_TEXTURE_TYPE_UNSIGNED_BYTE); //WebGLConstants.UNSIGNED_BYTE

					textures.push_back(picojson::value(texture));
				}
				if (!images.empty())
				{
					root["images"] = picojson::value(images);
				}
				if (!textures.empty())
				{
					root["textures"] = picojson::value(textures);
				}
			}

			{
                RegisterObjects(reg, node, IsOutputBin, IsOutputDraco, IsUnionBufferDraco);
			}

			{
				root["scene"] = picojson::value((double)0);
			}

			{
				picojson::array ar;

				picojson::object scene;
				picojson::array nodes_;
				const std::vector< std::shared_ptr<Node> >& nodes = reg.GetNodes();
				if(!nodes.empty())
				{
                    nodes_.push_back(picojson::value((double)0));
				}
				scene["nodes"] = picojson::value(nodes_);
				ar.push_back(picojson::value(scene));

				root["scenes"] = picojson::value(ar);
			}

			{
				const std::vector< std::shared_ptr<Node> >& nodes = reg.GetNodes();
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
                        if(!IsZero(T))
                        {
                            nd["translation"] = picojson::value(GetFloatAsArray(glm::value_ptr(T), 3));
                        }
                        if (!IsZero(R - glm::quat(1, 0, 0, 0)))
                        {
                            nd["rotation"] = picojson::value(GetFloatAsArray(glm::value_ptr(R), 4));//xyzw
                        }
                        if (!IsZero(S - glm::vec3(1, 1, 1)))
                        {
                            nd["scale"] = picojson::value(GetFloatAsArray(glm::value_ptr(S), 3));
                        }
                    }
                    else
                    {
                        glm::mat4 mat = n->GetMatrix();
                        if (!IsZero(mat-glm::mat4(1)))
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

			{
				const std::vector< std::shared_ptr<Mesh> >& meshes = reg.GetMeshes();
				//std::cout << meshes.size() << std::endl;
				picojson::array ar;
				for (size_t i = 0; i < meshes.size(); i++)
				{
					const std::shared_ptr<Mesh>& mesh = meshes[i];
					picojson::object nd;
					nd["name"] = picojson::value(mesh->GetName());

					picojson::object attributes;
					{
						attributes["NORMAL"] = picojson::value((double)mesh->GetAccessor("NORMAL")->GetIndex());//picojson::value(mesh->GetAccessor("NORMAL")->GetName());
						attributes["POSITION"] = picojson::value((double)mesh->GetAccessor("POSITION")->GetIndex());
						std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
						if (tex.get())
						{
							attributes["TEXCOORD_0"] = picojson::value((double)tex->GetIndex());
						}

                        std::shared_ptr<Accessor> joints  = mesh->GetAccessor("JOINTS_0");
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

					if (IsOutputDraco)
					{
						const std::vector< std::shared_ptr<BufferView> >& bufferViews = reg.GetBufferViews();
						if (i < bufferViews.size())
						{
							const std::shared_ptr<BufferView>& bufferview = bufferViews[i];
							picojson::object KHR_draco_mesh_compression;
							KHR_draco_mesh_compression["bufferView"] = picojson::value((double)bufferview->GetIndex());//TODO
							if (false)
							{
								//old style
								picojson::array attributesOrder;
								attributesOrder.push_back(picojson::value("POSITION"));
								std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
								if (tex.get())
								{
									attributesOrder.push_back(picojson::value("TEXCOORD_0"));
								}
								attributesOrder.push_back(picojson::value("NORMAL"));
								KHR_draco_mesh_compression["attributesOrder"] = picojson::value(attributesOrder);
								KHR_draco_mesh_compression["version"] = picojson::value("0.9.1");
							}
							else
							{
								//new style
								int nOrder = 0;
								picojson::object attributes;
								attributes["POSITION"] = picojson::value((double)nOrder++);
								std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
								if (tex.get())
								{
									attributes["TEXCOORD_0"] = picojson::value((double)nOrder++);
								}
								attributes["NORMAL"] = picojson::value((double)nOrder++);
								KHR_draco_mesh_compression["attributes"] = picojson::value(attributes);
							}

							picojson::object extensions;
							extensions["KHR_draco_mesh_compression"] = picojson::value(KHR_draco_mesh_compression);
							primitive["extensions"] = picojson::value(extensions);
						}
					}


					picojson::array primitives;
					primitives.push_back(picojson::value(primitive));

					nd["primitives"] = picojson::value(primitives);
					ar.push_back(picojson::value(nd));
				}
				root["meshes"] = picojson::value(ar);
			}

			{//
				const std::vector< std::shared_ptr<Accessor> >& accessors = reg.GetAccessors();
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
					nd["byteOffset"] = accessor->Get("byteOffset");
					//nd["byteStride"] = accessor->Get("byteStride");
					nd["componentType"] = accessor->Get("componentType");
					nd["count"] = accessor->Get("count");
					nd["type"] = accessor->Get("type");
                    auto min_ = accessor->Get("min");
                    if (!min_.is<picojson::null>())
                    {
                        nd["min"] = min_;
                    }
                    auto max_ = accessor->Get("max");
                    if (!max_.is<picojson::null>())
                    {
                        nd["max"] = max_;
                    }
					ar.push_back(picojson::value(nd));
				}
				root["accessors"] = picojson::value(ar);
			}

			{//buffer view
				const std::vector< std::shared_ptr<BufferView> >& bufferViews = reg.GetBufferViews();
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

			{
				picojson::array ar;
				if (IsOutputBin)
				{
					const std::vector< std::shared_ptr<Buffer> >& buffers = reg.GetBuffers();
					for (size_t i = 0; i < buffers.size(); i++)
					{
						const std::shared_ptr<Buffer>& buffer = buffers[i];
						picojson::object nd;
						//nd["name"] = picojson::value(buffer->GetName());
						nd["byteLength"] = picojson::value((double)buffer->GetByteLength());
						nd["uri"] = picojson::value(buffer->GetURI());
						ar.push_back(picojson::value(nd));
					}
				}
				if (IsOutputDraco)
				{
					const std::vector< std::shared_ptr<Buffer> >& buffers = reg.GetBuffersDraco();
					for (size_t i = 0; i < buffers.size(); i++)
					{
						const std::shared_ptr<Buffer>& buffer = buffers[i];
						picojson::object nd;
						//nd["name"] = picojson::value(buffer->GetName());
						nd["byteLength"] = picojson::value((double)buffer->GetByteLength());
						nd["uri"] = picojson::value(buffer->GetURI());
						ar.push_back(picojson::value(nd));
					}
				}
				root["buffers"] = picojson::value(ar);
			}

            {
                const std::vector< std::shared_ptr<Skin> >& skins = reg.GetSkins();
                picojson::array ar;
                for (size_t i = 0; i < skins.size(); i++)
                {
                    const std::shared_ptr<Skin>& skin = skins[i];
                    const std::vector< std::shared_ptr<Node> >& joints = skin->GetJoints();
                    if (!joints.empty())
                    {
                        picojson::object nd;
                        nd["name"] = picojson::value(skin->GetName());
                        {
                            picojson::array nj;
                            for (size_t i = 0; i < joints.size(); i++)
                            {
                                int index = joints[i]->GetIndex();
                                nj.push_back(picojson::value((double)index));
                            }
                            nd["joints"] = picojson::value(nj);
                        }

                        nd["skeleton"] = picojson::value((double)skin->GetRootJoint()->GetIndex());
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

			{
				const auto& materials = node->GetMaterials();
				picojson::array ar;
				for (size_t i = 0; i < materials.size(); i++)
				{
					const auto& mat = materials[i];
					picojson::object nd;
					nd["name"] = picojson::value(mat->GetName());
					picojson::array emissiveFactor;
					emissiveFactor.push_back(picojson::value(0.0));
					emissiveFactor.push_back(picojson::value(0.0));
					emissiveFactor.push_back(picojson::value(0.0));
					nd["emissiveFactor"] = picojson::value(emissiveFactor);

					picojson::object pbrMetallicRoughness;

					std::shared_ptr<kml::Texture> tex = mat->GetTexture("BaseColor");
					if (tex) {
						std::string basecolor_texname = tex->GetFilePath();
						int nIndex = FindTextureIndex(texture_vec, basecolor_texname);
						if (nIndex >= 0)
						{
							picojson::object baseColorTexture;
							baseColorTexture["index"] = picojson::value((double)nIndex);
							pbrMetallicRoughness["baseColorTexture"] = picojson::value(baseColorTexture);
						}
					}
					
					std::shared_ptr<kml::Texture> normaltex = mat->GetTexture("Normal");
					if (normaltex) {
						std::string normal_texname = normaltex->GetFilePath();
						int nIndex = FindTextureIndex(texture_vec, normal_texname);
						if (nIndex >= 0)
						{
							picojson::object normalTexture;
							normalTexture["index"] = picojson::value((double)nIndex);
							nd["normalTexture"] = picojson::value(normalTexture);
						}
					}

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

					pbrMetallicRoughness["metallicFactor"] = picojson::value(mat->GetFloat("metallicFactor"));
					pbrMetallicRoughness["roughnessFactor"] = picojson::value(mat->GetFloat("roughnessFactor"));
					nd["pbrMetallicRoughness"] = picojson::value(pbrMetallicRoughness);

					if (A >= 1.0f)
					{
						nd["alphaMode"] = picojson::value("OPAQUE");
					}
					else
					{
						nd["alphaMode"] = picojson::value("BLEND");
					}

					ar.push_back(picojson::value(nd));
				}
				root["materials"] = picojson::value(ar);
			}

			return true;
		}
		
		static picojson::value makeVec4Value(float x, float y, float z, float w) {
			picojson::array v4;
			v4.push_back(picojson::value(x));
			v4.push_back(picojson::value(y));
			v4.push_back(picojson::value(z));
			v4.push_back(picojson::value(w));
			return picojson::value(v4);
		}

        typedef const char* PCTR;
        struct JointMap {
            PCTR szVRMJointKey;
            PCTR szSubStrs[5];
        };

        static
        int FindJointKeyIndex(const JointMap JointMaps[], const char* key)
        {
            int i = 0;
            while(JointMaps[i].szVRMJointKey)
            {
                if (strcmp(JointMaps[i].szVRMJointKey, key) == 0)
                {
                    return i;
                }
                i++;
            }
            return -1;
        }

        static
        int FindVRNJointIndex(const std::vector<std::string>& joint_names, const std::string& key)
        {
            static const JointMap JointMaps[] = {
                { "hips",{ "hip", "pelvis", NULL, NULL, NULL } },
                { "leftUpperLeg",{ "upperleg", "upleg", NULL, NULL, NULL } },
                { "rightUpperLeg",{ "upperleg", "upleg", NULL, NULL, NULL } },
                { "leftLowerLeg",{ "lowerleg", "leftleg", NULL, NULL, NULL } },
                { "rightLowerLeg",{ "lowerleg", "rightleg", NULL, NULL, NULL } },
                { "leftFoot",{ "foot", NULL, NULL, NULL, NULL } },
                { "rightFoot",{ "foot", NULL, NULL, NULL, NULL } },
                { "spine",{ "spine", NULL, NULL, NULL, NULL } },
                { "chest",{ "chest", "spine1", NULL, NULL, NULL } },
                { "neck",{ "neck", NULL, NULL, NULL, NULL } },
                { "head",{ "head", NULL, NULL, NULL, NULL } },
                { "leftShoulder",{ "shoulder", NULL, NULL, NULL, NULL } },
                { "rightShoulder",{ "shoulder", NULL, NULL, NULL, NULL } },
                { "leftUpperArm",{ "upperarm", "leftarm", NULL, NULL, NULL } },
                { "rightUpperArm",{ "upperarm", "rightarm", NULL, NULL, NULL } },
                { "leftLowerArm",{ "lowerarm", "forearm", NULL, NULL, NULL } },
                { "rightLowerArm",{ "lowerarm", "forearm", NULL, NULL, NULL } },
                { "leftHand",{ "hand", NULL, NULL, NULL, NULL } },
                { "rightHand",{ "hand", NULL, NULL, NULL, NULL } },
                { "leftToes",{ "toe", NULL, NULL, NULL, NULL } },
                { "rightToes",{ "toe", NULL, NULL, NULL, NULL } },
                { "leftEye",{ "eye", NULL, NULL, NULL, NULL } },
                { "rightEye",{ "eye", NULL, NULL, NULL, NULL } },
                { "jaw",{ "jaw", NULL, NULL, NULL, NULL } },

                { "leftThumbProximal",{ "thumbproximal", "thumb1", NULL, NULL, NULL } },
                { "leftThumbIntermediate",{ "thumbintermediate", "thumb2", NULL, NULL, NULL } },
                { "leftThumbDistal",{ "thumbdistal", "thumb3", NULL, NULL, NULL } },

                { "leftIndexProximal",{ "indexproximal", "index1", NULL, NULL, NULL } },
                { "leftIndexIntermediate",{ "indexintermediate", "index2", NULL, NULL, NULL } },
                { "leftIndexDistal",{ "indexdistal", "index3", NULL, NULL, NULL } },

                { "leftMiddleProximal",{ "middleproximal", "middle1", NULL, NULL, NULL } },
                { "leftMiddleIntermediate",{ "middleintermediate", "middle2", NULL, NULL, NULL } },
                { "leftMiddleDistal",{ "middledistal", "middle3", NULL, NULL, NULL } },

                { "leftRingProximal",{ "ringproximal", "ring1", NULL, NULL, NULL } },
                { "leftRingIntermediate",{ "ringintermediate", "ring2", NULL, NULL, NULL } },
                { "leftRingDistal",{ "ringbdistal", "ring3", NULL, NULL, NULL } },

                { "leftLittleProximal",{ "littleproximal", "little1", "pinkey1", NULL, NULL } },
                { "leftLittleIntermediate",{ "littleintermediate", "little2", "pinkey1", NULL, NULL } },
                { "leftLittleDistal",{ "littledistal", "little3", "pinkey3", NULL, NULL } },

                { "rightThumbProximal",{ "thumbproximal", "thumb1", NULL, NULL, NULL } },
                { "rightThumbIntermediate",{ "thumbintermediate", "thumb2", NULL, NULL, NULL } },
                { "rightThumbDistal",{ "thumbdistal", "thumb3", NULL, NULL, NULL } },

                { "rightIndexProximal",{ "indexproximal", "index1", NULL, NULL, NULL } },
                { "rightIndexIntermediate",{ "indexintermediate", "index2", NULL, NULL, NULL } },
                { "rightIndexDistal",{ "indexdistal", "index3", NULL, NULL, NULL } },

                { "rightMiddleProximal",{ "middleproximal", "middle1", NULL, NULL, NULL } },
                { "rightMiddleIntermediate",{ "middleintermediate", "middle2", NULL, NULL, NULL } },
                { "rightMiddleDistal",{ "middledistal", "middle3", NULL, NULL, NULL } },

                { "rightRingProximal",{ "ringproximal", "ring1", NULL, NULL, NULL } },
                { "rightRingIntermediate",{ "ringintermediate", "ring2", NULL, NULL, NULL } },
                { "rightRingDistal",{ "ringbdistal", "ring3", NULL, NULL, NULL } },

                { "rightLittleProximal",{ "littleproximal", "little1", "pinkey1", NULL, NULL } },
                { "rightLittleIntermediate",{ "littleintermediate", "little2", "pinkey2", NULL, NULL } },
                { "rightLittleDistal",{ "littledistal", "little3", "pinkey3", NULL, NULL } },

                { "upperChest",{ "upperchest", "spine2", NULL, NULL, NULL } },


                { NULL,{ NULL, NULL, NULL, NULL, NULL } }
            };

            static const PCTR LeftKeys[]  = {"l_", "left", NULL};
            static const PCTR RightKeys[] = { "r_", "right", NULL };

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
                        std::string subKey = "_" + std::string(subStrs[j]);
                        if (strstr(joint_name.c_str(), subKey.c_str() ) != NULL)
                        {
                            if (key == "spine")
                            {
                                if (strstr(joint_name.c_str(), "spine1") != NULL)continue;
                                if (strstr(joint_name.c_str(), "spine2") != NULL)continue;
                            }

                            return i;
                        }
                        j++;
                    }
                }
            }
            
            return -1;
        }

		static
		bool WriteVRMMetaInfo(picojson::object& root_object, const std::shared_ptr<::kml::Node>& node)
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
				VRM["exporterVersion"] = picojson::value("kashikaVRM-1.00");
			}

			{
				picojson::object meta;
				meta["version"] = picojson::value("");
				meta["author"] = picojson::value("user_name"); // TODO: input from param
				meta["contactInformation"] = picojson::value("");
				meta["reference"] = picojson::value("");
				meta["title"] = picojson::value("user_title"); // TODO: input from param
				meta["texture"] = picojson::value(0.0); // TODO: count of texture
				meta["allowedUserName"] = picojson::value("Everyone");
				meta["violentUssageName"] = picojson::value("Disallow");
				meta["sexualUssageName"] = picojson::value("Disallow");
				meta["commercialUssageName"] = picojson::value("Disallow");
				meta["otherPermissionUrl"] = picojson::value("");
				meta["licenseName"] = picojson::value("Redistribution_Prohibited");
				meta["otherLicenseUrl"] = picojson::value("");
				VRM["meta"] = picojson::value(meta);
			}
			{
				picojson::object humanoid;
				picojson::array humanBones;
				static const char* boneNames[] = {
					"hips", "leftUpperLeg","rightUpperLeg", "leftLowerLeg","rightLowerLeg",
					"leftFoot", "rightFoot", "spine", "chest", "neck", "head", "leftShoulder",
					"rightShoulder", "leftUpperArm", "rightUpperArm", "leftLowerArm","rightLowerArm",
					"leftHand", "rightHand","leftToes","rightToes",	"leftEye","rightEye","jaw",
					"leftThumbProximal","leftThumbIntermediate","leftThumbDistal","leftIndexProximal",
					"leftIndexIntermediate","leftIndexDistal","leftMiddleProximal","leftMiddleIntermediate",
					"leftMiddleDistal","leftRingProximal","leftRingIntermediate","leftRingDistal",
					"leftLittleProximal","leftLittleIntermediate","leftLittleDistal","rightThumbProximal",
					"rightThumbIntermediate","rightThumbDistal", "rightIndexProximal","rightIndexIntermediate",
					"rightIndexDistal","rightMiddleProximal","rightMiddleIntermediate","rightMiddleDistal",
					"rightRingProximal","rightRingIntermediate","rightRingDistal","rightLittleProximal",
					"rightLittleIntermediate","rightLittleDistal","upperChest" };

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
					picojson::object info;
					info["bone"] = picojson::value(boneNames[i]);
					info["node"] = picojson::value((double)FindVRNJointIndex(joint_names, boneNames[i]));             // TODO: find from Node
					info["useDefaultValues"] = picojson::value(true); // what's this?
					humanBones.push_back(picojson::value(info));
				}
				humanoid["humanBones"] = picojson::value(humanBones);
				VRM["humanoid"] = picojson::value(humanoid);
			}

			{
				picojson::object firstPerson;
				firstPerson["firstPersonBone"] = picojson::value(-1.0);
				picojson::object firstPersonBoneOffset;
				firstPersonBoneOffset["x"] = picojson::value(0.0);
				firstPersonBoneOffset["y"] = picojson::value(0.0);
				firstPersonBoneOffset["z"] = picojson::value(0.0);
				firstPerson["firstPersonBoneOffset"] = picojson::value(firstPersonBoneOffset);
				firstPerson["meshAnnotations"] = picojson::value(picojson::array());
				firstPerson["lookAtTypeName"] = picojson::value("Bone");

				picojson::object lookAtHorizontalInner;
				lookAtHorizontalInner["xRange"] = picojson::value(90.0);
				lookAtHorizontalInner["yRange"] = picojson::value(10.0);
				firstPerson["lookAtHorizontalInner"] = picojson::value(lookAtHorizontalInner);
				
				picojson::object lookAtHorizontalOuter;
				lookAtHorizontalOuter["xRange"] = picojson::value(90.0);
				lookAtHorizontalOuter["yRange"] = picojson::value(10.0);
				firstPerson["lookAtHorizontalOuter"] = picojson::value(lookAtHorizontalOuter);
				
				picojson::object lookAtVerticalDown;
				lookAtVerticalDown["xRange"] = picojson::value(90.0);
				lookAtVerticalDown["yRange"] = picojson::value(10.0);
				firstPerson["lookAtVerticalDown"] = picojson::value(lookAtVerticalDown);
				
				picojson::object lookAtVerticalUp;
				lookAtVerticalUp["xRange"] = picojson::value(90.0);
				lookAtVerticalUp["yRange"] = picojson::value(10.0);
				firstPerson["lookAtVerticalDown"] = picojson::value(lookAtVerticalUp);

				VRM["firstPerson"] = picojson::value(firstPerson);
			}

			{
				picojson::object blendShapeMaster;
				VRM["blendShapeMaster"] = picojson::value(blendShapeMaster);

				picojson::array blendShapeGroups;
				picojson::object shapeinfo;
				static const char* names[] = { "Neutral", "A", "I", "U", "E", "O" };
				for (int i = 0; i < sizeof(names) / sizeof(const char*); ++i) {
					shapeinfo["name"] = picojson::value(names[i]);
					shapeinfo["presetName"] = picojson::value("unknown");
					shapeinfo["binds"] = picojson::value(picojson::array());
					shapeinfo["materialValues"] = picojson::value(picojson::array());
					blendShapeGroups.push_back(picojson::value(shapeinfo));
				}
				blendShapeMaster["blendShapeGroups"] = picojson::value(blendShapeGroups);
			}

			{
				picojson::object secondaryAnimation;
				secondaryAnimation["boneGroups"] = picojson::value(picojson::array());
				secondaryAnimation["colliderGroups"] = picojson::value(picojson::array());
				VRM["secondaryAnimation"] = picojson::value(secondaryAnimation);
			}

			{
				const auto& materials = node->GetMaterials();
				picojson::array materialProperties;
				for (size_t i = 0; i < materials.size(); ++i) {
					picojson::object mat;
					mat["name"] = picojson::value(materials[i]->GetName());
					mat["renderQueue"] = picojson::value(3000.0);
					mat["shader"] = picojson::value("VRM/MToon");
					picojson::object floatProperties;

					mat["floatProperties"] = picojson::value(floatProperties);
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

					picojson::object vectorProperties;
					vectorProperties["_Color"] = makeVec4Value(0.8450866, 0.05577575, 0.05577575, 1.0);
					vectorProperties["_ShadeColor"] = makeVec4Value(0.391907483, 0.217116714, 0.217116714, 1.0);
					vectorProperties["_EmissionColor"] = makeVec4Value(0.858823538, 0.694117665, 0.694117665, 1.0);
					vectorProperties["_MainTex"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					vectorProperties["_ShadeTexture"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					vectorProperties["_BumpMap"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					vectorProperties["_ReceiveShadowTexture"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					vectorProperties["_SphereAdd"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					vectorProperties["_EmissionMap"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					vectorProperties["_OutlineWidthTexture"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					vectorProperties["_OutlineColor"] = makeVec4Value(0.0, 0.0, 1.0, 1.0);
					mat["vectorProperties"] = picojson::value(vectorProperties);

					picojson::object textureProperties;
					/*	"_MainTex": 0,                 // TODO
						"_ShadeTexture" : 0,
						"_SphereAdd" : 1,
						"_EmissionMap" : 2
					},*/
					mat["textureProperties"] = picojson::value(textureProperties);

	
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
	}
	//-----------------------------------------------------------------------------

	static
	bool ExportGLTF(const std::string& path, const std::shared_ptr<Node>& node, const std::shared_ptr<Options>& opts, bool prettify = true)
	{
		bool output_bin = true;
		bool output_draco = true;
		bool union_buffer_draco = true;

		//std::shared_ptr<Options> opts = Options::GetGlobalOptions();
		bool vrm_export = opts->GetInt("vrm_export") > 0;
	    int output_buffer = opts->GetInt("output_buffer");

		if (output_buffer == 0)
		{
			output_bin = true;
			output_draco = false;
			union_buffer_draco = false;
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
			asset["generator"] = picojson::value("glTF-Maya-Exporter");
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

			if (!extensionsUsed.empty())
			{
				root_object["extensionsUsed"] = picojson::value(extensionsUsed);
			}
			if (!extensionsRequired.empty())
			{
				root_object["extensionsRequired"] = picojson::value(extensionsRequired);
			}
		}


		if (!gltf::NodeToGLTF(root_object, reg, node, output_bin, output_draco, union_buffer_draco))
		{
			return false;
		}

		if (vrm_export)
        {
			if (!gltf::WriteVRMMetaInfo(root_object, node)) {
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

		if (output_bin)
		{
			const std::shared_ptr<kml::gltf::Buffer>& buffer = reg.GetBuffers()[0];
			if (buffer->GetSize() > 0)
			{
				std::string binfile = base_dir + buffer->GetURI();
				std::ofstream ofs(binfile.c_str(), std::ofstream::binary);
				if (!ofs)
				{
					std::cerr << "Couldn't write bin outputfile : " << binfile << std::endl;
					return false;
				}
				ofs.write((const char*)buffer->GetBytesPtr(), buffer->GetByteLength());
			}
		}

		if (output_draco)
		{
			const std::vector< std::shared_ptr<kml::gltf::Buffer> >& buffers = reg.GetBuffersDraco();
			//std::cerr << "Draco Buffer Size"<< buffers.size() << std::endl;
			for (size_t j = 0; j < buffers.size(); j++)
			{
				const std::shared_ptr<kml::gltf::Buffer>& buffer = buffers[j];
				std::string binfile = base_dir + buffer->GetURI();
				std::ofstream ofs(binfile.c_str(), std::ofstream::binary);
				if (!ofs)
				{
					std::cerr << "Couldn't write Draco bin outputfile :" << binfile << std::endl;
					return -1;
				}
				ofs.write((const char*)buffer->GetBytesPtr(), buffer->GetByteLength());
			}
		}

		return true;
	}


	bool glTFExporter::Export(const std::string& path, const std::shared_ptr<Node>& node, const std::shared_ptr<Options>& opts)const
	{
		return ExportGLTF(path, node, opts);
	}

	
}
