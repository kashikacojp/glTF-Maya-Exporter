#pragma once
#ifndef _KML_MORPHTARGETS_H_
#define _KML_MORPHTARGETS_H_

#include <vector>
#include <memory>

namespace kml
{
    class Mesh;
    class MorphTarget
    {
    public:
        std::shared_ptr<Mesh> mesh;
    };

    class MorphTargets
    {
    public:
        std::vector < std::shared_ptr<MorphTarget> > targets;
        std::vector < float > weights;
    };
}

#endif
