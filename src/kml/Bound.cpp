#include "Bound.h"

namespace kml
{
	Bound::Bound()
	{

	}

	Bound::Bound(const glm::vec3& v0, const glm::vec3& v1)
		:min(v0), max(v1)
	{

	}

	glm::vec3 Bound::GetMin()const
	{
		return min;
	}

	glm::vec3 Bound::GetMax()const
	{
		return max;
	}

}
