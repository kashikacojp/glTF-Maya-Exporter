#pragma once
#ifndef _KML_SPLIT_NODE_BY_MATERIALID_H_
#define _KML_SPLIT_NODE_BY_MATERIALID_H_

#include "Compatibility.h"
#include "Node.h"
#include "Mesh.h"
#include <memory>
#include <vector>

namespace kml
{
	std::vector< std::shared_ptr<kml::Node> > SplitNodeByFaceNormal(std::shared_ptr<kml::Node>& node);
	std::vector< std::shared_ptr<kml::Node> > SplitNodeByFaceOrientation(std::shared_ptr<kml::Node>& node);
	std::vector< std::shared_ptr<kml::Node> > SplitNodeByMaterialID(std::shared_ptr<kml::Node>& node);
}

#endif
