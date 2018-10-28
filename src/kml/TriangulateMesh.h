#pragma once
#ifndef _KML_TRIANGULATE_MESH_H_
#define _KML_TRIANGULATE_MESH_H_

#include "Mesh.h"
#include <memory>

namespace kml
{
    bool TriangulateMesh(std::shared_ptr<Mesh>& mesh);
}

#endif
