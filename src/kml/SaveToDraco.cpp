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

    template<class T>
    int CreateDracoBuffer(draco::Mesh& dracoMesh, draco::GeometryAttribute::Type attrType, int numComponents, bool normalized, const std::vector<T>& values)
    {
        uint32_t count  = int(values.size()) / numComponents;
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

	bool SaveToDraco(std::vector<unsigned char>& bytes, const std::shared_ptr<Mesh>& mesh)
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

        size_t numFaces = mesh->facenums.size();
        dracoMesh->SetNumFaces(numFaces);
        for (uint32_t i = 0; i < numFaces; i++)
        {
            draco::Mesh::Face face;
            face[0] = mesh->pos_indices[(i * 3) + 0];
            face[1] = mesh->pos_indices[(i * 3) + 1];
            face[2] = mesh->pos_indices[(i * 3) + 2];
            dracoMesh->SetFace(draco::FaceIndex(i), face);
        }

        //Add buffers
        {
            //index
            //P
            //T
            //V
            {
                //std::vector<unsigned int> buf(mesh->pos_indices.size());
                //for (size_t i = 0; i < buf.size(); i++)
                //{
                //    buf[i] = (unsigned int)mesh->pos_indices[i];
                //}
                //int attId = CreateDracoBuffer(*dracoMesh, 1, false, buf);
            }
            
            {
                std::vector<float> buf(mesh->positions.size() * 3);
                for (size_t i = 0; i < mesh->positions.size(); i++)
                {
                    buf[3 * i + 0] = (float)mesh->positions[i][0];
                    buf[3 * i + 1] = (float)mesh->positions[i][1];
                    buf[3 * i + 2] = (float)mesh->positions[i][2];
                }
                int attId = CreateDracoBuffer(*dracoMesh, draco::GeometryAttribute::Type::POSITION, 3, false, buf);
            }
            
            if (mesh->texcoords.size() > 0)
            {
                std::vector<float> buf(mesh->texcoords.size() * 2);
                for (size_t i = 0; i < mesh->texcoords.size(); i++)
                {
                    buf[2 * i + 0] = (float)mesh->texcoords[i][0];
                    buf[2 * i + 1] = (float)mesh->texcoords[i][1];
                }
                int attId = CreateDracoBuffer(*dracoMesh, draco::GeometryAttribute::Type::TEX_COORD, 2, false, buf);
            }

            {
                std::vector<float> buf(mesh->normals.size() * 3);
                for (size_t i = 0; i < mesh->normals.size(); i++)
                {
                    buf[3 * i + 0] = (float)mesh->normals[i][0];
                    buf[3 * i + 1] = (float)mesh->normals[i][1];
                    buf[3 * i + 2] = (float)mesh->normals[i][2];
                }
                for (size_t i = 0; i < mesh->normals.size(); i++)
                {
                    float x = buf[3 * i + 0];
                    float y = buf[3 * i + 1];
                    float z = buf[3 * i + 2];
                    float l = (x*x + y * y + z * z);
                    if (fabs(l) > 1e-6f)
                    {
                        l = 1.0f / std::sqrt(l);
                        buf[3 * i + 0] = x * l;
                        buf[3 * i + 1] = y * l;
                        buf[3 * i + 2] = z * l;
                    }
                }
                int attId = CreateDracoBuffer(*dracoMesh, draco::GeometryAttribute::Type::NORMAL, 3, true, buf);
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
	bool SaveToDraco(std::vector<unsigned char>& bytes, const std::shared_ptr<Mesh>& m)
	{
		// Disable ENABLE_BUILD_WITH_DRACO option
		return false;
	}
#endif
}

