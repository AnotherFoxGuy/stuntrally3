#include "pch.h"
#include "App.h"
#include "settings.h"
#include "SceneXml.h"
#include "GraphicsSystem.h"

#include <OgreRoot.h>
#include <OgreCamera.h>
#include <OgreVector3.h>
#include <OgreWindow.h>
#include <OgreSceneManager.h>

#include <OgreHlmsManager.h>
#include <Compositor/OgreCompositorManager2.h>
#include <Compositor/OgreCompositorNodeDef.h>
#include <Compositor/OgreCompositorNode.h>
#include <Compositor/OgreCompositorWorkspace.h>
#include <Compositor/OgreCompositorWorkspaceListener.h>
#include <Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h>

#include <OgreItem.h>
#include <OgreVector2.h>
#include <OgreMesh.h>
#include <OgreMeshManager.h>
#include <OgreMesh2.h>
#include <OgreMeshManager2.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreRenderable.h>
#include <OgreBitwise.h>
#include <OgreStagingTexture.h>
#include <Compositor/Pass/PassScene/OgreCompositorPassScene.h>
using namespace Ogre;
using namespace std;


//  🕳️ SSAO
//--------------------------------------------------------------------------------------------------------------
void AppGui::InitSSAO()
{
	//  Create SSAO kernel samples
	float kernelSamples[64][4];
	for (size_t i = 0; i < 64u; ++i)
	{
		// Vector3 sample( 10, 10, 10 );
		// while( sample.squaredLength() > 1.0f )
		// {
		//     sample = Vector3( Math::RangeRandom(  -1.0f, 1.0f ),
		//                       Math::RangeRandom(  -1.0f, 1.0f ),
		//                       Math::RangeRandom(  0.01f, 1.0f ) );
		////     sample = Vector3( Math::RangeRandom(  -0.1f, 0.1f ),
		////                       Math::RangeRandom(  -0.1f, 0.1f ),
		////                       Math::RangeRandom(  0.5f, 1.0f ) );
		// }
		Vector3 sample = Vector3( Math::RangeRandom( -1.0f, 1.0f ),
								Math::RangeRandom( -1.0f, 1.0f ),
								Math::RangeRandom( 0.0f, 1.0f ) );
		sample.normalise();

		float scale = (float)i / 64.0f;
		scale = Math::lerp( 0.3f, 1.0f, scale * scale );
		sample = sample * scale;

		kernelSamples[i][0] = sample.x;
		kernelSamples[i][1] = sample.y;
		kernelSamples[i][2] = sample.z;
		kernelSamples[i][3] = 1.0f;
	}

	//  Generate noise texture
	Root *root = mGraphicsSystem->getRoot();
	TextureGpuManager *textureManager = root->getRenderSystem()->getTextureGpuManager();
	TextureGpu *noiseTexture = textureManager->createTexture(
		"noiseTexture", GpuPageOutStrategy::SaveToSystemRam, 0, TextureTypes::Type2D );
	noiseTexture->setResolution( 2u, 2u );
	noiseTexture->setPixelFormat( PFG_RGBA8_SNORM );
	noiseTexture->_transitionTo( GpuResidency::Resident, (uint8 *)0 );
	noiseTexture->_setNextResidencyStatus( GpuResidency::Resident );

	StagingTexture *stagingTexture =
		textureManager->getStagingTexture( 2u, 2u, 1u, 1u, PFG_RGBA8_SNORM );
	stagingTexture->startMapRegion();
	TextureBox texBox = stagingTexture->mapRegion( 2u, 2u, 1u, 1u, PFG_RGBA8_SNORM );

	for (size_t j = 0; j < texBox.height; ++j)
	{
		for (size_t i = 0; i < texBox.width; ++i)
		{
			Vector3 noise = Vector3( Math::RangeRandom( -1.0f, 1.0f ),
									Math::RangeRandom( -1.0f, 1.0f ), 0.0f );
			noise.normalise();

			int8 *pixelData = reinterpret_cast<int8 *>( texBox.at( i, j, 0 ) );
			pixelData[0] = Bitwise::floatToSnorm8( noise.x );
			pixelData[1] = Bitwise::floatToSnorm8( noise.y );
			pixelData[2] = Bitwise::floatToSnorm8( noise.z );
			pixelData[3] = Bitwise::floatToSnorm8( 1.0f );
		}
	}

	stagingTexture->stopMapRegion();
	stagingTexture->upload( texBox, noiseTexture, 0, 0, 0 );
	textureManager->removeStagingTexture( stagingTexture );
	stagingTexture = 0;


	//---------------------------------------------------------------------------------
	//  Set uniforms
	MaterialPtr material =
		std::static_pointer_cast<Material>( MaterialManager::getSingleton().load(
			"SSAO/HS", ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME ) );

	Pass *pass = material->getTechnique( 0 )->getPass( 0 );
	mSSAOPass = pass;
	GpuProgramParametersSharedPtr psParams = pass->getFragmentProgramParameters();

	TextureUnitState *noiseTextureState = pass->getTextureUnitState( "noiseTexture" );
	noiseTextureState->setTexture( noiseTexture );

	//  Reconstruct position from depth. Position is needed in SSAO
	//  We need to set the parameters based on camera to the
	//  shader so that the un-projection works as expected
	Camera *camera = mCamera;  //mGraphicsSystem->getCamera();
	Vector2 projectionAB = camera->getProjectionParamsAB();
	//  The division will keep "linearDepth" in the shader in the [0; 1] range.
	projectionAB.y /= camera->getFarClipDistance();
	psParams->setNamedConstant( "projectionParams", projectionAB );

	//  other uniforms
	psParams->setNamedConstant( "kernelRadius", pSet->ssao_radius );
	psParams->setNamedConstant(
		"noiseScale",
		Vector2(
			( Real( mGraphicsSystem->getRenderWindow()->getWidth() ) * 0.5f ) / 2.0f,
			( Real( mGraphicsSystem->getRenderWindow()->getHeight() ) * 0.5f ) / 2.0f ) );
	psParams->setNamedConstant( "invKernelSize", 1.0f / 64.0f );
	psParams->setNamedConstant( "sampleDirs", (float *)kernelSamples, 64, 4 );

	//  blur shader uniforms
	MaterialPtr materialBlurH =
		std::static_pointer_cast<Material>( MaterialManager::getSingleton().load(
			"SSAO/BlurH", ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME ) );

	Pass *passBlurH = materialBlurH->getTechnique( 0 )->getPass( 0 );
	GpuProgramParametersSharedPtr psParamsBlurH = passBlurH->getFragmentProgramParameters();
	psParamsBlurH->setNamedConstant( "projectionParams", projectionAB );

	MaterialPtr materialBlurV =
		std::static_pointer_cast<Material>( MaterialManager::getSingleton().load(
			"SSAO/BlurV", ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME ) );

	Pass *passBlurV = materialBlurV->getTechnique( 0 )->getPass( 0 );
	GpuProgramParametersSharedPtr psParamsBlurV = passBlurV->getFragmentProgramParameters();
	psParamsBlurV->setNamedConstant( "projectionParams", projectionAB );

	//  apply shader uniforms
	MaterialPtr materialApply =
		std::static_pointer_cast<Material>( MaterialManager::getSingleton().load(
			"SSAO/Apply", ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME ) );

	Pass *passApply = materialApply->getTechnique( 0 )->getPass( 0 );
	mApplyPass = passApply;
	GpuProgramParametersSharedPtr psParamsApply = passApply->getFragmentProgramParameters();
	psParamsApply->setNamedConstant( "powerScale", pSet->ssao_scale );
}


//  🕳️💫 Update
//-----------------------------------------------------------------------------------
void AppGui::UpdSSAO(Camera* camera)
{
	GpuProgramParametersSharedPtr psParams = mSSAOPass->getFragmentProgramParameters();
#if OGRE_NO_VIEWPORT_ORIENTATIONMODE == 0
	//  We don't render to render window directly, thus we need to get the projection
	//  matrix with phone orientation disable when calculating SSAO
	camera->setOrientationMode( OR_DEGREE_0 );
#endif
	psParams->setNamedConstant( "projection", camera->getProjectionMatrix() );
	psParams->setNamedConstant( "kernelRadius",
#ifdef SR_EDITOR
		edPrvCam ? 0.3f :  // const for 1024 view rtt
#endif
		pSet->ssao_radius );

	GpuProgramParametersSharedPtr psParamsApply = mApplyPass->getFragmentProgramParameters();
	psParamsApply->setNamedConstant( "powerScale",
#ifdef SR_EDITOR
		edPrvCam ? 1.8f :
#endif
		pSet->ssao_scale );
}

//  💫 common util  🔆🌄
void AppGui::UpdSunPos(Camera *camera)
{
	//  project sun pos to screen
	Vector3 pos = scn->fx.sunDir * -10000.f;  // very far
	Vector3 p2 = camera->getProjectionMatrix() * (camera->getViewMatrix() * pos);
	
	auto& v = uvSunPos_Fade;
	v.x =  p2.x * 0.5f;
	v.y = -p2.y * 0.5f;
	v.z = max(0.f, min(1.f, -scn->fx.sunDir.dotProduct(camera->getDirection()) ));  // sun dot cam
	v.w = camera->getAspectRatio();
	
	efxClrSun = scn->fx.sunClr;
	efxClrRays = scn->fx.raysClr;  // or own par-
}


//  🔆 Lens flare
//--------------------------------------------------------------------------------------------------------------
void AppGui::InitLens()
{
	MaterialPtr material = std::static_pointer_cast<Material>( MaterialManager::getSingleton().load(
		"LensFlare", ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME ) );
	mLensPass = material->getTechnique(0)->getPass(0);
}

void AppGui::UpdLens()
{
	if (!mLensPass)  return;
	GpuProgramParametersSharedPtr psParams = mLensPass->getFragmentProgramParameters();
	psParams->setNamedConstant( "uvSunPos_Fade", uvSunPos_Fade );
	psParams->setNamedConstant( "efxClrSun", efxClrSun );
}


//  🌄 Sun beams
//--------------------------------------------------------------------------------------------------------------
void AppGui::InitSunbeams()
{
	MaterialPtr material = std::static_pointer_cast<Material>( MaterialManager::getSingleton().load(
		"SunBeams", ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME ) );
	mSunbeamsPass = material->getTechnique(0)->getPass(0);
}

void AppGui::UpdSunbeams()
{
	if (!mSunbeamsPass)  return;
	GpuProgramParametersSharedPtr psParams = mSunbeamsPass->getFragmentProgramParameters();
	psParams->setNamedConstant( "uvSunPos_Fade", uvSunPos_Fade );
	psParams->setNamedConstant( "efxClrRays", efxClrRays );
}


//  All post effects
//--------------------------------------------------------------------------------------------------------------
void AppGui::InitEffects()
{
	InitSSAO();
	InitLens();
	InitSunbeams();
}

//  done in ReflectListener::passEarlyPreExecute too
void AppGui::UpdateEffects(Camera *cam)
{
	if (pSet->g.ssao)
		UpdSSAO(cam);  // 🕳️
	
	if (pSet->g.lens_flare ||
		pSet->g.sunbeams)
		UpdSunPos(cam);  // 🔆🌄

	if (pSet->g.lens_flare)
		UpdLens();  // 🔆
	
	if (pSet->g.sunbeams)
		UpdSunbeams();  // 🌄
	
	if (pSet->gi)
		UpdateGI();  // 🌇
}
