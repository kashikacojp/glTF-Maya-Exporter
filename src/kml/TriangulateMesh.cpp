#include "TriangulateMesh.h"

namespace kml
{
    static bool HasQuadFace(const std::shared_ptr<Mesh>& mesh)
    {
        for (size_t i = 0; i < mesh->facenums.size(); i++)
        {
            if (mesh->facenums[i] >= 4)
            {
                return true;
            }
        }
        return false;
    }

    bool TriangulateMesh(std::shared_ptr<Mesh>& mesh)
    {
        if (!HasQuadFace(mesh))
        {
            return true;
        }
        std::vector<int> pos_indices;
        std::vector<int> nor_indices;
        std::vector<int> tex_indices;
        std::vector<unsigned char> facenums;
        std::vector<int> materials;
        size_t offset = 0;
        for (size_t i = 0; i < mesh->facenums.size(); i++)
        {
            int nf = mesh->facenums[i];
            if (nf == 3)
            {
                facenums.push_back(nf);
                for (int j = 0; j < nf; j++)
                {
                    pos_indices.push_back(mesh->pos_indices[offset + j]);
                    nor_indices.push_back(mesh->nor_indices[offset + j]);
                    tex_indices.push_back(mesh->tex_indices[offset + j]);
                }
                materials.push_back(mesh->materials[i]);
            }
            else if (nf == 4)
            {
                //assert(nf == 4);
                facenums.push_back(3);
                facenums.push_back(3);

                //
                pos_indices.push_back(mesh->pos_indices[offset + 0]);
                pos_indices.push_back(mesh->pos_indices[offset + 1]);
                pos_indices.push_back(mesh->pos_indices[offset + 3]);

                pos_indices.push_back(mesh->pos_indices[offset + 1]);
                pos_indices.push_back(mesh->pos_indices[offset + 2]);
                pos_indices.push_back(mesh->pos_indices[offset + 3]);

                //
                nor_indices.push_back(mesh->nor_indices[offset + 0]);
                nor_indices.push_back(mesh->nor_indices[offset + 1]);
                nor_indices.push_back(mesh->nor_indices[offset + 3]);

                nor_indices.push_back(mesh->nor_indices[offset + 1]);
                nor_indices.push_back(mesh->nor_indices[offset + 2]);
                nor_indices.push_back(mesh->nor_indices[offset + 3]);

                //
                tex_indices.push_back(mesh->tex_indices[offset + 0]);
                tex_indices.push_back(mesh->tex_indices[offset + 1]);
                tex_indices.push_back(mesh->tex_indices[offset + 3]);

                tex_indices.push_back(mesh->tex_indices[offset + 1]);
                tex_indices.push_back(mesh->tex_indices[offset + 2]);
                tex_indices.push_back(mesh->tex_indices[offset + 3]);

                materials.push_back(mesh->materials[i]);
                materials.push_back(mesh->materials[i]);
            }
            else
            {
                int nn = nf - 2;
                for (int j = 0; j < nn; j++)
                {
                    facenums.push_back(3);
                    materials.push_back(mesh->materials[i]);
                    pos_indices.push_back(mesh->pos_indices[offset + 0]);
                    pos_indices.push_back(mesh->pos_indices[offset + j + 1]);
                    pos_indices.push_back(mesh->pos_indices[offset + j + 2]);

                    nor_indices.push_back(mesh->nor_indices[offset + 0]);
                    nor_indices.push_back(mesh->nor_indices[offset + j + 1]);
                    nor_indices.push_back(mesh->nor_indices[offset + j + 2]);

                    tex_indices.push_back(mesh->tex_indices[offset + 0]);
                    tex_indices.push_back(mesh->tex_indices[offset + j + 1]);
                    tex_indices.push_back(mesh->tex_indices[offset + j + 2]);
                }
            }
            offset += nf;
        }
        mesh->facenums.swap(facenums);
        mesh->pos_indices.swap(pos_indices);
        mesh->nor_indices.swap(nor_indices);
        mesh->tex_indices.swap(tex_indices);
        mesh->materials.swap(materials);

        return true;
    }
} // namespace kml