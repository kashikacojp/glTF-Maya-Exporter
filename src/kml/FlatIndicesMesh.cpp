#include "FlatIndicesMesh.h"
#include "CalculateNormalsMesh.h"
#include "Skin.h"
#include <glm/glm.hpp> // vec3 normalize cross
#include <map>
#include <vector>

namespace kml
{
    static bool FixToReferenced(std::shared_ptr<Mesh>& mesh)
    {
        {
            size_t vsz = mesh->positions.size();
            std::vector<int> counts(vsz);
            for (size_t i = 0; i < vsz; i++)
            {
                counts[i] = 0;
            }

            for (size_t i = 0; i < mesh->pos_indices.size(); i++)
            {
                int idx = mesh->pos_indices[i];
                if (idx >= 0)
                {
                    counts[idx]++;
                }
            }

            for (size_t i = 0; i < vsz; i++)
            {
                if (counts[i])
                {
                    counts[i] = 1;
                }
            }
            std::vector<glm::vec3> positions;
            std::vector<int> new_indices(vsz);

            std::vector<kml::SkinWeight::WeightVertex> weight_vertices;

            size_t offset = 0;
            for (size_t i = 0; i < vsz; i++)
            {
                if (counts[i])
                {
                    positions.push_back(mesh->positions[i]);
                    if (mesh->skin_weight.get())
                    {
                        weight_vertices.push_back(mesh->skin_weight->weights[i]);
                    }
                }
                new_indices[i] = offset;
                offset += counts[i];
            }
            mesh->positions.swap(positions);
            if (mesh->skin_weight.get())
            {
                mesh->skin_weight->weights = weight_vertices;
            }
            for (size_t i = 0; i < mesh->pos_indices.size(); i++)
            {
                int idx = mesh->pos_indices[i];
                if (idx >= 0)
                {
                    mesh->pos_indices[i] = new_indices[idx];
                }
            }
        }

        if (mesh->tex_indices.size())
        {
            bool bAll = true;
            for (size_t i = 0; i < mesh->tex_indices.size(); i++)
            {
                if (mesh->tex_indices[i] >= 0)
                {
                    bAll = false;
                    break;
                }
            }
            if (bAll)
            {
                mesh->texcoords.clear();
            }
        }

        if (mesh->texcoords.size() == 0)
        {
            mesh->tex_indices.resize(mesh->pos_indices.size());
            for (size_t i = 0; i < mesh->tex_indices.size(); i++)
            {
                mesh->tex_indices[i] = -1;
            }
        }
        else
        {
            size_t vsz = mesh->texcoords.size();
            if (vsz)
            {
                std::vector<int> counts(vsz);
                for (size_t i = 0; i < vsz; i++)
                {
                    counts[i] = 0;
                }

                for (size_t i = 0; i < mesh->tex_indices.size(); i++)
                {
                    int idx = mesh->tex_indices[i];
                    if (idx >= 0)
                    {
                        counts[idx]++;
                    }
                }

                for (size_t i = 0; i < vsz; i++)
                {
                    if (counts[i])
                    {
                        counts[i] = 1;
                    }
                }
                std::vector<glm::vec2> texcoords;
                std::vector<int> new_indices(vsz);
                size_t offset = 0;
                for (size_t i = 0; i < vsz; i++)
                {
                    if (counts[i])
                    {
                        texcoords.push_back(mesh->texcoords[i]);
                    }
                    new_indices[i] = offset;
                    offset += counts[i];
                }
                mesh->texcoords.swap(texcoords);
                for (size_t i = 0; i < mesh->tex_indices.size(); i++)
                {
                    int idx = mesh->tex_indices[i];
                    if (idx >= 0)
                    {
                        mesh->tex_indices[i] = new_indices[idx];
                    }
                }
            }
        }

        {
            if (mesh->normals.size() == 0)
            {
                mesh->nor_indices.resize(mesh->pos_indices.size());
                for (size_t i = 0; i < mesh->nor_indices.size(); i++)
                {
                    mesh->nor_indices[i] = -1;
                }
            }
            else
            {
                for (size_t i = 0; i < mesh->normals.size(); i++)
                {
                    if (glm::length(mesh->normals[i]) > 1e-6f)
                    {
                        mesh->normals[i] = glm::normalize(mesh->normals[i]);
                    }
                    else
                    {
                        mesh->normals[i] = glm::vec3(0, 1, 0);
                    }
                }
            }
        }
        {
            size_t vsz = mesh->normals.size();
            if (vsz)
            {
                std::vector<int> counts(vsz);
                for (size_t i = 0; i < vsz; i++)
                {
                    counts[i] = 0;
                }

                for (size_t i = 0; i < mesh->nor_indices.size(); i++)
                {
                    int idx = mesh->nor_indices[i];
                    if (idx >= 0)
                    {
                        counts[idx]++;
                    }
                }

                for (size_t i = 0; i < vsz; i++)
                {
                    if (counts[i])
                    {
                        counts[i] = 1;
                    }
                }
                std::vector<glm::vec3> normals;
                std::vector<int> new_indices(vsz);
                size_t offset = 0;
                for (size_t i = 0; i < vsz; i++)
                {
                    if (counts[i])
                    {
                        normals.push_back(mesh->normals[i]);
                    }
                    new_indices[i] = offset;
                    offset += counts[i];
                }
                mesh->normals.swap(normals);
                for (size_t i = 0; i < mesh->nor_indices.size(); i++)
                {
                    int idx = mesh->nor_indices[i];
                    if (idx >= 0)
                    {
                        mesh->nor_indices[i] = new_indices[idx];
                    }
                }
            }
        }
        return true;
    }

    static bool NeedRecalcNormals(const std::shared_ptr<Mesh>& mesh)
    {
        if (mesh->normals.size() == 0)
        {
            return true;
        }
        //if (mesh->normals.size() != mesh->positions.size())
        //{
        //	return true;
        //}
        for (size_t i = 0; i < mesh->nor_indices.size(); i++)
        {
            int idx = mesh->nor_indices[i];
            if (idx < 0)
            {
                return true;
            }
        }
        return false;
    }

    static bool CheckNormals(std::shared_ptr<Mesh>& mesh)
    {
        if (NeedRecalcNormals(mesh))
        {
            mesh->normals.clear();
            return CalculateNormalsMesh(mesh);
        }
        return true;
    }

    namespace
    {
        struct FaceIndices
        {
            FaceIndices() {}
            FaceIndices(int a, int b, int c)
            {
                indices[0] = a;
                indices[1] = b;
                indices[2] = c;
            }
            int indices[3];
        };

        bool operator<(const FaceIndices& a, const FaceIndices& b)
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

        bool operator==(const FaceIndices& a, const FaceIndices& b)
        {
            return memcmp(&a, &b, sizeof(FaceIndices)) == 0;
        }
    } // namespace

    static bool FixIndices(std::shared_ptr<Mesh>& mesh)
    {
#if 0
		typedef std::pair<int, int> IndexPair;
		typedef std::map<IndexPair, int> IndexMap;

		IndexMap imap;
		for (size_t i = 0; i < mesh->tex_indices.size(); i++)
		{
			int vidx = mesh->pos_indices[i];
			int tidx = mesh->tex_indices[i];
			IndexPair pair = std::make_pair(vidx, tidx);
			IndexMap::iterator it = imap.find(pair);
			if (it == imap.end())
			{
				int sz = imap.size();
				imap.insert(IndexMap::value_type(pair, sz));
			}
		}
		{
			int ii = 0;
			for (IndexMap::iterator it = imap.begin(); it != imap.end(); it++)
			{
				it->second = ii;
				ii++;
			}
		}

		size_t sz = imap.size();
		std::vector<int> indices(mesh->tex_indices.size());
		std::vector<glm::vec3> positions(sz);
		std::vector<glm::vec2> texcoords(sz);
		std::vector<glm::vec3> normals(sz);

		for (size_t i = 0; i < mesh->tex_indices.size(); i++)
		{
			int vidx = mesh->pos_indices[i];
			int tidx = mesh->tex_indices[i];
			int nidx = mesh->nor_indices[i];
			IndexPair pair = std::make_pair(vidx, tidx);
			IndexMap::iterator it = imap.find(pair);
			int index = it->second;
			positions[index] = mesh->positions[vidx];
			if (tidx >= 0)
			{
				texcoords[index] = mesh->texcoords[tidx];
			}
			else
			{
				texcoords[index] = glm::vec2(0, 0);
			}
			
			if (nidx >= 0)
			{
				normals[index] = mesh->normals[nidx];
			}
			else
			{
				assert(0);
				normals[index] = glm::vec3(0, 1, 0);
			}

			indices[i] = index;
		}
#else
        typedef FaceIndices IndexPair;
        typedef std::map<IndexPair, int> IndexMap;

        IndexMap imap;
        for (size_t i = 0; i < mesh->tex_indices.size(); i++)
        {
            int vidx = mesh->pos_indices[i];
            int tidx = mesh->tex_indices[i];
            int nidx = mesh->nor_indices[i];
            IndexPair pair = IndexPair(vidx, tidx, nidx);
            IndexMap::iterator it = imap.find(pair);
            if (it == imap.end())
            {
                int sz = imap.size();
                imap.insert(IndexMap::value_type(pair, sz));
            }
        }
        {
            int ii = 0;
            for (IndexMap::iterator it = imap.begin(); it != imap.end(); it++)
            {
                it->second = ii;
                ii++;
            }
        }

        size_t sz = imap.size();
        std::vector<int> indices(mesh->tex_indices.size());
        std::vector<glm::vec3> positions(sz);
        std::vector<glm::vec2> texcoords(sz);
        std::vector<glm::vec3> normals(sz);

        std::vector<SkinWeight::WeightVertex> weight_vertices(sz);

        std::vector<std::shared_ptr<MorphTarget> > morph_targets;
        if (mesh->morph_targets.get())
        {
            for (size_t j = 0; j < mesh->morph_targets->targets.size(); j++)
            {
                std::shared_ptr<MorphTarget> target(new MorphTarget());
                target->positions.resize(sz);
                target->normals.resize(sz);
                morph_targets.push_back(target);
            }
        }

        for (size_t i = 0; i < mesh->tex_indices.size(); i++)
        {
            int vidx = mesh->pos_indices[i];
            int tidx = mesh->tex_indices[i];
            int nidx = mesh->nor_indices[i];
            IndexPair pair = IndexPair(vidx, tidx, nidx);
            IndexMap::iterator it = imap.find(pair);
            int index = it->second;
            positions[index] = mesh->positions[vidx];
            if (mesh->skin_weight.get())
            {
                weight_vertices[index] = mesh->skin_weight->weights[vidx];
            }
            {
                for (size_t j = 0; j < morph_targets.size(); j++)
                {
                    morph_targets[j]->positions[index] = mesh->morph_targets->targets[j]->positions[vidx];

                    if (nidx >= 0)
                    {
                        morph_targets[j]->normals[index] = mesh->morph_targets->targets[j]->normals[nidx];
                    }
                    else
                    {
                        assert(0);
                        morph_targets[j]->normals[index] = glm::vec3(0, 1, 0);
                    }
                }
            }

            if (tidx >= 0)
            {
                texcoords[index] = mesh->texcoords[tidx];
            }
            else
            {
                texcoords[index] = glm::vec2(0, 0);
            }

            if (nidx >= 0)
            {
                normals[index] = mesh->normals[nidx];
            }
            else
            {
                assert(0);
                normals[index] = glm::vec3(0, 1, 0);
            }

            indices[i] = index;
        }
#endif
        mesh->pos_indices = indices;
        mesh->nor_indices = indices;
        mesh->tex_indices = indices;
        mesh->positions.swap(positions);
        mesh->texcoords.swap(texcoords);
        mesh->normals.swap(normals);

        if (mesh->skin_weight.get())
        {
            mesh->skin_weight->weights.swap(weight_vertices);
        }

        if (mesh->morph_targets.get())
        {
            mesh->morph_targets->targets = morph_targets;
        }

        return true;
    }

    static bool ShortenNormal(std::shared_ptr<Mesh>& mesh)
    {
        for (size_t i = 0; i < mesh->normals.size(); i++)
        {
            mesh->normals[i] = 0.95f * glm::normalize(mesh->normals[i]);
        }
        return true;
    }

    bool FlatIndicesMesh(std::shared_ptr<Mesh>& mesh)
    {
        if (!FixToReferenced(mesh))
        {
            return false;
        }

        if (!CheckNormals(mesh))
        {
            return false;
        }

        if (!FixIndices(mesh))
        {
            return false;
        }

        if (!ShortenNormal(mesh))
        {
            return false;
        }

        return true;
    }
} // namespace kml