#ifndef _KML_NODE_EXPORTER_H_
#define _KML_NODE_EXPORTER_H_

#include "Compatibility.h"
#include "Node.h"

namespace kml
{
	class NodeExporter
	{
	public:
		static bool Export(const std::string& path, const std::shared_ptr<Node>& node);
	};
}

#endif
