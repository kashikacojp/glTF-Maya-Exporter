#ifndef _WIN32
#define NOMINMAX
#endif
#include "CalculateBound.h"
#include <algorithm>
#include <limits>
#include <vector>

namespace kml
{
    static glm::vec3 Transform(const glm::mat4& mat, const glm::vec3& v)
    {
        glm::vec4 t = mat * glm::vec4(v, 1.0f);
        t[0] /= t[3];
        t[1] /= t[3];
        t[2] /= t[3];
        return glm::vec3(t[0], t[1], t[2]);
    }

    std::shared_ptr<Bound> CalculateBound(const std::shared_ptr<Mesh>& mesh, const glm::mat4& mat)
    {
        static float MIN_ = -std::numeric_limits<float>::max();
        static float MAX_ = +std::numeric_limits<float>::max();
        glm::vec3 m0(MAX_, MAX_, MAX_);
        glm::vec3 m1(MIN_, MIN_, MIN_);
        for (size_t i = 0; i < mesh->positions.size(); i++)
        {
            glm::vec3 p = mesh->positions[i];
            p = Transform(mat, p);
            for (int j = 0; j < 3; j++)
            {
                m0[j] = std::min(m0[j], p[j]);
                m1[j] = std::max(m1[j], p[j]);
            }
        }
        return std::shared_ptr<Bound>(new Bound(m0, m1));
    }
} // namespace kml