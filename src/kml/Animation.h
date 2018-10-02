#pragma once
#ifndef _KML_ANIMATION_H_
#define _KML_ANIMATION_H_

#include <string>
#include <memory>
#include <vector>
#include <map>

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
        AnimationInterporationType GetInterpolationType()const
        {
            return interporation_type;
        }
        std::vector< float >&  GetValues()
        {
            return values;
        }
        const std::vector< float >&  GetValues()const
        {
            return values;
        }
    public:
        AnimationInterporationType interporation_type;
        std::vector< float > values;
    };

    class AnimationChannel
    {
    public:
        void SetChannelType(const std::string& chan)
        {
            channel_type = chan;
        }
        const std::string& GetChannelType()const
        {
            return channel_type;
        }
        void SetKeys(const std::shared_ptr<AnimationCurve>& keys)
        {
            this->SetCurve("k", keys);
        }
        std::shared_ptr<AnimationCurve> GetKeys()const
        {
            return this->GetCurve("k");
        }
        void SetCurve(const std::string& key, const std::shared_ptr<AnimationCurve>& val)
        {
            curves[key] = val;
        }
        std::shared_ptr<AnimationCurve> GetCurve(const std::string& key)const
        {
            typedef std::map<std::string, std::shared_ptr<AnimationCurve> > MapType;
            typedef MapType::const_iterator iterator;
            iterator it = curves.find(key);
            if(it != curves.end())
            {
                return it->second;
            }
            else
            {
                return std::shared_ptr<AnimationCurve>();
            }
        }
    public:
        std::string channel_type;
        std::map<std::string, std::shared_ptr<AnimationCurve> > curves;
    };

    class AnimationInstruction
    {
    public:
        const std::vector< std::shared_ptr<AnimationChannel> >& GetChannels()const
        {
            return channels;
        }
        const std::vector< std::shared_ptr<Node> >& GetTargets()const
        {
            return targets;
        }
        void AddChannel(const std::shared_ptr<AnimationChannel>& chan)
        {
            channels.push_back(chan);
        }
        void SetTargets(const std::vector< std::shared_ptr<Node> >& tars)
        {
            targets = tars;
        }
    public:
        std::vector< std::shared_ptr<AnimationChannel> > channels;
        std::vector< std::shared_ptr<Node> > targets;
    };

    class Animation
    {
    public:
        void SetName(const std::string& n)
        {
            name = n;
        }
        std::string GetName()const
        {
            return name;
        }
        const std::vector< std::shared_ptr<AnimationInstruction> >& GetInstructions()
        {
            return instructions;
        }
        void AddInstruction(const std::shared_ptr<AnimationInstruction>& ins)
        {
            instructions.push_back(ins);
        }
    public:
        std::string name;
        std::vector< std::shared_ptr<AnimationInstruction> > instructions;
    };

}


#endif
