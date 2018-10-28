#pragma once
#ifndef _KML_FLAT_INDICES_MESH_H_
#define _KML_FLAT_INDICES_MESH_H_

#include "Mesh.h"
#include <memory>

namespace kml
{
    bool FlatIndicesMesh(std::shared_ptr<Mesh>& mesh);
}

#endif