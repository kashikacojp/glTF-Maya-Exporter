#pragma once
#ifndef _KML_MORPHTARGETS_H_
#define _KML_MORPHTARGETS_H_

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <memory>
#include <string>
#include <vector>

namespace kml
{
    class MorphTarget
    {
    public:
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
    };

    class MorphTargets
    {
    public:
        std::vector<std::shared_ptr<MorphTarget> > targets;
        std::vector<float> weights;
        std::vector<std::string> names;
    };
} // namespace kml

#endif
