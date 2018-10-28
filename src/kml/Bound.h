#pragma once
#ifndef _KML_BOUND_H_
#define _KML_BOUND_H_

#include <glm/glm.hpp>

namespace kml
{
    class Bound
    {
    public:
        Bound();
        Bound(const glm::vec3 &v0, const glm::vec3 &v1);
        glm::vec3 GetMin() const;
        glm::vec3 GetMax() const;

    protected:
        glm::vec3 min;
        glm::vec3 max;
    };
} // namespace kml

#endif
