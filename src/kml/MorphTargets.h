#pragma once
#ifndef _KML_MORPHTARGETS_H_
#define _KML_MORPHTARGETS_H_

#include <string>
#include <vector>
#include <memory>

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
        std::vector < std::shared_ptr<MorphTarget> > targets;
        std::vector < float > weights;
        std::vector < std::string > names;
    };
}

#endif
