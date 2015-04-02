/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
#include "OgreStableHeaders.h"

#include "OgreAutoParamDataSource.h"
#include "OgreRenderable.h"
#include "OgreCamera.h"
#include "OgreRenderTarget.h"
#include "OgreControllerManager.h"
#include "OgreMath.h"
#include "OgreRoot.h"
#include "OgreFrameStats.h"
#include "OgreRenderSystem.h"
#include "OgreMatrix4.h"
#include "OgreVector4.h"
#include "OgreColourValue.h"
#include "OgreSceneNode.h"
#include "OgreViewport.h"

#include "Compositor/OgreCompositorShadowNode.h"

namespace Ogre {
    const Matrix4 PROJECTIONCLIPSPACE2DTOIMAGESPACE_PERSPECTIVE(
        0.5,    0,    0,  0.5, 
        0,   -0.5,    0,  0.5, 
        0,      0,    1,    0,
        0,      0,    0,    1);

    //-----------------------------------------------------------------------------
    AutoParamDataSource::AutoParamDataSource()
        : mWorldMatrixCount(0),
         mWorldMatrixArray(0),
         mWorldMatrixDirty(true),
         mViewMatrixDirty(true),
         mProjMatrixDirty(true),
         mWorldViewMatrixDirty(true),
         mViewProjMatrixDirty(true),
         mWorldViewProjMatrixDirty(true),
         mInverseWorldMatrixDirty(true),
         mInverseWorldViewMatrixDirty(true),
         mInverseViewMatrixDirty(true),
         mInverseTransposeWorldMatrixDirty(true),
         mInverseTransposeWorldViewMatrixDirty(true),
         mCameraPositionDirty(true),
         mCameraPositionObjectSpaceDirty(true),
         mPassNumber(0),
         mSceneDepthRangeDirty(true),
         mLodCameraPositionDirty(true),
         mLodCameraPositionObjectSpaceDirty(true),
         mCurrentRenderable(0),
         mCurrentCamera(0), 
         mCurrentLightList(0),
         mCurrentRenderTarget(0),
         mCurrentViewport(0), 
         mCurrentSceneManager(0),
         mCurrentPass(0),
         mCurrentShadowNode(0),
         mBlankLight( 0, &mObjectMemoryManager )
    {
        mBlankLight.setDiffuseColour(ColourValue::Black);
        mBlankLight.setSpecularColour(ColourValue::Black);
        mBlankLight.setAttenuation(0,1,0,0);

        mNodeMemoryManager = new NodeMemoryManager();
        mBlankLightNode = OGRE_NEW SceneNode( 0, 0, mNodeMemoryManager, 0 );
        mBlankLightNode->attachObject( &mBlankLight );
        mBlankLightNode->_getDerivedPositionUpdated();

        for(size_t i = 0; i < OGRE_MAX_SIMULTANEOUS_LIGHTS; ++i)
        {
            mTextureViewProjMatrixDirty[i] = true;
            mTextureWorldViewProjMatrixDirty[i] = true;
            mSpotlightViewProjMatrixDirty[i] = true;
            mSpotlightWorldViewProjMatrixDirty[i] = true;
            mCurrentTextureProjector[i] = 0;
            mShadowCamDepthRangesDirty[i] = false;
        }
        mNullPssmSplitPoint.resize( 4, Real( 0.0f ) );
    }
    //-----------------------------------------------------------------------------
    AutoParamDataSource::~AutoParamDataSource()
    {
        OGRE_DELETE mBlankLightNode;
        mBlankLightNode = 0;
        delete mNodeMemoryManager;
        mNodeMemoryManager = 0;
    }
    //-----------------------------------------------------------------------------
	const Camera* AutoParamDataSource::getCurrentCamera() const
	{
		return mCurrentCamera;
	}
	//-----------------------------------------------------------------------------
    const Light& AutoParamDataSource::getLight(size_t index) const
    {
        // If outside light range, return a blank light to ensure zeroised for program
        if (mCurrentLightList && index < mCurrentLightList->size())
        {
            return *(*mCurrentLightList)[index].light;
        }
        else
        {
            return mBlankLight;
        }
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setCurrentRenderable(const Renderable* rend)
    {
        mCurrentRenderable = rend;
        mWorldMatrixDirty = true;
        mViewMatrixDirty = true;
        mProjMatrixDirty = true;
        mWorldViewMatrixDirty = true;
        mViewProjMatrixDirty = true;
        mWorldViewProjMatrixDirty = true;
        mInverseWorldMatrixDirty = true;
        mInverseViewMatrixDirty = true;
        mInverseWorldViewMatrixDirty = true;
        mInverseTransposeWorldMatrixDirty = true;
        mInverseTransposeWorldViewMatrixDirty = true;
        mCameraPositionObjectSpaceDirty = true;
        mLodCameraPositionObjectSpaceDirty = true;
        for(size_t i = 0; i < OGRE_MAX_SIMULTANEOUS_LIGHTS; ++i)
        {
            mTextureWorldViewProjMatrixDirty[i] = true;
            mSpotlightWorldViewProjMatrixDirty[i] = true;
        }

    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setCurrentCamera(const Camera* cam)
    {
        mCurrentCamera = cam;
        mViewMatrixDirty = true;
        mProjMatrixDirty = true;
        mWorldViewMatrixDirty = true;
        mViewProjMatrixDirty = true;
        mWorldViewProjMatrixDirty = true;
        mInverseViewMatrixDirty = true;
        mInverseWorldViewMatrixDirty = true;
        mInverseTransposeWorldViewMatrixDirty = true;
        mCameraPositionObjectSpaceDirty = true;
        mCameraPositionDirty = true;
        mLodCameraPositionObjectSpaceDirty = true;
        mLodCameraPositionDirty = true;
        mSceneDepthRangeDirty = true;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setCurrentLightList(const LightList* ll)
    {
        mCurrentLightList = ll;
        for(size_t i = 0; i < ll->size() && i < OGRE_MAX_SIMULTANEOUS_LIGHTS; ++i)
        {
            mSpotlightViewProjMatrixDirty[i] = true;
            mSpotlightWorldViewProjMatrixDirty[i] = true;
        }

    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getLightDiffuseColour(size_t index) const
    {
        return getLight(index).getDiffuseColour();
    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getLightSpecularColour(size_t index) const
    {
        return getLight(index).getSpecularColour();
    }
    //-----------------------------------------------------------------------------
    const ColourValue AutoParamDataSource::getLightDiffuseColourWithPower(size_t index) const
    {
        const Light& l = getLight(index);
        ColourValue scaled(l.getDiffuseColour());
        Real power = l.getPowerScale();
        // scale, but not alpha
        scaled.r *= power;
        scaled.g *= power;
        scaled.b *= power;
        return scaled;
    }
    //-----------------------------------------------------------------------------
    const ColourValue AutoParamDataSource::getLightSpecularColourWithPower(size_t index) const
    {
        const Light& l = getLight(index);
        ColourValue scaled(l.getSpecularColour());
        Real power = l.getPowerScale();
        // scale, but not alpha
        scaled.r *= power;
        scaled.g *= power;
        scaled.b *= power;
        return scaled;
    }
    //-----------------------------------------------------------------------------
    Vector3 AutoParamDataSource::getLightPosition(size_t index) const
    {
        return getLight(index).getParentNode()->_getDerivedPosition();
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getLightAs4DVector(size_t index) const
    {
        return getLight(index).getAs4DVector();
    }
    //-----------------------------------------------------------------------------
    Vector3 AutoParamDataSource::getLightDirection(size_t index) const
    {
        return getLight(index).getDerivedDirection();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getLightPowerScale(size_t index) const
    {
        return getLight(index).getPowerScale();
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getLightAttenuation(size_t index) const
    {
        // range, const, linear, quad
        const Light& l = getLight(index);
        return Vector4(l.getAttenuationRange(),
                       l.getAttenuationConstant(),
                       l.getAttenuationLinear(),
                       l.getAttenuationQuadric());
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getSpotlightParams(size_t index) const
    {
        // inner, outer, fallof, isSpot
        const Light& l = getLight(index);
        if (l.getType() == Light::LT_SPOTLIGHT)
        {
            return Vector4(Math::Cos(l.getSpotlightInnerAngle().valueRadians() * 0.5f),
                           Math::Cos(l.getSpotlightOuterAngle().valueRadians() * 0.5f),
                           l.getSpotlightFalloff(),
                           1.0);
        }
        else
        {
            // Use safe values which result in no change to point & dir light calcs
            // The spot factor applied to the usual lighting calc is 
            // pow((dot(spotDir, lightDir) - y) / (x - y), z)
            // Therefore if we set z to 0.0f then the factor will always be 1
            // since pow(anything, 0) == 1
            // However we also need to ensure we don't overflow because of the division
            // therefore set x = 1 and y = 0 so divisor doesn't change scale
            return Vector4(1.0, 0.0, 0.0, 0.0); // since the main op is pow(.., vec4.z), this will result in 1.0
        }
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setCurrentSceneManager(const SceneManager* sm)
    {
        mCurrentSceneManager = sm;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setWorldMatrices(const Matrix4* m, size_t count)
    {
        mWorldMatrixArray = m;
        mWorldMatrixCount = count;
        mWorldMatrixDirty = false;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getWorldMatrix(void) const
    {
        if (mWorldMatrixDirty)
        {
            mWorldMatrixArray = mWorldMatrix;
            mCurrentRenderable->getWorldTransforms(mWorldMatrix);
            mWorldMatrixCount = mCurrentRenderable->getNumWorldTransforms();
            mWorldMatrixDirty = false;
        }
        return mWorldMatrixArray[0];
    }
    //-----------------------------------------------------------------------------
    size_t AutoParamDataSource::getWorldMatrixCount(void) const
    {
        // trigger derivation
        getWorldMatrix();
        return mWorldMatrixCount;
    }
    //-----------------------------------------------------------------------------
    const Matrix4* AutoParamDataSource::getWorldMatrixArray(void) const
    {
        // trigger derivation
        getWorldMatrix();
        return mWorldMatrixArray;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getViewMatrix(void) const
    {
        if (mViewMatrixDirty)
        {
            if (mCurrentRenderable && mCurrentRenderable->getUseIdentityView())
                mViewMatrix = Matrix4::IDENTITY;
            else
            {
                mViewMatrix = mCurrentCamera->getViewMatrix(true);
            }
            mViewMatrixDirty = false;
        }
        return mViewMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getViewProjectionMatrix(void) const
    {
        if (mViewProjMatrixDirty)
        {
            mViewProjMatrix = getProjectionMatrix() * getViewMatrix();
            mViewProjMatrixDirty = false;
        }
        return mViewProjMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getProjectionMatrix(void) const
    {
        if (mProjMatrixDirty)
        {
            // NB use API-independent projection matrix since GPU programs
            // bypass the API-specific handedness and use right-handed coords
            if (mCurrentRenderable && mCurrentRenderable->getUseIdentityProjection())
            {
                // Use identity projection matrix, still need to take RS depth into account.
                RenderSystem* rs = Root::getSingleton().getRenderSystem();
                rs->_convertProjectionMatrix(Matrix4::IDENTITY, mProjectionMatrix, true);
            }
            else
            {
                mProjectionMatrix = mCurrentCamera->getProjectionMatrixWithRSDepth();
            }
            if (mCurrentRenderTarget && mCurrentRenderTarget->requiresTextureFlipping())
            {
                // Because we're not using setProjectionMatrix, this needs to be done here
                // Invert transformed y
                mProjectionMatrix[1][0] = -mProjectionMatrix[1][0];
                mProjectionMatrix[1][1] = -mProjectionMatrix[1][1];
                mProjectionMatrix[1][2] = -mProjectionMatrix[1][2];
                mProjectionMatrix[1][3] = -mProjectionMatrix[1][3];
            }
            mProjMatrixDirty = false;
        }
        return mProjectionMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getWorldViewMatrix(void) const
    {
        if (mWorldViewMatrixDirty)
        {
            mWorldViewMatrix = getViewMatrix().concatenateAffine(getWorldMatrix());
            mWorldViewMatrixDirty = false;
        }
        return mWorldViewMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getWorldViewProjMatrix(void) const
    {
        if (mWorldViewProjMatrixDirty)
        {
            mWorldViewProjMatrix = getProjectionMatrix() * getWorldViewMatrix();
            mWorldViewProjMatrixDirty = false;
        }
        return mWorldViewProjMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getInverseWorldMatrix(void) const
    {
        if (mInverseWorldMatrixDirty)
        {
            mInverseWorldMatrix = getWorldMatrix().inverseAffine();
            mInverseWorldMatrixDirty = false;
        }
        return mInverseWorldMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getInverseWorldViewMatrix(void) const
    {
        if (mInverseWorldViewMatrixDirty)
        {
            mInverseWorldViewMatrix = getWorldViewMatrix().inverseAffine();
            mInverseWorldViewMatrixDirty = false;
        }
        return mInverseWorldViewMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getInverseViewMatrix(void) const
    {
        if (mInverseViewMatrixDirty)
        {
            mInverseViewMatrix = getViewMatrix().inverseAffine();
            mInverseViewMatrixDirty = false;
        }
        return mInverseViewMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getInverseTransposeWorldMatrix(void) const
    {
        if (mInverseTransposeWorldMatrixDirty)
        {
            mInverseTransposeWorldMatrix = getInverseWorldMatrix().transpose();
            mInverseTransposeWorldMatrixDirty = false;
        }
        return mInverseTransposeWorldMatrix;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getInverseTransposeWorldViewMatrix(void) const
    {
        if (mInverseTransposeWorldViewMatrixDirty)
        {
            mInverseTransposeWorldViewMatrix = getInverseWorldViewMatrix().transpose();
            mInverseTransposeWorldViewMatrixDirty = false;
        }
        return mInverseTransposeWorldViewMatrix;
    }
    //-----------------------------------------------------------------------------
    const Vector4& AutoParamDataSource::getCameraPosition(void) const
    {
        if(mCameraPositionDirty)
        {
            Vector3 vec3 = mCurrentCamera->getDerivedPosition();
            mCameraPosition[0] = vec3[0];
            mCameraPosition[1] = vec3[1];
            mCameraPosition[2] = vec3[2];
            mCameraPosition[3] = 1.0;
            mCameraPositionDirty = false;
        }
        return mCameraPosition;
    }    
    //-----------------------------------------------------------------------------
    const Vector4& AutoParamDataSource::getCameraPositionObjectSpace(void) const
    {
        if (mCameraPositionObjectSpaceDirty)
        {
            mCameraPositionObjectSpace = 
                    getInverseWorldMatrix().transformAffine(mCurrentCamera->getDerivedPosition());
            mCameraPositionObjectSpaceDirty = false;
        }
        return mCameraPositionObjectSpace;
    }
    //-----------------------------------------------------------------------------
    const Vector4& AutoParamDataSource::getLodCameraPosition(void) const
    {
        if(mLodCameraPositionDirty)
        {
            Vector3 vec3 = mCurrentCamera->getLodCamera()->getDerivedPosition();
            mLodCameraPosition[0] = vec3[0];
            mLodCameraPosition[1] = vec3[1];
            mLodCameraPosition[2] = vec3[2];
            mLodCameraPosition[3] = 1.0;
            mLodCameraPositionDirty = false;
        }
        return mLodCameraPosition;
    }
    //-----------------------------------------------------------------------------
    const Vector4& AutoParamDataSource::getLodCameraPositionObjectSpace(void) const
    {
        if (mLodCameraPositionObjectSpaceDirty)
        {
            mLodCameraPositionObjectSpace = 
                getInverseWorldMatrix().transformAffine(mCurrentCamera->getLodCamera()->getDerivedPosition());
            mLodCameraPositionObjectSpaceDirty = false;
        }
        return mLodCameraPositionObjectSpace;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setAmbientLightColour(const ColourValue& ambient)
    {
        mAmbientLight = ambient;
    }
    //---------------------------------------------------------------------
    float AutoParamDataSource::getLightCount() const
    {
        return static_cast<float>(mCurrentLightList->size());
    }
    //---------------------------------------------------------------------
    float AutoParamDataSource::getLightCastsShadows(size_t index) const
    {
        return getLight(index).getCastShadows() ? 1.0f : 0.0f;
    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getAmbientLightColour(void) const
    {
        return mAmbientLight;
        
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setCurrentShadowNode(const CompositorShadowNode *sn)
    {
        mCurrentShadowNode = sn;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setCurrentPass(const Pass* pass)
    {
        mCurrentPass = pass;
    }
    //-----------------------------------------------------------------------------
    const Pass* AutoParamDataSource::getCurrentPass(void) const
    {
        return mCurrentPass;
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getTextureSize(size_t index) const
    {
        Vector4 size = Vector4(1,1,1,1);

        if (index < mCurrentPass->getNumTextureUnitStates())
        {
            const TexturePtr& tex = mCurrentPass->getTextureUnitState(
                static_cast<unsigned short>(index))->_getTexturePtr();
            if (!tex.isNull())
            {
                size.x = static_cast<Real>(tex->getWidth());
                size.y = static_cast<Real>(tex->getHeight());
                size.z = static_cast<Real>(tex->getDepth());
            }
        }

        return size;
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getInverseTextureSize(size_t index) const
    {
        Vector4 size = getTextureSize(index);
        return 1 / size;
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getPackedTextureSize(size_t index) const
    {
        Vector4 size = getTextureSize(index);
        return Vector4(size.x, size.y, 1 / size.x, 1 / size.y);
    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getSurfaceAmbientColour(void) const
    {
        return mCurrentPass->getAmbient();
    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getSurfaceDiffuseColour(void) const
    {
        return mCurrentPass->getDiffuse();
    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getSurfaceSpecularColour(void) const
    {
        return mCurrentPass->getSpecular();
    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getSurfaceEmissiveColour(void) const
    {
        return mCurrentPass->getSelfIllumination();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getSurfaceShininess(void) const
    {
        return mCurrentPass->getShininess();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getSurfaceAlphaRejectionValue(void) const
    {
        return static_cast<Real>(static_cast<unsigned int>(mCurrentPass->getAlphaRejectValue())) / 255.0f;
    }
    //-----------------------------------------------------------------------------
    ColourValue AutoParamDataSource::getDerivedAmbientLightColour(void) const
    {
        return getAmbientLightColour() * getSurfaceAmbientColour();
    }
    //-----------------------------------------------------------------------------
    ColourValue AutoParamDataSource::getDerivedSceneColour(void) const
    {
        ColourValue result = getDerivedAmbientLightColour() + getSurfaceEmissiveColour();
        result.a = getSurfaceDiffuseColour().a;
        return result;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setFog(FogMode mode, const ColourValue& colour,
        Real expDensity, Real linearStart, Real linearEnd)
    {
        (void)mode; // ignored
        mFogColour = colour;
        mFogParams.x = expDensity;
        mFogParams.y = linearStart;
        mFogParams.z = linearEnd;
        mFogParams.w = linearEnd != linearStart ? 1 / (linearEnd - linearStart) : 0;
    }
    //-----------------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getFogColour(void) const
    {
        return mFogColour;
    }
    //-----------------------------------------------------------------------------
    const Vector4& AutoParamDataSource::getFogParams(void) const
    {
        return mFogParams;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setTextureProjector(const Frustum* frust, size_t index = 0)
    {
        if (index < OGRE_MAX_SIMULTANEOUS_LIGHTS)
        {
            if( mCurrentTextureProjector[index] != frust )
            {
                mCurrentTextureProjector[index] = frust;
                mTextureViewProjMatrixDirty[index] = true;
                mTextureWorldViewProjMatrixDirty[index] = true;
                mShadowCamDepthRangesDirty[index] = true;
            }
        }

    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getTextureViewProjMatrix(size_t index) const
    {
        if (index < OGRE_MAX_SIMULTANEOUS_LIGHTS)
        {
            if (mTextureViewProjMatrixDirty[index] && mCurrentTextureProjector[index])
            {
                mTextureViewProjMatrix[index] = 
                    PROJECTIONCLIPSPACE2DTOIMAGESPACE_PERSPECTIVE * 
                    mCurrentTextureProjector[index]->getProjectionMatrixWithRSDepth() * 
                    mCurrentTextureProjector[index]->getViewMatrix();
                mTextureViewProjMatrixDirty[index] = false;
            }
            return mTextureViewProjMatrix[index];
        }
        else
            return Matrix4::IDENTITY;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getTextureWorldViewProjMatrix(size_t index) const
    {
        if (index < OGRE_MAX_SIMULTANEOUS_LIGHTS)
        {
            if (mTextureWorldViewProjMatrixDirty[index] && mCurrentTextureProjector[index])
            {
                mTextureWorldViewProjMatrix[index] = 
                    getTextureViewProjMatrix(index) * getWorldMatrix();
                mTextureWorldViewProjMatrixDirty[index] = false;
            }
            return mTextureWorldViewProjMatrix[index];
        }
        else
            return Matrix4::IDENTITY;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getSpotlightViewProjMatrix(size_t index) const
    {
        if (index < OGRE_MAX_SIMULTANEOUS_LIGHTS)
        {
            const Light& l = getLight(index);

            if (&l != &mBlankLight && 
                l.getType() == Light::LT_SPOTLIGHT &&
                mSpotlightViewProjMatrixDirty[index])
            {
                Frustum frust( 0, 0 );
                SceneNode dummyNode( 0, 0, 0, 0 );
                dummyNode.attachObject(&frust);

                frust.setProjectionType(PT_PERSPECTIVE);
                frust.setFOVy(l.getSpotlightOuterAngle());
                frust.setAspectRatio(1.0f);
                // set near clip the same as main camera, since they are likely
                // to both reflect the nature of the scene
                frust.setNearClipDistance(mCurrentCamera->getNearClipDistance());
                // Calculate position, which same as spotlight position
                dummyNode.setPosition(l.getParentNode()->_getDerivedPosition());
                // Calculate direction, which same as spotlight direction
                Vector3 dir = - l.getDerivedDirection(); // backwards since point down -z
                dir.normalise();
                Vector3 up = Vector3::UNIT_Y;
                // Check it's not coincident with dir
                if (Math::Abs(up.dotProduct(dir)) >= 1.0f)
                {
                    // Use camera up
                    up = Vector3::UNIT_Z;
                }
                // cross twice to rederive, only direction is unaltered
                Vector3 left = dir.crossProduct(up);
                left.normalise();
                up = dir.crossProduct(left);
                up.normalise();
                // Derive quaternion from axes
                Quaternion q;
                q.FromAxes(left, up, dir);
                dummyNode.setOrientation(q);

                // The view matrix here already includes camera-relative changes if necessary
                // since they are built into the frustum position
                mSpotlightViewProjMatrix[index] = 
                    PROJECTIONCLIPSPACE2DTOIMAGESPACE_PERSPECTIVE * 
                    frust.getProjectionMatrixWithRSDepth() * 
                    frust.getViewMatrix();

                mSpotlightViewProjMatrixDirty[index] = false;
            }
            return mSpotlightViewProjMatrix[index];
        }
        else
            return Matrix4::IDENTITY;
    }
    //-----------------------------------------------------------------------------
    const Matrix4& AutoParamDataSource::getSpotlightWorldViewProjMatrix(size_t index) const
    {
        if (index < OGRE_MAX_SIMULTANEOUS_LIGHTS)
        {
            const Light& l = getLight(index);

            if (&l != &mBlankLight && 
                l.getType() == Light::LT_SPOTLIGHT &&
                mSpotlightWorldViewProjMatrixDirty[index])
            {
                mSpotlightWorldViewProjMatrix[index] = 
                    getSpotlightViewProjMatrix(index) * getWorldMatrix();
                mSpotlightWorldViewProjMatrixDirty[index] = false;
            }
            return mSpotlightWorldViewProjMatrix[index];
        }
        else
            return Matrix4::IDENTITY;
    }
//-----------------------------------------------------------------------------
  const Matrix4& AutoParamDataSource::getTextureTransformMatrix(size_t index) const
  {
    // make sure the current pass is set
    assert(mCurrentPass && "current pass is NULL!");
    // check if there is a texture unit with the given index in the current pass
    if(index < mCurrentPass->getNumTextureUnitStates())
    {
      // texture unit existent, return its currently set transform
      return mCurrentPass->getTextureUnitState(static_cast<unsigned short>(index))->getTextureTransform();
    }
    else
    {
      // no such texture unit, return unity
      return Matrix4::IDENTITY;
    }
  }
    //-----------------------------------------------------------------------------
    const vector<Real>::type& AutoParamDataSource::getPssmSplits( size_t shadowMapIdx ) const
    {
        vector<Real>::type const *retVal;
        if( !mCurrentShadowNode )
        {
            retVal = &mNullPssmSplitPoint;
        }
        else
        {
            retVal = mCurrentShadowNode->getPssmSplits( shadowMapIdx );
            if( !retVal )
                retVal = &mNullPssmSplitPoint;
        }
        
        return *retVal;
    }
    //-----------------------------------------------------------------------------
    const RenderTarget* AutoParamDataSource::getCurrentRenderTarget(void) const
    {
        return mCurrentRenderTarget;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setCurrentViewport(const Viewport* viewport)
    {
        mCurrentViewport = viewport;
        mCurrentRenderTarget = viewport->getTarget();
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setShadowDirLightExtrusionDistance(Real dist)
    {
        mDirLightExtrusionDistance = dist;
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getShadowExtrusionDistance(void) const
    {
        const Light& l = getLight(0); // only ever applies to one light at once
        if (l.getType() == Light::LT_DIRECTIONAL)
        {
            // use constant
            return mDirLightExtrusionDistance;
        }
        else
        {
            // Calculate based on object space light distance
            // compared to light attenuation range
            Vector3 objPos = getInverseWorldMatrix().transformAffine(l.getParentNode()->_getDerivedPosition());
            return l.getAttenuationRange() - objPos.length();
        }
    }
    //-----------------------------------------------------------------------------
    const Renderable* AutoParamDataSource::getCurrentRenderable(void) const
    {
        return mCurrentRenderable;
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getInverseViewProjMatrix(void) const
    {
        return this->getViewProjectionMatrix().inverse();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getInverseTransposeViewProjMatrix(void) const
    {
        return this->getInverseViewProjMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getTransposeViewProjMatrix(void) const
    {
        return this->getViewProjectionMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getTransposeViewMatrix(void) const
    {
        return this->getViewMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getInverseTransposeViewMatrix(void) const
    {
        return this->getInverseViewMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getTransposeProjectionMatrix(void) const
    {
        return this->getProjectionMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getInverseProjectionMatrix(void) const 
    {
        return this->getProjectionMatrix().inverse();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getInverseTransposeProjectionMatrix(void) const
    {
        return this->getInverseProjectionMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getTransposeWorldViewProjMatrix(void) const
    {
        return this->getWorldViewProjMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getInverseWorldViewProjMatrix(void) const
    {
        return this->getWorldViewProjMatrix().inverse();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getInverseTransposeWorldViewProjMatrix(void) const
    {
        return this->getInverseWorldViewProjMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getTransposeWorldViewMatrix(void) const
    {
        return this->getWorldViewMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Matrix4 AutoParamDataSource::getTransposeWorldMatrix(void) const
    {
        return this->getWorldMatrix().transpose();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getTime(void) const
    {
        return ControllerManager::getSingleton().getElapsedTime();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getTime_0_X(Real x) const
    {
        return fmod(this->getTime(), x);
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getCosTime_0_X(Real x) const
    { 
        return cos(this->getTime_0_X(x)); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getSinTime_0_X(Real x) const
    { 
        return sin(this->getTime_0_X(x)); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getTanTime_0_X(Real x) const
    { 
        return tan(this->getTime_0_X(x)); 
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getTime_0_X_packed(Real x) const
    {
        Real t = this->getTime_0_X(x);
        return Vector4(t, sin(t), cos(t), tan(t));
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getTime_0_1(Real x) const
    { 
        return this->getTime_0_X(x)/x; 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getCosTime_0_1(Real x) const
    { 
        return cos(this->getTime_0_1(x)); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getSinTime_0_1(Real x) const
    { 
        return sin(this->getTime_0_1(x)); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getTanTime_0_1(Real x) const
    { 
        return tan(this->getTime_0_1(x)); 
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getTime_0_1_packed(Real x) const
    {
        Real t = this->getTime_0_1(x);
        return Vector4(t, sin(t), cos(t), tan(t));
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getTime_0_2Pi(Real x) const
    { 
        return this->getTime_0_X(x)/x*2*Math::PI; 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getCosTime_0_2Pi(Real x) const
    { 
        return cos(this->getTime_0_2Pi(x)); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getSinTime_0_2Pi(Real x) const
    { 
        return sin(this->getTime_0_2Pi(x)); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getTanTime_0_2Pi(Real x) const
    { 
        return tan(this->getTime_0_2Pi(x)); 
    }
    //-----------------------------------------------------------------------------
    Vector4 AutoParamDataSource::getTime_0_2Pi_packed(Real x) const
    {
        Real t = this->getTime_0_2Pi(x);
        return Vector4(t, sin(t), cos(t), tan(t));
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getFrameTime(void) const
    {
        return ControllerManager::getSingleton().getFrameTimeSource()->getValue();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getFPS() const
    {
        return Root::getSingleton().getFrameStats()->getFps();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getViewportWidth() const
    { 
        return static_cast<Real>(mCurrentViewport->getActualWidth()); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getViewportHeight() const
    { 
        return static_cast<Real>(mCurrentViewport->getActualHeight()); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getInverseViewportWidth() const
    { 
        return 1.0f/mCurrentViewport->getActualWidth(); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getInverseViewportHeight() const
    { 
        return 1.0f/mCurrentViewport->getActualHeight(); 
    }
    //-----------------------------------------------------------------------------
    Vector3 AutoParamDataSource::getViewDirection() const
    {
        return mCurrentCamera->getDerivedDirection();
    }
    //-----------------------------------------------------------------------------
    Vector3 AutoParamDataSource::getViewSideVector() const
    { 
        return mCurrentCamera->getDerivedRight();
    }
    //-----------------------------------------------------------------------------
    Vector3 AutoParamDataSource::getViewUpVector() const
    { 
        return mCurrentCamera->getDerivedUp();
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getFOV() const
    { 
        return mCurrentCamera->getFOVy().valueRadians(); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getNearClipDistance() const
    { 
        return mCurrentCamera->getNearClipDistance(); 
    }
    //-----------------------------------------------------------------------------
    Real AutoParamDataSource::getFarClipDistance() const
    { 
        return mCurrentCamera->getFarClipDistance(); 
    }
    //-----------------------------------------------------------------------------
    int AutoParamDataSource::getPassNumber(void) const
    {
        return mPassNumber;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::setPassNumber(const int passNumber)
    {
        mPassNumber = passNumber;
    }
    //-----------------------------------------------------------------------------
    void AutoParamDataSource::incPassNumber(void)
    {
        ++mPassNumber;
    }
    //-----------------------------------------------------------------------------
    const Vector4& AutoParamDataSource::getSceneDepthRange() const
    {
        static Vector4 dummy(0, 100000.0, 100000.0, 1/100000.0);

        if (mSceneDepthRangeDirty)
        {
            Real fNear, fFar;
            mCurrentSceneManager->getMinMaxDepthRange( mCurrentCamera, fNear, fFar );
            const Real depthRange = fFar - fNear;
            if (depthRange > std::numeric_limits<Real>::epsilon())
                mSceneDepthRange = Vector4( fNear, fFar, depthRange, 1.0f / depthRange );
            else
                mSceneDepthRange = dummy;

            mSceneDepthRangeDirty = false;
        }

        return mSceneDepthRange;
    }
    //-----------------------------------------------------------------------------
    const Vector4& AutoParamDataSource::getShadowSceneDepthRange(size_t index) const
    {
        static Vector4 dummy(0, 100000.0, 100000.0, 1/100000.0);

        if (index < OGRE_MAX_SIMULTANEOUS_LIGHTS)
        {
            if( mShadowCamDepthRangesDirty[index] && mCurrentTextureProjector[index] )
            {
                Real fNear, fFar;
                mCurrentSceneManager->getMinMaxDepthRange( mCurrentTextureProjector[index],
                                                            fNear, fFar );
                const Real depthRange = fFar - fNear;
                if (depthRange > std::numeric_limits<Real>::epsilon())
                    mShadowCamDepthRanges[index] = Vector4( fNear, fFar, depthRange, 1.0f / depthRange );
                else
                    mShadowCamDepthRanges[index] = dummy;

                mShadowCamDepthRangesDirty[index] = false;
            }

            return mShadowCamDepthRanges[index];
        }
        else
            return dummy;
    }
    //---------------------------------------------------------------------
    const ColourValue& AutoParamDataSource::getShadowColour() const
    {
        return mCurrentSceneManager->getShadowColour();
    }
    //-------------------------------------------------------------------------
    void AutoParamDataSource::updateLightCustomGpuParameter(const GpuProgramParameters::AutoConstantEntry& constantEntry, GpuProgramParameters *params) const
    {
        uint16 lightIndex = static_cast<uint16>(constantEntry.data & 0xFFFF),
            paramIndex = static_cast<uint16>((constantEntry.data >> 16) & 0xFFFF);
        if(mCurrentLightList && lightIndex < mCurrentLightList->size())
        {
            const Light &light = getLight(lightIndex);
            light._updateCustomGpuParameter(paramIndex, constantEntry, params);
        }
    }

}

