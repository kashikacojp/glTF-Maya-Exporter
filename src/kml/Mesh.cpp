#include "Mesh.h"

namespace kml
{
	Mesh::Mesh()
	{
		;//
	}

	void Mesh::SetName(const std::string& name)
	{
		this->name = name;
	}

	std::string Mesh::GetName() const
	{
		return this->name;
	}
}