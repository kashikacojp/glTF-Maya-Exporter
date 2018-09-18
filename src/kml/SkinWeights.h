#pragma once
#ifndef _KML_SKINWEIGHTS_H_
#define _KML_SKINWEIGHTS_H_

#include <map>
#include <string>

#include <glm/glm.hpp>

namespace kml
{
	class SkinWeights
	{
	public:
		typedef std::map<std::string, float> WeightVertex;
        std::string name;
		std::vector<WeightVertex> weights;
		std::vector<std::string> joint_paths;
        std::vector<glm::mat4> joint_bind_matrices;
	};
}

#endif
