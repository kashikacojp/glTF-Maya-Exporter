#pragma once
#ifndef _KML_ANIMATION_H_
#define _KML_ANIMATION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace kml
{
    class Node;
    enum AnimationInterporationType
    {
        LINEAR,
        STEP,
        CUBICSPLINE
    };

    class AnimationCurve
    {
    public:
        void SetInterpolationType(AnimationInterporationType inter)
        {
            interporation_type = inter;
        }
        AnimationInterporationType GetInterpolationType() const
        {
            return interporation_type;
        }
        std::vector<float>& GetValues()
        {
            return values;
        }
        const std::vector<float>& GetValues() const
        {
            return values;
        }
        void SetValues(const std::vector<float>& v)
        {
            values = v;
        }
    protected:
        AnimationInterporationType interporation_type;
        std::vector<float> values;
    };

    class AnimationPath
    {
    public:
        void SetPathType(const std::string& path)
        {
            path_type = path;
        }
        const std::string& GetPathType() const
        {
            return path_type;
        }
        void SetKeys(const std::shared_ptr<AnimationCurve>& keys)
        {
            this->SetCurve("k", keys);
        }
        std::shared_ptr<AnimationCurve> GetKeys() const
        {
            return this->GetCurve("k");
        }
        void SetCurve(const std::string& key, const std::shared_ptr<AnimationCurve>& val)
        {
            curves[key] = val;
        }
        std::shared_ptr<AnimationCurve> GetCurve(const std::string& key) const
        {
            typedef std::map<std::string, std::shared_ptr<AnimationCurve> > MapType;
            typedef MapType::const_iterator iterator;
            iterator it = curves.find(key);
            if (it != curves.end())
            {
                return it->second;
            }
            else
            {
                return std::shared_ptr<AnimationCurve>();
            }
        }

    protected:
        std::string path_type;
        std::map<std::string, std::shared_ptr<AnimationCurve> > curves;
    };

    class AnimationInstruction
    {
    public:
        const std::vector<std::shared_ptr<AnimationPath> >& GetPaths() const
        {
            return paths;
        }
        const std::vector<std::shared_ptr<Node> >& GetTargets() const
        {
            return targets;
        }
        void AddPath(const std::shared_ptr<AnimationPath>& path)
        {
            paths.push_back(path);
        }
        void SetTarget(const std::shared_ptr<Node>& tar)
        {
            targets.clear();
            targets.push_back(tar);
        }
        //If MorphTargets...
        void SetTargets(const std::vector<std::shared_ptr<Node> >& tars)
        {
            targets = tars;
        }

    protected:
        std::vector<std::shared_ptr<AnimationPath> > paths;
        std::vector<std::shared_ptr<Node> > targets;
    };

    class Animation
    {
    public:
        void SetName(const std::string& n)
        {
            name = n;
        }
        std::string GetName() const
        {
            return name;
        }
        const std::vector<std::shared_ptr<AnimationInstruction> >& GetInstructions()
        {
            return instructions;
        }
        void AddInstruction(const std::shared_ptr<AnimationInstruction>& ins)
        {
            instructions.push_back(ins);
        }

    protected:
        std::string name;
        std::vector<std::shared_ptr<AnimationInstruction> > instructions;
    };

} // namespace kml

#endif
