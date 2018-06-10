#include "Skeleton.h"

namespace kml
{
    Bone::Bone()
    {
        ;
    }

    Bone::~Bone()
    {
        ;
    }
    
    Bone::Bone(const std::string& name)
        :name_(name)
    {
        ;
    }

    void Bone::SetName(const std::string& name)
    {
        name_ = name;
    }

    const std::string& Bone::GetName()const
    {
        return name_;
    }

    std::shared_ptr<Bone> Bone::GetParent()const
    {
        return parent_.lock();
    }

    const std::vector< std::shared_ptr<Bone> >& Bone::GetChildren()const
    {
        return children_;
    }

	std::shared_ptr<Bone> Bone::AddChild(std::shared_ptr<Bone>& parent, const std::shared_ptr<Bone>& bone)
    {
        bone->parent_ = parent;
		parent->children_.push_back(bone);
		return bone;
    }

    const std::shared_ptr<Transform>& Bone::GetTransform()const
    {
        return this->transform_;
    }

    Skeleton::Skeleton()
    {
        ;
    }

	Skeleton::Skeleton(const std::shared_ptr<Bone>& root)
        :root_(root)
    {
        ;
    }

    Skeleton::~Skeleton()
    {
        ;
    }

    static
    void Flatten(std::vector< std::shared_ptr<Bone> >& bones, const std::shared_ptr<Bone>& bone)
    {
        bones.push_back(bone);
        auto& children = bone->GetChildren();
        for(size_t i = 0; i < children.size(); i++)
        {
            Flatten(bones, children[i]);
        }
    }
    
    std::vector< std::shared_ptr<Bone> > Skeleton::GetBones()const
    {
        std::vector< std::shared_ptr<Bone> > bones;
        if(root_.get() != nullptr)
        {
            Flatten(bones, root_);
        }
        return bones;
    }

}
