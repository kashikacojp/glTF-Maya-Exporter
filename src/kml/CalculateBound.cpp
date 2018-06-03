#ifndef _WIN32
#define NOMINMAX
#endif
#include "CalculateBound.h"
#include <limits>
#include <algorithm>
#include <vector>

namespace kml
{
	std::shared_ptr<Bound> CalculateBound(const std::shared_ptr<Mesh>& mesh)
	{
		static float MIN_ = -std::numeric_limits<float>::max();
		static float MAX_ = +std::numeric_limits<float>::max();
		glm::vec3 m0(MAX_, MAX_, MAX_);
		glm::vec3 m1(MIN_, MIN_, MIN_);
		for (size_t i = 0; i < mesh->positions.size(); i++)
		{
			glm::vec3 p = mesh->positions[i];
			for (int j = 0; j < 3; j++)
			{
				m0[j] = std::min(m0[j], p[j]);
				m1[j] = std::max(m1[j], p[j]);
			}
		}
		return std::shared_ptr<Bound>(new Bound(m0, m1));
	}
}