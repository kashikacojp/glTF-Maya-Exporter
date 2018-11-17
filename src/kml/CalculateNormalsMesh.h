#pragma once
#ifndef _KML_CALCULATE_NORMALS_MESH_H
#define _KML_CALCULATE_NORMALS_MESH_H

#include "Mesh.h"
#include <memory>

namespace kml
{
    bool RemoveNoAreaMesh(std::shared_ptr<Mesh>& mesh);
    bool CalculateNormalsMesh(std::shared_ptr<Mesh>& mesh);
} // namespace kml

#endif
