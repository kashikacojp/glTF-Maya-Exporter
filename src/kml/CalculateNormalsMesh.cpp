#include "CalculateNormalsMesh.h"

#include <glm/glm.hpp> // vec3 normalize cross

namespace kml
{
    bool RemoveNoAreaMesh(std::shared_ptr<Mesh>& mesh)
    {
        std::vector<unsigned char> facenums;
        std::vector<int> pos_indices;
        std::vector<int> nor_indices;
        std::vector<int> tex_indices;
        std::vector<int> materials;
        {
            int offset = 0;
            for (size_t i = 0; i < mesh->facenums.size(); i++)
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
                if (len <= 1e-4f)
                {
                    ; //
                }
                else
                {
                    facenums.push_back(mesh->facenums[i]);
                    materials.push_back(mesh->materials[i]);
                    for (int j = 0; j < facenum; j++)
                    {
                        pos_indices.push_back(mesh->pos_indices[offset + j]);
                        nor_indices.push_back(mesh->nor_indices[offset + j]);
                        tex_indices.push_back(mesh->tex_indices[offset + j]);
                    }
                }
                offset += facenum;
            }
        }
        mesh->facenums.swap(facenums);
        mesh->materials.swap(materials);
        mesh->pos_indices.swap(pos_indices);
        mesh->nor_indices.swap(nor_indices);
        mesh->tex_indices.swap(tex_indices);

        return true;
    }

    static bool NeedRecalc(const std::shared_ptr<Mesh>& mesh)
    {
        if (mesh->normals.size() == 0)
        {
            return true;
        }
        return false;
    }

    static bool IsFaceVaryingNormal(const std::shared_ptr<Mesh>& mesh, float E)
    {
        if (mesh->normals.size() != 0)
        {
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

            {
                int offset = 0;
                for (size_t i = 0; i < nf.size(); i++)
                {
                    int facenum = mesh->facenums[i];
                    for (size_t j = 0; j < facenum; j++)
                    {
                        int i0 = mesh->nor_indices[offset + j];
                        glm::vec3 n = mesh->normals[i0];
                        float d = glm::dot(nf[i], n);
                        if (d < E)
                        {
                            return true;
                        }
                    }
                    offset += facenum;
                }
            }
            return false;
        }
        else
        {
            return true;
        }
        return false;
    }

    static bool CalculateNormalsMeshFaceVaring(std::shared_ptr<Mesh>& mesh, float E)
    {
        std::vector<glm::vec3> nf(mesh->facenums.size());
        for (size_t i = 0; i < nf.size(); i++)
        {
            nf[i] = glm::vec3(0, 0, 0);
        }
        std::vector<glm::vec3> nv(mesh->positions.size());
        for (size_t i = 0; i < nv.size(); i++)
        {
            nv[i] = glm::vec3(0, 0, 0);
        }

        {
            size_t offset = 0;
            for (size_t i = 0; i < mesh->facenums.size(); i++)
            {
                int fsz = mesh->facenums[i];
                {
                    int i0 = mesh->pos_indices[offset + 0];
                    int i1 = mesh->pos_indices[offset + 1];
                    int i2 = mesh->pos_indices[offset + 2];
                    glm::vec3 p0 = mesh->positions[i0];
                    glm::vec3 p1 = mesh->positions[i1];
                    glm::vec3 p2 = mesh->positions[i2];
                    glm::vec3 e1 = p1 - p0;
                    glm::vec3 e2 = p2 - p0;
                    glm::vec3 n = glm::cross(e1, e2);
                    nf[i] = glm::normalize(n);

                    nv[i0] += n;
                    nv[i1] += n;
                    nv[i2] += n;
                }
                offset += fsz;
            }
        }

        for (size_t i = 0; i < nv.size(); i++)
        {
            if (glm::length(nv[i]) > 1e-5f)
            {
                nv[i] = glm::normalize(nv[i]);
            }
            else
            {
                nv[i] = glm::vec3(0, 0, 0);
            }
        }

        std::vector<glm::vec3> normals(mesh->pos_indices.size());
        std::vector<int> nor_indices = mesh->pos_indices;

        bool bFaceVarying = false;
        {
            size_t offset = 0;
            for (size_t i = 0; i < mesh->facenums.size(); i++)
            {
                int fsz = mesh->facenums[i];
                for (int j = 0; j < fsz; j++)
                {
                    int i0 = mesh->pos_indices[offset + j];
                    float d = glm::dot(nf[i], nv[i0]);
                    if (d < E)
                    {
                        normals[offset + j] = nf[i];
                        bFaceVarying = true;
                    }
                    else
                    {
                        normals[offset + j] = nv[i0];
                    }

                    nor_indices[offset + j] = offset + j;
                }
                offset += fsz;
            }
        }

        if (bFaceVarying)
        {
            mesh->normals.swap(normals);
            mesh->nor_indices.swap(nor_indices);
        }
        else
        {
            nor_indices = mesh->pos_indices;
            mesh->normals.swap(nv);
            mesh->nor_indices.swap(nor_indices);
        }

        return true;
    }

    static bool CalculateNormalsMeshVertexVaring(std::shared_ptr<Mesh>& mesh)
    {
        if (!NeedRecalc(mesh))
        {
            return true;
        }

        std::vector<glm::vec3> normals(mesh->positions.size());
        for (size_t i = 0; i < normals.size(); i++)
        {
            normals[i] = glm::vec3(0, 0, 0);
        }

        size_t offset = 0;
        for (size_t i = 0; i < mesh->facenums.size(); i++)
        {
            int fsz = mesh->facenums[i];
            {
                int i0 = mesh->pos_indices[offset + 0];
                int i1 = mesh->pos_indices[offset + 1];
                int i2 = mesh->pos_indices[offset + 2];
                glm::vec3 p0 = mesh->positions[i0];
                glm::vec3 p1 = mesh->positions[i1];
                glm::vec3 p2 = mesh->positions[i2];
                glm::vec3 e1 = p1 - p0;
                glm::vec3 e2 = p2 - p0;
                glm::vec3 n = glm::cross(e1, e2);

                normals[i0] += n;
                normals[i1] += n;
                normals[i2] += n;
            }
            offset += fsz;
        }

        for (size_t i = 0; i < normals.size(); i++)
        {
            if (glm::length(normals[i]) > 1e-6f)
            {
                normals[i] = glm::normalize(normals[i]);
            }
            else
            {
                normals[i] = glm::vec3(0, 1, 0);
            }
        }

        std::vector<int> nor_indices = mesh->pos_indices;

        mesh->normals.swap(normals);
        mesh->nor_indices.swap(nor_indices);

        return true;
    }

    bool CalculateNormalsMesh(std::shared_ptr<Mesh>& mesh)
    {
        //static const float E = std::cos(glm::radians(30.0f));

        //if (IsFaceVaryingNormal(mesh, E))
        //{
        //	return CalculateNormalsMeshFaceVaring(mesh, E);
        //}
        //else
        {
            return CalculateNormalsMeshVertexVaring(mesh);
        }
    }
} // namespace kml