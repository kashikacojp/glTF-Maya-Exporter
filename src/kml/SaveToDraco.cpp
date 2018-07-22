#include "SaveToDraco.h"

#include <glm/glm.hpp>

#ifdef ENABLE_BUILD_WITH_DRACO
#include <draco/mesh/mesh.h>
#include <draco/compression/encode.h>
#include <draco/core/cycle_timer.h>
#include <draco/io/mesh_io.h>
#include <draco/io/point_cloud_io.h>
#include <draco/io/obj_decoder.h>
#endif

#include <fstream>
#include <sstream>

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
			compression_level(7) {}

	}

	static
	int IdxAdjust(int x)
	{
		if (x >= 0)
		{
			return x + 1;
		}
		else
		{
			return x;
		}
	}

	static
	void WriteToStream(std::ostream& os, const std::shared_ptr<Mesh>& mesh)
	{
		assert(mesh->positions.size() > 0);
		if (mesh->positions.size()  > 0)
		{
			for (int j = 0; j < mesh->positions.size(); j++)
			{
				os << "v " << mesh->positions[j][0] << " " << mesh->positions[j][1] << " " << mesh->positions[j][2] << "\n";
			}
		}

		if (mesh->texcoords.size()  > 0)
		{
			for (int j = 0; j < mesh->texcoords.size(); j++)
			{
				os << "vt " << mesh->texcoords[j][0] << " " << mesh->texcoords[j][1] << "\n";
			}
		}

		if (mesh->normals.size()  > 0)
		{
			for (int j = 0; j < mesh->normals.size(); j++)
			{
				glm::vec3 n = glm::normalize(mesh->normals[j]);
				os << "vn " << n[0] << " " << n[1] << " " << n[2] << "\n";
			}
		}

		os << "\n";

		int nFacePattern = 0;
		if (mesh->positions.size() > 0) nFacePattern |= 1;
		if (mesh->texcoords.size() > 0) nFacePattern |= 2;
		if (mesh->normals.size()   > 0) nFacePattern |= 4;

		switch (nFacePattern)
		{
			case 1:
			{
				size_t offset = 0;
				for (size_t i = 0; i < mesh->facenums.size(); i++)
				{
					os << "f ";
					int fsz = mesh->facenums[i];
					for (int j = 0; j < fsz; j++)
					{
						int pidx = mesh->pos_indices[offset + j];
						os << IdxAdjust(pidx);
						if (j != fsz - 1)
						{
							os << " ";
						}
						else
						{
							os << "\n";
						}
					}
					offset += fsz;
				}
			}
			break;
			case 3: // 1 | 2
			{//p/t
				size_t offset = 0;
				for (size_t i = 0; i < mesh->facenums.size(); i++)
				{
					os << "f ";
					int fsz = mesh->facenums[i];
					for (int j = 0; j < fsz; j++)
					{
						int pidx = mesh->pos_indices[offset + j];
						int tidx = mesh->tex_indices[offset + j];
						os << IdxAdjust(pidx) << "/" << IdxAdjust(tidx);
						if (j != fsz - 1)
						{
							os << " ";
						}
						else
						{
							os << "\n";
						}
					}
					offset += fsz;
				}
			}
			break;
			case 5:
			{//p//n
				size_t offset = 0;
				for (size_t i = 0; i < mesh->facenums.size(); i++)
				{
					os << "f ";
					int fsz = mesh->facenums[i];
					for (int j = 0; j < fsz; j++)
					{
						int pidx = mesh->pos_indices[offset + j];
						int nidx = mesh->nor_indices[offset + j];
						os << IdxAdjust(pidx) << "//" << IdxAdjust(nidx);
						if (j != fsz - 1)
						{
							os << " ";
						}
						else
						{
							os << "\n";
						}
					}
					offset += fsz;
				}
			}
			break;
			case 7:
			{
				size_t offset = 0;
				for (size_t i = 0; i < mesh->facenums.size(); i++)
				{
					os << "f ";
					int fsz = mesh->facenums[i];
					for (int j = 0; j < fsz; j++)
					{
						int pidx = mesh->pos_indices[offset + j];
						int tidx = mesh->tex_indices[offset + j];
						int nidx = mesh->nor_indices[offset + j];
						os << IdxAdjust(pidx) << "/" << IdxAdjust(tidx) << "/" << IdxAdjust(nidx);
						if (j != fsz - 1)
						{
							os << " ";
						}
						else
						{
							os << "\n";
						}
					}
					offset += fsz;
				}
			}
			break;
		}
	}

#ifdef ENABLE_BUILD_WITH_DRACO
	static
	int EncodeMeshToFile(std::ostream& os, const draco::Mesh &mesh, draco::Encoder *encoder)
	{
		draco::EncoderBuffer buffer;
		const draco::Status status = encoder->EncodeMeshToBuffer(mesh, &buffer);
		if (!status.ok()) 
		{
			return -1;
		}
		os.write(buffer.data(), buffer.size());
		return 0;
	}

	static
	void WriteDebug(std::string& s)
	{
		std::ofstream ofs("c:/src/Debug/debug.obj");
		if (ofs)
		{
			ofs << s << std::endl;
		}
	}

	static
	int EncodeMeshToFile(std::vector<unsigned char>& bytes, const draco::Mesh &mesh, draco::Encoder *encoder)
	{
		draco::EncoderBuffer buffer;
		const draco::Status status = encoder->EncodeMeshToBuffer(mesh, &buffer);
		if (!status.ok())
		{
			return -1;
		}
		std::copy(buffer.data(), buffer.data() + buffer.size(), back_inserter(bytes));
		return 0;
	}

	bool SaveToDraco(std::vector<unsigned char>& bytes, const std::shared_ptr<Mesh>& m)
	{
		using namespace draco;
		//------------------------------------
		std::unique_ptr<draco::Mesh> mesh(new draco::Mesh());

		std::stringstream ss;
		WriteToStream(ss, m);
		std::string s = ss.str();
		//WriteDebug(s);
		//return false;

		draco::DecoderBuffer buffer;
		buffer.Init(s.c_str(), s.size());
		draco::ObjDecoder obj_decoder;
		if (!obj_decoder.DecodeFromBuffer(&buffer, mesh.get()).ok())
		{
			return false;
		}
		
		ns::Options options;

		// Convert compression level to speed (that 0 = slowest, 10 = fastest).
		const int speed = 10 - options.compression_level;

		// Setup encoder options.
		draco::Encoder encoder;
		if (options.pos_quantization_bits > 0) {
			encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION,
				options.pos_quantization_bits);
		}
		if (options.tex_coords_quantization_bits > 0) {
			encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD,
				options.tex_coords_quantization_bits);
		}
		if (options.normals_quantization_bits > 0) {
			encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL,
				options.normals_quantization_bits);
		}
		encoder.SetSpeedOptions(speed, speed);

		int ret = EncodeMeshToFile(bytes, *mesh, &encoder);

		if (ret == -1)
		{
			printf("\nFailed to EncodedMeshToFile\n\n");
			return false;
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

