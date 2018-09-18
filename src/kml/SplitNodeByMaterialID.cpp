#define _CRT_SECURE_NO_WARNINGS 1

#include "SplitNodeByMaterialID.h"
#include "CalculateBound.h"

#include <algorithm>

namespace kml
{
	std::vector< std::shared_ptr<kml::Node> > SplitNodeByFaceNormal(std::shared_ptr<kml::Node>& node)
	{
		std::vector< std::shared_ptr<kml::Node> > nodes;
		auto& mesh = node->GetMesh();

		std::vector<glm::vec3> nf(mesh->facenums.size());
		{
			int offset = 0;
			for (size_t i = 0; i < nf.size(); i++)
			{
				int facenum = mesh->facenums[i];
				int i0 = mesh->pos_indices[offset + 0];
				int i1 = mesh->pos_indices[offset + 1];
				int i2 = mesh->pos_indices[offset + 2];
				glm::vec3 p0 = mesh->positions[i0];
				glm::vec3 p1 = mesh->positions[i1];
				glm::vec3 p2 = mesh->positions[i2];

				glm::vec3 e1 = p1 - p0;
				glm::vec3 e2 = p2 - p0;

				glm::vec3 nn = glm::cross(e1, e2);
				float len = glm::length(nn);
				assert(1e-5f < len);
				nn = glm::normalize(nn);
				nf[i] = nn;
				offset += facenum;
			}
		}

		std::vector<int> abnormal_faces;
		{
			//static const float E = std::cos(glm::radians(35.0));
			int offset = 0;
			for (size_t i = 0; i < nf.size(); i++)
			{
				int facenum = mesh->facenums[i];
				for (size_t j = 0; j < facenum; j++)
				{
					int i0 = mesh->nor_indices[offset + j];
					glm::vec3 n = mesh->normals[i0];
					float d = glm::dot(nf[i], n);
					if (d < 0)
					{
						abnormal_faces.push_back(i);
					}
				}
				offset += facenum;
			}
		}

		if (abnormal_faces.empty())
		{
			nodes.push_back(node);
		}
		else
		{
			//assert(0);
			nodes.push_back(node);
		}



		return nodes;
	}

	namespace
	{
		struct FaceIndices
		{
			int self_index;
			int indices[3];
		};

		struct FaceSorter
		{
			bool operator()(const FaceIndices& a, const FaceIndices& b)const
			{
				if (a.indices[0] == b.indices[0])
				{
					if (a.indices[1] == b.indices[1])
					{
						return a.indices[2] < b.indices[2];
					}
					else
					{
						return a.indices[1] < b.indices[1];
					}
				}
				else
				{
					return a.indices[0] < b.indices[0];
				}
			}
		};

		bool operator==(const FaceIndices& a, const FaceIndices& b)
		{
			return memcmp(&a, &b, sizeof(FaceIndices)) == 0;
		}
	}

	static
	void SortIndex(FaceIndices& fInd)
	{
		int* a = fInd.indices;
		int* b = a + 3;
		std::sort(a, b);
	}


	static
	void RotateIndex(FaceIndices& fInd)
	{
		int min_ = 0;
		if (fInd.indices[1] < fInd.indices[min_])min_ = 1;
		if (fInd.indices[2] < fInd.indices[min_])min_ = 2;
		int indices[3];
		for (int i = 0; i < 3; i++)
		{
			indices[i] = fInd.indices[(min_ + i) % 3];
		}
		for (int i = 0; i < 3; i++)
		{
			fInd.indices[i] = indices[i];
		}
	}
	
	static
	bool HasDoubleSidedFace(const std::shared_ptr<Mesh>& mesh)
	{
		std::vector<FaceIndices> faces;
		faces.reserve(mesh->facenums.size());
		int offset = 0;
		for (int i = 0; i < mesh->facenums.size(); i++)
		{
			int facenum = mesh->facenums[i];
			FaceIndices fInd;
			for (int j = 0; j < 3; j++)
			{
				fInd.indices[j] = mesh->pos_indices[offset + j];
			}
			fInd.self_index = 0;//
			faces.push_back(fInd);
			offset += facenum;
		}
		for (int i = 0; i < faces.size(); i++)
		{
			SortIndex(faces[i]);
		}
		std::sort(faces.begin(), faces.end(), FaceSorter());
		for (int i = 1; i < faces.size(); i++)
		{
			if (faces[i - 1] == faces[i])
			{
				return true;
			}
		}

		return false;
	}

	std::vector< std::shared_ptr<kml::Node> > SplitNodeByFaceOrientation(std::shared_ptr<kml::Node>& node)
	{
		std::vector< std::shared_ptr<kml::Node> > nodes;
		auto& mesh = node->GetMesh();
		if (HasDoubleSidedFace(mesh))
		{
			nodes.push_back(node);
		}
		else
		{
			nodes.push_back(node);
		}

		return nodes;
	}


	std::vector< std::shared_ptr<kml::Node> > SplitNodeByMaterialID(std::shared_ptr<kml::Node>& node)
	{
		std::vector< std::shared_ptr<kml::Node> > nodes;
		auto& mesh = node->GetMesh();
		auto& materials = node->GetMaterials();
		if (materials.size() == 1)
		{
			nodes.push_back(node);
		}
		else
		{
			std::vector< std::shared_ptr<kml::Mesh> > meshes;
			for (size_t i = 0; i < materials.size(); i++)
			{
				meshes.push_back( std::shared_ptr<kml::Mesh>(new kml::Mesh()) );
				if (mesh->skin_weights.get())
				{
					meshes[i]->skin_weights = std::shared_ptr<kml::SkinWeights>( new kml::SkinWeights() );
				}
			}

			int offset = 0;
			for (size_t i = 0; i < mesh->materials.size(); i++)
			{
				int matID = mesh->materials[i];
				int facenum = mesh->facenums[i];
				if (0 <= matID && matID < meshes.size())
				{
					meshes[matID]->facenums.push_back(facenum);
					meshes[matID]->materials.push_back(0);
					for (int j = 0; j < facenum; j++)
					{
						meshes[matID]->pos_indices.push_back(mesh->pos_indices[offset + j]);
						meshes[matID]->tex_indices.push_back(mesh->tex_indices[offset + j]);
						meshes[matID]->nor_indices.push_back(mesh->nor_indices[offset + j]);
					}
				}

				offset += facenum;
			}
			for (int i = 0; i < materials.size(); i++)
			{
				if (meshes[i]->facenums.empty())
				{
					continue;
				}
				meshes[i]->positions = mesh->positions;
				meshes[i]->texcoords = mesh->texcoords;
				meshes[i]->normals   = mesh->normals;
				meshes[i]->name = mesh->name;

				if (mesh->skin_weights.get())
				{
					meshes[i]->skin_weights->joint_paths = mesh->skin_weights->joint_paths;
					meshes[i]->skin_weights->weights = mesh->skin_weights->weights;
				}

				std::shared_ptr<kml::Node> tnode = std::shared_ptr<kml::Node>(new kml::Node());
				tnode->SetMesh(meshes[i]);
				tnode->AddMaterial(materials[i]);

				int k = (int)nodes.size();
				char buffer[32] = {};
				sprintf(buffer, "%d", k + 1);
				std::string number = buffer;

				
				tnode->SetName(node->GetName() + "_" + number); //TODO:
				tnode->SetPath(node->GetPath() + "_" + number);
				tnode->GetTransform()->SetMatrix(node->GetTransform()->GetMatrix());
				tnode->SetBound(kml::CalculateBound(mesh));

				nodes.push_back(tnode);
			}
		}

		return nodes;
	}
}