#pragma once
#ifndef _KML_GLTF_EXPORTER_H_
#define _KML_GLTF_EXPORTER_H_

#include <map>
#include <vector>
#include "Node.h"
#include "Options.h"

namespace kml
{
	class glTFExporter
	{
	public:
		bool Export(const std::string& path, const std::shared_ptr<Node>& node, const std::shared_ptr<Options>& opts)const;
	};
}

#endif
