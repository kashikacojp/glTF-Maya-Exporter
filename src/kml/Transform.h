#pragma once
#ifndef _KML_TRANSFORM_H_
#define _KML_TRANSFORM_H_

#include "Compatibility.h"
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace kml
{
    class TransformCore
    {
    public:
        virtual ~TransformCore() {}
        virtual glm::mat4 GetMatrix()const = 0;
        virtual int GetType()const = 0;
        virtual TransformCore* Clone()const = 0;
        virtual void SetIdentity() = 0;
    };

    class MatrixTransformCore;
    class TRSTransformCore;


	class Transform
	{
    public:
        enum TRANSFORM_TYPE
        {
            TRANSFORM_MATRIX = 0,
            TRANSFORM_TRS = 1
        };
	public:
		Transform();
        ~Transform();
		void SetMatrix(const glm::mat4& m);
        void SetTRS   (const glm::vec3& T, const glm::quat& R, const glm::vec3& S);
        void SetIdentity();
		glm::mat4 GetMatrix()const;
        glm::vec3 GetT()const;
        glm::quat GetR()const;
        glm::vec3 GetS()const;
        int GetType()const;
        bool IsTRS()const;
        const TransformCore* GetCore()const { return core_; }
	public:
        TransformCore * core_;
	};
}

#endif
