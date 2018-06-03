#pragma once
#ifndef _KML_CALCULATE_NORMALS_MESH_H
#define _KML_CALCULATE_NORMALS_MESH_H

#include <memory>
#include "Mesh.h"

namespace kml
{
	bool RemoveNoAreaMesh(std::shared_ptr<Mesh>& mesh);
	bool CalculateNormalsMesh(std::shared_ptr<Mesh>& mesh);
}

#endif
