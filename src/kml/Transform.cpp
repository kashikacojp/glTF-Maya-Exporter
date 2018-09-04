#define GLM_ENABLE_EXPERIMENTAL 1
#include "Transform.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace kml
{
    typedef Transform::TRANSFORM_TYPE TRANSFORM_TYPE;
    
    class MatrixTransformCore : public TransformCore
    {
    public:
        MatrixTransformCore(const glm::mat4& m)
            :mat_(m)
        {}
        virtual glm::mat4 GetMatrix()const
        {
            return mat_;
        }
        virtual int GetType()const
        {
            return TRANSFORM_TYPE::TRANSFORM_MATRIX;
        }
        virtual TransformCore* Clone()const
        {
            return new MatrixTransformCore(mat_);
        }
    protected:
        glm::mat4 mat_;
    };

    class TRSTransformCore : public TransformCore
    {
    public:
        TRSTransformCore(const glm::vec3& T, const glm::quat& R, const glm::vec3& S)
            :T_(T), R_(R), S_(S)
        {}
        virtual glm::mat4 GetMatrix()const
        {
            return GetTMat() * GetRMat() * GetSMat();
        }
        virtual int GetType()const
        {
            return TRANSFORM_TYPE::TRANSFORM_TRS;
        }
        virtual TransformCore* Clone()const
        {
            return new TRSTransformCore(T_, R_, S_);
        }

        glm::vec3 GetT()const { return T_; }
        glm::quat GetR()const { return R_; }
        glm::vec3 GetS()const { return S_; }
    protected:
        glm::mat4 GetTMat()const
        {
            return glm::translate(glm::mat4(1.0f), T_);
        }
        glm::mat4 GetRMat()const
        {
            return glm::toMat4(R_);
        }
        glm::mat4 GetSMat()const
        {
            return glm::scale(glm::mat4(1.0f), S_);
        }
    protected:
        glm::vec3 T_;
        glm::quat R_;
        glm::vec3 S_;
    };

	Transform::Transform()
	{
        core_ = new MatrixTransformCore(glm::mat4(1.0));
	}

    Transform::~Transform()
    {
        delete core_;
    }

    void Transform::SetMatrix(const glm::mat4& m)
    {
        TransformCore* tmp = core_;
        core_ = new MatrixTransformCore(m);
        delete tmp;
    }

    void Transform::SetTRS(const glm::vec3& T, const glm::quat& R, const glm::vec3& S)
    {
        TransformCore* tmp = core_;
        core_ = new TRSTransformCore(T, R, S);
        delete tmp;
    }

	glm::mat4 Transform::GetMatrix()const
	{
		return core_->GetMatrix();
	}

    int Transform::GetType()const
    {
        return core_->GetType();
    }

    glm::vec3 Transform::GetT()const
    {
        if (this->GetType() == TRANSFORM_TYPE::TRANSFORM_TRS)
        {
            return dynamic_cast<const TRSTransformCore*>(this->core_)->GetT();
        }
        else
        {
            return glm::vec3(0);
        }
    }
    glm::quat Transform::GetR()const
    {
        if (this->GetType() == TRANSFORM_TYPE::TRANSFORM_TRS)
        {
            return dynamic_cast<const TRSTransformCore*>(this->core_)->GetR();
        }
        else
        {
            return glm::quat(1,0,0,0);
        }
    }
    glm::vec3 Transform::GetS()const
    {
        if (this->GetType() == TRANSFORM_TYPE::TRANSFORM_TRS)
        {
            return dynamic_cast<const TRSTransformCore*>(this->core_)->GetS();
        }
        else
        {
            return glm::vec3(1, 1, 1);
        }
    }
}
