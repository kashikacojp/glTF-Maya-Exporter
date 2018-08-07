#pragma once
#ifndef _KML_SKINWEIGHTS_H_
#define _KML_SKINWEIGHTS_H_

#include <map>

namespace kml
{
	class SkinWeights
	{
	public:
		typedef std::map<std::string, float> WeightVertex;
		std::vector<WeightVertex> vertices;
		std::vector<std::string> joint_names;
	};
}

#endif
