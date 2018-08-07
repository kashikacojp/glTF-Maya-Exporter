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
		void GetTextures(std::set<std::string>& texture_set, const std::vector< std::shared_ptr<::kml::Material> >& materials)
	{
		for (size_t j = 0; j < materials.size(); j++)
		{
			const auto& mat = materials[j];
			auto keys = mat->GetTextureKeys();
			for (int i = 0; i < keys.size(); i++)
			{
				std::shared_ptr<kml::Texture> tex = mat->GetTexture(keys[i]);
				std::string texpath = tex->GetFilePath();
				texture_set.insert(texpath);
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

	namespace gltf
	{
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

		class Node
		{
		public:
			Node(const std::string& name, int index)
				:name_(name), index_(index), matrix_(1.0f)
			{}
			const std::string& GetName()const
			{
				return name_;
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
			const glm::mat4 GetMatrix()const
			{
				return matrix_;
			}
			void SetMatrix(const glm::mat4& mat)
			{
				matrix_ = mat;
			}
		protected:
			std::string name_;
			int index_;
			glm::mat4 matrix_;
			std::shared_ptr<Mesh> mesh_;
			std::vector< std::shared_ptr<Node> > children_;
		};

		static
			void GetMinMax(float min[], float max[], const std::vector<float>& verts, int n)
		{
			for (int i = 0; i<n; i++)
			{
				min[i] = std::numeric_limits<float>::max();
				max[i] = std::numeric_limits<float>::min();
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
			picojson::array ConvertToArray(float v[], int n)
		{
			picojson::array a;
			for (int j = 0; j<n; j++)
			{
				a.push_back(picojson::value(v[j]));
			}
			return a;
		}

		class ObjectRegister
		{
		public:
			ObjectRegister(const std::string& basename)
			{
				basename_ = basename;
			}
			std::shared_ptr<Node> RegisterObject(const std::shared_ptr<::kml::Node>& in_node)
			{
				int nNode = nodes_.size();
				std::string nodeName = in_node->GetName();
				std::shared_ptr<Node> node(new Node(nodeName, nNode));

				node->SetMatrix(in_node->GetTransform()->GetMatrix());

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
						const std::shared_ptr<BufferView>& bufferView = this->AddBufferView(indices);
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
					node->SetMesh(mesh);
					this->meshes_.push_back(mesh);
				}

				this->AddNode(node);

				return node;
			}

			std::shared_ptr<Node> RegisterObjectDraco(const std::shared_ptr<::kml::Node>& in_node, bool is_union_buffer = false)
			{
				int nNode = nodes_.size();
				std::string nodeName = in_node->GetName();
				std::shared_ptr<Node> node(new Node(nodeName, nNode));

				node->SetMatrix(in_node->GetTransform()->GetMatrix());

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

					node->SetMesh(mesh);
					this->meshes_.push_back(mesh);
				}

				this->AddNode(node);
				return node;
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
			const std::vector<std::shared_ptr<Buffer> >& GetBuffersDraco()const
			{
				return dracoBuffers_;
			}
		protected:
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
			const std::shared_ptr<BufferView>& AddBufferView(const std::vector<float>& vec)
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
				bufferView->SetTarget(GLTF_TARGET_ARRAY_BUFFER);
				bufferViews_.push_back(bufferView);
				return bufferViews_.back();
			}

			const std::shared_ptr<BufferView>& AddBufferView(const std::vector<unsigned int>& vec)
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
				bufferView->SetTarget(GLTF_TARGET_ELEMENT_ARRAY_BUFFER);
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
			std::shared_ptr<Node> RegisterNodes(
				ObjectRegister& reg,
				const std::shared_ptr<::kml::Node>& node,
				bool IsOutputBin,
				bool IsOutputDraco, bool IsUnionBufferDraco)
		{
			std::shared_ptr<Node> ret_node;

			if (IsOutputBin)
			{
				ret_node = reg.RegisterObject(node);
			}
			else if (IsOutputDraco)
			{
				ret_node = reg.RegisterObjectDraco(node, IsUnionBufferDraco);
			}

			{
				auto& children = node->GetChildren();
				for (size_t i = 0; i < children.size(); i++)
				{
					auto child_node = RegisterNodes(reg, children[i], IsOutputBin, IsOutputDraco, IsUnionBufferDraco);
					if (ret_node.get() && child_node.get())
					{
						ret_node->AddChild(child_node);
					}
				}
			}
			return ret_node;
		}

		static
		picojson::value createLTE_pbr_material(const std::shared_ptr<kml::Material> mat, const std::vector<std::string>& texture_vec) {
			picojson::object LTE_pbr_material;
			
			// base
			picojson::array baseColorFactor;
			baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorR")));
			baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorG")));
			baseColorFactor.push_back(picojson::value(mat->GetFloat("ai_baseColorB")));
			LTE_pbr_material["baseWeight"]       = picojson::value(mat->GetFloat("ai_baseWeight"));
			LTE_pbr_material["baseColor"]        = picojson::value(baseColorFactor);
			LTE_pbr_material["diffuseRoughness"] = picojson::value(mat->GetFloat("ai_diffuseRoughness"));
			LTE_pbr_material["metalness"]        = picojson::value(mat->GetFloat("ai_metalness"));
			std::shared_ptr <kml::Texture> ai_baseTex = mat->GetTexture("ai_baseColor");
			if (ai_baseTex) {
				const std::string ai_basetexname = ai_baseTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_basetexname);
				if (nIndex >= 0)
				{
					picojson::object baseColorTexture;
					baseColorTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["baseColorTexture"] = picojson::value(baseColorTexture);
				}
			}

			// specular
			picojson::array specularColorFactor;
			specularColorFactor.push_back(picojson::value(mat->GetFloat("ai_specularColorR")));
			specularColorFactor.push_back(picojson::value(mat->GetFloat("ai_specularColorG")));
			specularColorFactor.push_back(picojson::value(mat->GetFloat("ai_specularColorB")));
			LTE_pbr_material["specularWeight"]     = picojson::value(mat->GetFloat("ai_specularWeight"));
			LTE_pbr_material["specularColor"]      = picojson::value(specularColorFactor);
			LTE_pbr_material["specularRoughness"]  = picojson::value(mat->GetFloat("ai_specularRoughness"));
			LTE_pbr_material["specularIOR"]        = picojson::value(mat->GetFloat("ai_specularIOR"));
			LTE_pbr_material["specularRotation"]   = picojson::value(mat->GetFloat("ai_specularRotation"));
			LTE_pbr_material["specularAnisotropy"] = picojson::value(mat->GetFloat("ai_specularAnisotropy"));
			std::shared_ptr <kml::Texture> ai_specularTex = mat->GetTexture("ai_specularColor");
			if (ai_specularTex) {
				const std::string ai_specularTexname = ai_specularTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_specularTexname);
				if (nIndex >= 0)
				{
					picojson::object specularColorTexture;
					specularColorTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["specularColorTexture"] = picojson::value(specularColorTexture);
				}
			}
			
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
			std::shared_ptr <kml::Texture> ai_transmissionTex = mat->GetTexture("ai_transmissionColor");
			if (ai_transmissionTex) {
				const std::string ai_transmissionTexname = ai_transmissionTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_transmissionTexname);
				if (nIndex >= 0)
				{
					picojson::object transmissionColorTexture;
					transmissionColorTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["transmissionColorTexture"] = picojson::value(transmissionColorTexture);
				}
			}
			std::shared_ptr <kml::Texture> ai_transmissionScatterTex = mat->GetTexture("ai_transmissionScatter");
			if (ai_transmissionScatterTex) {
				const std::string ai_transmissionScatterTexname = ai_transmissionScatterTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_transmissionScatterTexname);
				if (nIndex >= 0)
				{
					picojson::object transmissionScatterTexture;
					transmissionScatterTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["transmissionScatterTexture"] = picojson::value(transmissionScatterTexture);
				}
			}

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
				std::string("randomwalk")
			};
			LTE_pbr_material["subsurfaceWeight"] = picojson::value(mat->GetFloat("ai_subsurfaceWeight"));
			LTE_pbr_material["subsurfaceColor"] = picojson::value(subsurfaceColorFactor);
			LTE_pbr_material["subsurfaceRadius"] = picojson::value(subsurfaceRadius);
			LTE_pbr_material["subsurfaceType"] = picojson::value(stype[mat->GetInteger("ai_subsurfaceType")]);
			LTE_pbr_material["subsurfaceScale"] = picojson::value(mat->GetFloat("ai_subsurfaceScale"));
			LTE_pbr_material["subsurfaceAnisotropy"] = picojson::value(mat->GetFloat("ai_subsurfaceAnisotropy"));
			std::shared_ptr <kml::Texture> ai_subsurfaceColorTex = mat->GetTexture("ai_subsurfaceColor");
			if (ai_subsurfaceColorTex) {
				const std::string ai_subsurfaceColorTexname = ai_subsurfaceColorTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_subsurfaceColorTexname);
				if (nIndex >= 0)
				{
					picojson::object subsurfaceColorTexture;
					subsurfaceColorTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["subsurfaceColorTexture"] = picojson::value(subsurfaceColorTexture);
				}
			}
			
			std::shared_ptr <kml::Texture> ai_subsurfaceRadiusTex = mat->GetTexture("ai_subsurfaceRadius");
			if (ai_subsurfaceRadiusTex) {
				const std::string ai_subsurfaceRadiusTexname = ai_subsurfaceRadiusTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_subsurfaceRadiusTexname);
				if (nIndex >= 0)
				{
					picojson::object subsurfaceRadiusTexture;
					subsurfaceRadiusTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["subsurfaceRadiusTexture"] = picojson::value(subsurfaceRadiusTexture);
				}
			}
		
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

			std::shared_ptr <kml::Texture> ai_coatColorTex = mat->GetTexture("ai_coatColor");
			if (ai_coatColorTex) {
				const std::string ai_coatColorTexname = ai_coatColorTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_coatColorTexname);
				if (nIndex >= 0)
				{
					picojson::object ai_coatColorTexture;
					ai_coatColorTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["coatColorTexture"] = picojson::value(ai_coatColorTexture);
				}
			}
			
			// Emissive
			picojson::array emissiveColor;
			emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorR")));
			emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorG")));
			emissiveColor.push_back(picojson::value(mat->GetFloat("ai_emissionColorB")));
			LTE_pbr_material["emissionWeight"] = picojson::value(mat->GetFloat("ai_emissionWeight"));
			LTE_pbr_material["emissionColor"] = picojson::value(emissiveColor);
			std::shared_ptr <kml::Texture> ai_emissionColorTex = mat->GetTexture("ai_emissionColor");
			if (ai_emissionColorTex) {
				const std::string ai_emissionColorTexname = ai_emissionColorTex->GetFilePath();
				const int nIndex = FindTextureIndex(texture_vec, ai_emissionColorTexname);
				if (nIndex >= 0)
				{
					picojson::object ai_emissionColorTexture;
					ai_emissionColorTexture["index"] = picojson::value((double)nIndex);
					LTE_pbr_material["emissionColorTexture"] = picojson::value(ai_emissionColorTexture);
				}
			}

			// Output LTE_PBR_material
			picojson::object extensions;
			extensions["LTE_PBR_material"] = picojson::value(LTE_pbr_material);
			return picojson::value(extensions);
		}

		picojson::array GetMatrixAsArray(const glm::mat4& mat)
		{
			picojson::array matrix;
			const float* ptr = glm::value_ptr(mat);
			for (int i = 0; i < 16; i++)
			{
				matrix.push_back(picojson::value(ptr[i]));
			}

			return matrix;
		}


		static
		bool NodeToGLTF(
			picojson::object& root,
			ObjectRegister& reg,
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
			{
				std::set<std::string> texture_set;
				GetTextures(texture_set, node->GetMaterials());

				//std::ofstream fff("c:\\src\\debug.txt");

				static const std::string t = "_s0.";
				for (std::set<std::string>::const_iterator it = texture_set.begin(); it != texture_set.end(); ++it)
				{
					std::string tex = *it;
					//fff << tex << std::endl;

					if (tex.find(t) == std::string::npos)
					{
						texture_vec.push_back(tex);
					}
					else
					{
						std::string orgPath = tex;
						orgPath.replace(tex.find(t), t.size(), ".");
						orgPath = RemoveExt(orgPath);
						cache_map.insert(CacheMapType::value_type(orgPath, tex));
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
				RegisterNodes(reg, node, IsOutputBin, IsOutputDraco, IsUnionBufferDraco);
			}


			{
				root["scene"] = picojson::value((double)0);
			}

			{
				picojson::array ar;

				picojson::object scene;
				picojson::array nodes_;
				const std::vector< std::shared_ptr<Node> >& nodes = reg.GetNodes();
				for (size_t i = 0; i < nodes.size(); i++)
				{
					nodes_.push_back(picojson::value((double)nodes[i]->GetIndex()));
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

					nd["matrix"] = picojson::value(GetMatrixAsArray(n->GetMatrix()));

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
						attributes["POSITION"] = picojson::value((double)mesh->GetAccessor("POSITION")->GetIndex());;
						std::shared_ptr<Accessor> tex = mesh->GetAccessor("TEXCOORD_0");
						if (tex.get())
						{
							attributes["TEXCOORD_0"] = picojson::value((double)tex->GetIndex());
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
					nd["min"] = accessor->Get("min");
					nd["max"] = accessor->Get("max");
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
					nd["target"] = picojson::value((double)bufferView->GetTarget());
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

					// LTE extenstion
					nd["extensions"] = createLTE_pbr_material(mat, texture_vec);
					
					ar.push_back(picojson::value(nd));
				}
				root["materials"] = picojson::value(ar);
			}

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
		gltf::ObjectRegister reg(base_name);
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

			// LTE extention
			extensionsUsed.push_back(picojson::value("LTE_PBR_material"));

			root_object["extensionsUsed"] = picojson::value(extensionsUsed);
			root_object["extensionsRequired"] = picojson::value(extensionsRequired);
		}


		if (!gltf::NodeToGLTF(root_object, reg, node, output_bin, output_draco, union_buffer_draco))
		{
			return false;
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
