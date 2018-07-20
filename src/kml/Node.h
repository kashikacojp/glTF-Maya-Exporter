#pragma once
#ifndef _KML_NODE_H_
#define _KML_NODE_H_

#include <vector>
#include <memory>
#include <string>

#include "Compatibility.h"
#include "Transform.h"
#include "Bound.h"
#include "Mesh.h"
#include "Material.h"

namespace kml
{
	class Mesh;
	class Bound;
	class Transform;
	class Node
	{
	public:
		Node();
		void SetName(const std::string& name);
		std::string GetName()const;
		std::string GetPath()const;
		const std::shared_ptr<Transform>& GetTransform()const;
		      std::shared_ptr<Mesh>&      GetMesh();
		const std::shared_ptr<Mesh>&      GetMesh()const;
		      std::shared_ptr<Bound>&     GetBound();
		const std::shared_ptr<Bound>&     GetBound()const;

		const std::vector< std::shared_ptr <Material> >& GetMaterials()const;
		const std::vector< std::shared_ptr<Node> >& GetChildren()const;
	public:
		void SetTransform(const std::shared_ptr<Transform>& trans);
		void SetPath (const std::string& path);
		void SetMesh (const std::shared_ptr<Mesh>& mesh);
		void SetBound(const std::shared_ptr<Bound>& bound);
		void AddMaterial(const std::shared_ptr <Material>& material);
		void AddChild(const std::shared_ptr<Node>& child);
		void ClearChildren();
	public:
		bool IsLeaf()const;
		bool HasShape()const;
	protected:
		std::string name;
		std::string path;
		std::vector< std::shared_ptr<Node> > children;
		std::shared_ptr < Transform > transform;
		std::shared_ptr < Bound > bound;
		std::shared_ptr < Mesh > mesh;
		std::vector< std::shared_ptr <Material> > materials;
	};
}

#endif