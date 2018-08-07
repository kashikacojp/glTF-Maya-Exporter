#pragma once
#ifndef _KML_SKELETON_H_
#define _KML_SKELETON_H_

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

#include "Transform.h"

namespace kml
{
    class Bone
    {
    public:
        Bone();
        virtual ~Bone();
        Bone(const std::string& name);
        void SetName(const std::string& name);
        const std::string& GetName()const;
        std::shared_ptr<Bone> GetParent()const;
        const std::vector< std::shared_ptr<Bone> >& GetChildren()const;
    public:
        const std::shared_ptr<Transform>& GetTransform()const;
	public:
		static std::shared_ptr<Bone> AddChild(std::shared_ptr<Bone>& parent, const std::shared_ptr<Bone>& bone);
    protected:
        std::string name_;
        std::shared_ptr<Transform> transform_;
        mutable std::weak_ptr<Bone> parent_;
        std::vector< std::shared_ptr<Bone> > children_;
    };

    class Skeleton
    {
    public:
        Skeleton();
        Skeleton(const std::shared_ptr<Bone>& root);
        virtual ~Skeleton();
        std::vector< std::shared_ptr<Bone> > GetBones()const;
    protected:
        std::shared_ptr<Bone> root_;
    };

}

#endif
