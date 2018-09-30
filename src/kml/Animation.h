#pragma once
#ifndef _KML_ANIMATION_H_
#define _KML_ANIMATION_H_

#include <string>
#include <memory>
#include <vector>

namespace kml
{
    class Node;
    enum AnimationInterporationType
    {
        LINEAR,
        STEP,
        CUBICSPLINE
    };

    class AnimationCurve
    {
    public:
        std::string channel;
        AnimationInterporationType interporation_type;
        std::vector< float > keys;
        std::map<std::string, std::vector< float >  > values;
        std::shared_ptr<Node> target;
    };

    class Animation
    {
    public:
        std::string name;
        std::vector< std::shared_ptr<AnimationCurve> > curves;
    };

}


#endif
