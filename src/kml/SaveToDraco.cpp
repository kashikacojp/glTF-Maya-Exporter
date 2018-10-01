#define GLM_ENABLE_EXPERIMENTAL 1

#include "SaveToDraco.h"

#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdint>

#include <glm/glm.hpp>

#ifdef ENABLE_BUILD_WITH_DRACO
#include <draco/mesh/mesh.h>
#include <draco/compression/encode.h>
#include <draco/core/cycle_timer.h>
#include <draco/io/mesh_io.h>
#include <draco/io/point_cloud_io.h>
#include <draco/io/obj_decoder.h>
#endif

namespace kml
{
	namespace ns
	{
		struct Options {
			Options();

			bool is_point_cloud;
			int pos_quantization_bits;
			int tex_coords_quantization_bits;
			bool tex_coords_deleted;
			int normals_quantization_bits;
			bool normals_deleted;
            int color_quantization_bits;
            int generic_quantization_bits;
			int compression_level;
			bool use_metadata;
			std::string input;
			std::string output;
		};

		Options::Options()
			: is_point_cloud(false),
			pos_quantization_bits(14),
			tex_coords_quantization_bits(12),
			tex_coords_deleted(false),
			normals_quantization_bits(10),
			normals_deleted(false),
            color_quantization_bits(10),
            generic_quantization_bits(14),
			compression_level(7) {}

	}

#ifdef ENABLE_BUILD_WITH_DRACO

    template<class T>
    struct DracoTraits {};

    template<>
    struct DracoTraits<int8_t>
    {
        static draco::DataType GetDataType(){return draco::DataType::DT_INT8;}
    };

    template<>
    struct DracoTraits<uint8_t>
    {
        static draco::DataType GetDataType() { return draco::DataType::DT_UINT8; }
    };

    template<>
    struct DracoTraits<int16_t>
    {
        static draco::DataType GetDataType() { return draco::DataType::DT_INT16; }
    };

    template<>
    struct DracoTraits<uint16_t>
    {
        static draco::DataType GetDataType() { return draco::DataType::DT_UINT16; }
    };

    template<>
    struct DracoTraits<int32_t>
    {
        static draco::DataType GetDataType() { return draco::DataType::DT_INT32; }
    };

    template<>
    struct DracoTraits<uint32_t>
    {
        static draco::DataType GetDataType() { return draco::DataType::DT_UINT32; }
    };

    template<>
    struct DracoTraits<float>
    {
        static draco::DataType GetDataType() { return draco::DataType::DT_FLOAT32; }
    };

    template<>
    struct DracoTraits<double>
    {
        static draco::DataType GetDataType() { return draco::DataType::DT_FLOAT64; }
    };

    static
    int GetNumComponents(const std::string& type)
    {
        if (type == "VEC2")return 2;
        if (type == "VEC3")return 3;
        if (type == "VEC4")return 4;
        return 1;
    }

    static
    bool GetNormalized(const std::string& attr)
    {
        if (attr == "NORMAL") return true;
        if (attr == "TANGENT")return true;
        return false;
    }

    static
    draco::GeometryAttribute::Type GetAttributeType(const std::string& name)
    {
        if (name == "POSITION")
        {
            return draco::GeometryAttribute::Type::POSITION;
        }
        if (name == "NORMAL")
        {
            return draco::GeometryAttribute::Type::NORMAL;
        }
        if (name == "TEXCOORD_0")
        {
            return draco::GeometryAttribute::Type::TEX_COORD;
        }
        if (name == "TEXCOORD_1")
        {
            return draco::GeometryAttribute::Type::TEX_COORD;
        }
        if (name == "COLOR_0")
        {
            return draco::GeometryAttribute::Type::COLOR;
        }
        if (name == "JOINTS_0")
        {
            return draco::GeometryAttribute::Type::GENERIC;
        }
        if (name == "WEIGHTS_0")
        {
            return draco::GeometryAttribute::Type::GENERIC;
        }
        if (name == "TANGENT")
        {
            return draco::GeometryAttribute::Type::GENERIC;
        }
        return draco::GeometryAttribute::Type::GENERIC;
    }

    template<class T>
    int CreateDracoBuffer(draco::Mesh& dracoMesh, const std::string& attr, const std::shared_ptr<gltf::Accessor>& acc)
    {
        draco::GeometryAttribute::Type attrType = GetAttributeType(attr);
        std::shared_ptr<gltf::DracoTemporaryBuffer> buf = acc->GetDracoTemporaryBuffer();
        T* values = (T*)(buf->GetBytesPtr());
        bool normalized = GetNormalized(attr);
        int numComponents = GetNumComponents(acc->GetType());
        uint32_t count = acc->GetCount();
        uint32_t stride = sizeof(T) * numComponents;
        draco::PointAttribute pointAttr;
        pointAttr.Init(attrType, nullptr, numComponents, DracoTraits<T>::GetDataType(), normalized, stride, 0);
        int attId = dracoMesh.AddAttribute(pointAttr, true, static_cast<unsigned int>(count));
        auto attrActual = dracoMesh.attribute(attId);

        if (dracoMesh.num_points() == 0)
        {
            dracoMesh.set_num_points(count);
        }
        for (draco::PointIndex i(0); i < count; i++)
        {
            attrActual->SetAttributeValue(attrActual->mapped_index(i), &values[i.value() * numComponents]);
        }

        return attId;
    }

    bool SaveToDraco(std::vector<unsigned char>& bytes, const std::shared_ptr<gltf::Mesh>& mesh)
    {
        std::unique_ptr<ns::Options> options(new ns::Options());
        int speed = 10 - options->compression_level;
        std::unique_ptr<draco::Encoder> dracoEncoder(new draco::Encoder());
        {
            dracoEncoder->SetAttributeQuantization(draco::GeometryAttribute::POSITION, options->pos_quantization_bits);
            dracoEncoder->SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, options->tex_coords_quantization_bits);
            dracoEncoder->SetAttributeQuantization(draco::GeometryAttribute::NORMAL, options->normals_quantization_bits);
            dracoEncoder->SetAttributeQuantization(draco::GeometryAttribute::COLOR, options->color_quantization_bits);
            dracoEncoder->SetAttributeQuantization(draco::GeometryAttribute::GENERIC, options->generic_quantization_bits);
            dracoEncoder->SetSpeedOptions(speed, speed);
            //dracoEncoder->SetTrackEncodedProperties(true);
        }

        std::unique_ptr<draco::Mesh> dracoMesh(new draco::Mesh());

        {
            std::shared_ptr<gltf::Accessor> accIndices = mesh->GetIndices();
            std::shared_ptr<gltf::DracoTemporaryBuffer> bufIndices = accIndices->GetDracoTemporaryBuffer();
            unsigned int* indices = (unsigned int*)(bufIndices->GetBytesPtr());
            size_t numFaces = accIndices->GetCount() / 3;
            dracoMesh->SetNumFaces(numFaces);
            for (uint32_t i = 0; i < numFaces; i++)
            {
                draco::Mesh::Face face;
                face[0] = indices[(i * 3) + 0];
                face[1] = indices[(i * 3) + 1];
                face[2] = indices[(i * 3) + 2];
                dracoMesh->SetFace(draco::FaceIndex(i), face);
            }
        }

        //Add buffers
        {
            static const char* ATTRS[] = {
                "POSITION", "TEXCOORD_0", "TEXCOORD_1", "NORMAL", "COLOR_0", "JOINTS_0", "WEIGHTS_0", "TANGENT", NULL
            };
            int i = 0; 
            while(ATTRS[i])
            {
                std::shared_ptr<gltf::Accessor> acc = mesh->GetAccessor(ATTRS[i]);
                int attId = 0;
                if (acc.get())
                {
                    switch (acc->GetComponentType())
                    {
                    case GLTF_COMPONENT_TYPE_BYTE:          attId = CreateDracoBuffer<  int8_t>(*dracoMesh, ATTRS[i], acc); break;
                    case GLTF_COMPONENT_TYPE_UNSIGNED_BYTE: attId = CreateDracoBuffer< uint8_t>(*dracoMesh, ATTRS[i], acc); break;
                    case GLTF_COMPONENT_TYPE_SHORT:         attId = CreateDracoBuffer< int16_t>(*dracoMesh, ATTRS[i], acc); break;
                    case GLTF_COMPONENT_TYPE_UNSIGNED_SHORT:attId = CreateDracoBuffer<uint16_t>(*dracoMesh, ATTRS[i], acc); break;
                    case GLTF_COMPONENT_TYPE_INT:           attId = CreateDracoBuffer< int32_t>(*dracoMesh, ATTRS[i], acc); break;
                    case GLTF_COMPONENT_TYPE_UNSIGNED_INT:  attId = CreateDracoBuffer<uint32_t>(*dracoMesh, ATTRS[i], acc); break;
                    case GLTF_COMPONENT_TYPE_FLOAT:         attId = CreateDracoBuffer<   float>(*dracoMesh, ATTRS[i], acc); break;
                    case GLTF_COMPONENT_TYPE_DOUBLE:        attId = CreateDracoBuffer<  double>(*dracoMesh, ATTRS[i], acc); break;
                    }

                    int order = dracoMesh->attribute(attId)->unique_id();
                    mesh->SetOrderInDraco(ATTRS[i], order);
                }
                i++;
            }
        }

        //Encode
        dracoMesh->DeduplicateAttributeValues();
        dracoMesh->DeduplicatePointIds();

        draco::EncoderBuffer buffer;
        const draco::Status status = dracoEncoder->EncodeMeshToBuffer(*dracoMesh, &buffer);
        if (!status.ok())
        {
            return false;
        }

        //Copy
        {
            const unsigned char* data_begin = (const unsigned char*)(buffer.data());
            const unsigned char* data_end = data_begin + buffer.size();
            std::copy(data_begin, data_end, std::back_inserter(bytes));
        }

        return true;
    }
#else
    bool SaveToDraco(std::vector<unsigned char>& bytes, const std::shared_ptr<gltf::Mesh>& mesh)
    {
        // Disable ENABLE_BUILD_WITH_DRACO option
        return false;
    }
#endif
}

