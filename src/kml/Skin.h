#pragma once
#ifndef _KML_SKIN_H_
#define _KML_SKIN_H_

#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

namespace kml
{
    class Node;
    class SkinWeight
	{
	public:
		typedef std::map<std::string, float> WeightVertex;
    public:
        std::string GetName()const 
        {
            return name;
        }
        const std::vector<WeightVertex>& GetWeightVertices()const
        {
            return weights;
        }
        const std::vector<std::string>& GetJointPaths()const
        {
            return joint_paths;
        }
        const std::vector<glm::mat4>& GetJointBindMatrices()const
        {
            return joint_bind_matrices;
        }
    public:
        std::string name;
		std::vector<WeightVertex> weights;
		std::vector<std::string> joint_paths;
        std::vector<glm::mat4> joint_bind_matrices;
	};

    class Skin
    {
    public:
        std::string GetName()const
        {
            return name_;
        }
        const std::vector< std::shared_ptr<SkinWeight> >& GetSkinWeights()const
        {
            return skin_weights_;
        }
        const std::vector< std::shared_ptr<Node> >& GetJoints()const
        {
            return joints_;
        }
        const std::vector<glm::mat4>& GetJointBindMatrices()const
        {
            return joint_bind_matrices_;
        }
    public:
        void SetName(const std::string& name)
        {
            name_ = name;
        }
        void AddSkinWeight(const std::shared_ptr<SkinWeight>& w)
        {
            skin_weights_.push_back(w);
        }
        void AddJoint(const std::shared_ptr<Node>& n)
        {
            joints_.push_back(n);
        }
        void SetSkinWeights(const std::vector< std::shared_ptr<SkinWeight> >& w)
        {
            skin_weights_ = w;
        }
        void SetJoints(const std::vector< std::shared_ptr<Node> >& nodes)
        {
            joints_ = nodes;
        }
        void SetJointBindMatrices(const std::vector<glm::mat4>& mats)
        {
            joint_bind_matrices_ = mats;
        }
    protected:
        std::string name_;
        std::vector< std::shared_ptr<SkinWeight> > skin_weights_;
        std::vector< std::shared_ptr<Node> > joints_;
        std::vector<glm::mat4> joint_bind_matrices_;
    };
}

#endif
