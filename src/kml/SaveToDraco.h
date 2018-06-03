#pragma once
#ifndef _KML_SAVE_TO_DRACO_H
#define _KML_SAVE_TO_DRACO_H

#include "Mesh.h"
#include <memory>
#include <vector>

namespace kml
{
	
	bool SaveToDraco(std::vector<unsigned char>& bytes, const std::shared_ptr<Mesh>& mesh);
	bool SaveToDraco(const std::string& strPath, const std::shared_ptr<Mesh>& mesh);
}

#endif
