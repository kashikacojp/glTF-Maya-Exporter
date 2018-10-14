#pragma once
#ifndef _KML_MESH_H_
#define _KML_MESH_H_

#include "Compatibility.h"
#include "MorphTargets.h"
#include "Skin.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <string>
#include <vector>

namespace kml
{
    class Mesh
    {
    public:
        Mesh();
        std::shared_ptr<SkinWeight> GetSkinWeight() const
        {
            return skin_weight;
        }
    public:
        std::string name;
        std::vector<unsigned char> facenums;
        std::vector<int> pos_indices;
        std::vector<int> nor_indices;
        std::vector<int> tex_indices;
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texcoords;
        std::vector<int> materials;
        std::shared_ptr<SkinWeight> skin_weight;
        std::shared_ptr<MorphTargets> morph_targets;
    };
} // namespace kml

#endif
