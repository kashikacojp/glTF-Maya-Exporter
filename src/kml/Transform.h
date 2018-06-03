#pragma once
#ifndef _KML_TRANSFORM_H_
#define _KML_TRANSFORM_H_

#include "Compatibility.h"
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace kml
{
	class Transform
	{
	public:
		Transform();
		void SetMatrix(const glm::mat4& m);
		glm::mat4 GetMatrix()const;
	public:
		glm::mat4 mat;
	};
}

#endif
