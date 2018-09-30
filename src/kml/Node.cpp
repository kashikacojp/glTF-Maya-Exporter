#include "Node.h"
#include "Transform.h"

namespace kml
{
	Node::Node()
	{
		transform.reset( new Transform() );
	}

	void Node::SetName(const std::string& n)
	{
		this->name = n;
	}

	std::string Node::GetName()const
	{
		return this->name;
	}

	void Node::SetPath(const std::string& n)
	{
		this->path = n;
        if (this->modified_path.empty())
        {
            this->modified_path = n;
        }
	}

	std::string Node::GetPath()const
	{
		return this->path;
	}

    void Node::SetModifiedPath(const std::string& n)
    {
        this->modified_path = n;
    }

    std::string Node::GetModifiedPath()const
    {
        return this->modified_path;
    }

	void Node::SetTransform(const std::shared_ptr<Transform>& trans)
	{
		this->transform = trans;
	}

	const std::shared_ptr<Transform>& Node::GetTransform()const
	{
		return this->transform;
	}

	std::shared_ptr<Bound>&     Node::GetBound()
	{
		return bound;
	}

	std::shared_ptr<Mesh>&      Node::GetMesh()
	{
		return this->mesh;
	}

	const std::shared_ptr<Bound>&     Node::GetBound()const
	{
		return bound;
	}

	const std::shared_ptr<Mesh>&      Node::GetMesh()const
	{
		return this->mesh;
	}

	const std::vector< std::shared_ptr <Material> >& Node::GetMaterials()const
	{
		return materials;
	}

	const std::vector< std::shared_ptr<Node> >& Node::GetChildren()const
	{
		return this->children;
	}

	void Node::SetMesh(const std::shared_ptr<Mesh>& m)
	{
		this->mesh = m;
	}

	void Node::SetBound(const std::shared_ptr<Bound>& b)
	{
		this->bound = b;
	}

	void Node::AddMaterial(const std::shared_ptr <Material>& material)
	{
		this->materials.push_back(material);
	}

	void Node::AddChild(const std::shared_ptr<Node>& child)
	{
		this->children.push_back(child);
	}

	bool Node::IsLeaf()const
	{
		return this->children.empty();
	}

	bool Node::HasShape()const
	{
		return this->mesh.get() != NULL;
	}

	void Node::ClearChildren()
	{
		this->children.clear();
	}

    void Node::SetVisiblity(bool isVisible)
    {
        this->iprops["Visiblity"] = isVisible;
    }

    bool Node::GetVisibility()const
    {
        typedef std::map<std::string, int> MapType;
        typedef MapType::const_iterator iterator;
        iterator it = this->iprops.find("Visiblity");
        if (it != this->iprops.end())
        {
            return it->second;
        }
        else
        {
            return true;
        }
    }

    const std::vector< std::shared_ptr <Animation> >& Node::GetAnimations()const
    {
        return animations;
    }

    void Node::AddAnimation(const std::shared_ptr<Animation>& anim)
    {
        animations.push_back(anim);
    }

}