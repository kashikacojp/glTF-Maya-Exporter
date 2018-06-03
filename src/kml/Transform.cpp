#include "Transform.h"

namespace kml
{
	Transform::Transform()
	{
		mat = glm::mat4(1);
	}

	void Transform::SetMatrix(const glm::mat4& m)
	{
		this->mat = m;
	}	

	glm::mat4 Transform::GetMatrix()const
	{
		return mat;
	}
}
