/*
Modified by CryHam  under GPLv3
-----------------------------------------------------------------------------
This source file WAS part of OGRE-Next
	(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2021 Torus Knot Software Ltd

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
#pragma once

#include <OgrePrerequisites.h>
#include <OgreMovableObject.h>
#include <OgreShaderParams.h>

#include "TerrainCell.h"

class Scene;
class App;


namespace Ogre
{
	class HlmsTerraDatablock;

	struct GridPoint
	{
		int32 x;
		int32 z;
	};

	struct GridDirection
	{
		int x;
		int z;
	};

	class ShadowMapper;
	struct TerraSharedResources;

	/**
	@brief The Terra class
		Internally Terra operates in Y-up space so input and outputs may
		be converted to/from the correct spaces based on setting, unless
		explicitly stated to be always Y-up by documentation.
	*/
	class Terra : public MovableObject
	{
		friend class TerrainCell;

		struct SavedState
		{
			RenderableArray m_renderables;
			size_t m_currentCell;
			Camera const *m_camera;
		};

		std::vector<float>  m_heightMap;
		uint32      m_iWidth;
		uint32      m_iHeight;

		float       m_depthWidthRatio;
		float       m_skirtSize; // Already unorm scaled
		float       m_invWidth;
		float       m_invDepth;

		bool m_zUp;

		Vector2     m_xzDimensions;
		Vector2     m_xzInvDimensions;
		Vector2     m_xzRelativeSize; // m_xzDimensions / [m_width, m_height]
		float       m_fHeightMul;
		float       m_heightUnormScaled; // m_height / 1 or m_height / 65535
		Vector3     m_terrainOrigin;
		uint32      m_basePixelDimension;

		/// 0 is currently in use
		/// 1 is SavedState
		std::vector<TerrainCell>   m_terrainCells[2];
		/// 0 & 1 are for tmp use
		std::vector<TerrainCell*>  m_collectedCells[2];
		size_t                     m_currentCell;

		TextureGpu*   m_heightMapTex;

		Vector3       m_prevLightDir;
		ShadowMapper  *m_shadowMapper;

		/// When rendering shadows we want to override the data calculated by update
		/// but only temporarily, for later restoring it.
		SavedState m_savedState;

		//Ogre stuff
		CompositorManager2      *m_compositorManager;
		Camera const            *m_camera;

		/// Converts value from Y-up to whatever the user up vector is (see m_zUp)
		inline Vector3 fromYUp( Vector3 value ) const;
		/// Same as fromYUp, but preserves original sign. Needed when value is a scale
		inline Vector3 fromYUpSignPreserving( Vector3 value ) const;
		/// Converts value from user up vector to Y-up
		inline Vector3 toYUp( Vector3 value ) const;
		/// Same as toYUp, but preserves original sign. Needed when value is a scale
		inline Vector3 toYUpSignPreserving( Vector3 value ) const;

	public:
		uint32 mHlmsTerraIndex;

		//--------------------------------------------------------------------------------
		//begin  extras by CryHam
		Ogre::String mtrName;
		Ogre::SceneNode* node =0;
		Ogre::HlmsTerraDatablock* tDb = 0;

		bool bGenerateShadowMap;  //** ter
		bool bNormalized;  // true: Hmap floats 0..1 (broken),  false: any, real heights
		int iLodMax;  //**
		float fHMin, fHMax, fHRange;  //norm meh-
		
		App* app =0;
		Scene* sc =0;
		int cnt = 0;  // name counter, for many, Id
		
		int getSize()
		{	return m_iWidth;  }

		std::vector<float>& getHeightData()
		{	return m_heightMap;  }
		
		//  upload hmap to gpu tex after edits, rect ignored
		void dirtyRect(Rect rect);

		//  util
		bool worldInside( const Vector3 &vPos ) const;
		bool getHeightAt( Vector3 &vPos ) const;
		float getAngle( float x, float z, float s) const;
		Real getHeight( Real x, Real z ) const;

		//----------------------------------------
		/*struct Grassmap  // todo
		{
			Terra* pTerra =0;
			Grassmap(Terra* terra);

			TextureGpu* texture =0;
			Pass* pass =0;
			Camera* camera =0;
			CompositorWorkspace* workspace =0;

			void Create();
			void SetParams(), Update();
			void Destroy();
		}
		grassmap;*/

		//----------------------------------------
		struct Blendmap
		{
			Terra* pTerra =0;
			Blendmap(Terra* terra);

			TextureGpu* texture =0;
			Pass* pass =0;
			Camera* camera =0;
			CompositorWorkspace* workspace =0;

			void Create();
			void SetParams(), Update();
			void Destroy();
		}
		blendmap;
		//----------------------------------------

		void readBackHmap( std::vector<float>& hfHeight, int row );
	protected:
		void createHmapTex(const std::vector<float>& hfHeight, int row );

		struct Normalmap
		{
			Terra* pTerra =0;
			Normalmap(Terra* terra);

			TextureGpu* texture =0, *rtt =0;
			Camera* camera =0;
			CompositorWorkspace* workspace =0;

			void Create();
			void Update();
			void Destroy();
		}
		normalmap;

		//end  extras by CryHam
		//--------------------------------------------------------------------------------

		void destroyHeightmapTex();

		/// Creates the Ogre texture based on heightmap float array.
		void createHeightmap(
			int width, int height,
			std::vector<float> hfHeight, int row,
			bool bMinimizeMemoryConsumption, bool bLowResShadow );

		///	Automatically calculates the optimum skirt size (no gaps with lowest overdraw possible).
		///	This is done by taking the heighest delta between two adjacent pixels in a 4x4 block.
		///	This calculation may not be perfect, as the block search should get bigger for higher LODs.
		void calculateOptimumSkirtSize();

		inline GridPoint worldToGrid( const Vector3 &vPos ) const;
		inline Vector2 gridToWorld( const GridPoint &gPos ) const;

		bool isVisible( const GridPoint &gPos, const GridPoint &gSize ) const;

		void addRenderable( const GridPoint &gridPos, const GridPoint &cellSize, uint32 lodLevel );

		void optimizeCellsAndAdd();

	public:
		//  🌟 ctor
		Terra( App* app1, Scene* sc1, int n,
				ObjectMemoryManager *objectMemoryManager, SceneManager *sceneManager,
				uint8 renderQueueId, CompositorManager2 *compositorManager, Camera *camera, bool zUp );
		~Terra() override;

		/// Sets shared resources for minimizing memory consumption wasted on temporary
		/// resources when you have more than one Terra.
		///
		/// This function is only useful if you load/have multiple Terras at once.
		///
		/// @see    TerraSharedResources
		// void setSharedResources( TerraSharedResources *sharedResources );

		/// How low should the skirt be. Normally you should let this value untouched and let
		/// calculateOptimumSkirtSize do its thing for best performance/quality ratio.
		///
		/// However if your height values are unconventional (i.e. artificial, non-natural) and you
		/// need to look the terrain from the "outside" (rather than being inside the terrain),
		/// you may have to tweak this value manually.
		///
		/// This value should be between min height and max height of the heightmap.
		///
		/// A value of 0.0 will give you the biggest skirt and fix all skirt-related issues.
		/// Note however, this may have a *tremendous* GPU performance impact.
		void setCustomSkirtMinHeight( const float skirtMinHeight ) { m_skirtSize = skirtMinHeight; }
		float getCustomSkirtMinHeight() const { return m_skirtSize; }

		/** Must be called every frame so we can check the camera's position
			(passed in the constructor) and update our visible batches (and LODs)
			We also update the shadow map if the light direction changed.
		@param lightDir
			Light direction for computing the shadow map.
		@param lightEpsilon
			Epsilon to consider how different light must be from previous
			call to recompute the shadow map.
			Interesting values are in the range [0; 2], but any value is accepted.
		@par
			Large epsilons will reduce the frequency in which the light is updated,
			improving performance (e.g. only compute the shadow map when needed)
		@par
			Use an epsilon of <= 0 to force recalculation every frame. This is
			useful to prevent heterogeneity between frames (reduce stutter) if
			you intend to update the light slightly every frame.
		*/
		void update( const Vector3 &lightDir, Ogre::Camera* camera, float lightEpsilon=1e-6f );

		/**
		@brief load
		@param texName
		@param center
		@param dimensions
		@param bMinimizeMemoryConsumption
			See ShadowMapper::setMinimizeMemoryConsumption
		@param bLowResShadow
			See ShadowMapper::createShadowMap
		*/
		void load( int width, int height, std::vector<float> hfHeight, int row,
				   Vector3 center, Vector3 dimensions, bool bMinimizeMemoryConsumption,
				   bool bLowResShadow );

		/** Gets the interpolated height at the given location.
			If outside the bounds, it leaves the height untouched.
		@param vPos
			Y-up:
				[in] XZ position, Y for default height.
				[out] Y height, or default Y (from input) if outside terrain bounds.
			Z-up
				[in] XY position, Z for default height.
				[out] Z height, or default Z (from input) if outside terrain bounds.
		@return
			True if Y (or Z for Z-up) component was changed
		*/
		
		/// load must already have been called.
		void setDatablock( HlmsDatablock *datablock );

		//MovableObject overloads
		const String &getMovableType() const override;

		/// Swaps current state with a saved one. Useful for rendering shadow maps
		void _swapSavedState();

		const Camera* getCamera() const                 { return m_camera; }
		void setCamera( const Camera *camera )          { m_camera = camera; }

		bool isZUp() const { return m_zUp; }

		const ShadowMapper* getShadowMapper() const { return m_shadowMapper; }

		TextureGpu* getHeightMapTex() const   { return m_heightMapTex; }
		TextureGpu* getNormalMapTex() const   { return normalmap.texture; }
		TextureGpu* _getShadowMapTex() const;

		// These are always in Y-up space
		const Vector2& getXZDimensions() const      { return m_xzDimensions; }
		const Vector2& getXZInvDimensions() const   { return m_xzInvDimensions; }
		float getHeightMul() const                  { return m_fHeightMul; }
		const Vector3& getTerrainOriginRaw() const  { return m_terrainOrigin; }

		/// Return value is in client-space (i.e. could be y- or z-up)
		Vector3 getTerrainOrigin() const;

		// Always in Y-up space
		Vector2 getTerrainXZCenter() const;
	};


	/** Terra during creation requires a number of temporary surfaces that are used then discarded.
		These resources can be shared by all the Terra 'islands' since as long as they have the
		same resolution and format.

		Originally, we were creating them and destroying them immediately. But because
		GPUs are async and drivers don't discard resources immediately, it was causing
		out of memory conditions.

		Once all N Terras are constructed, memory should be freed.

		Usage:

		@code
			// At level load:
			TerraSharedResources *sharedResources = new TerraSharedResources();
			for( i < numTerras )
			{
				terra[i]->setSharedResources( sharedResources );
				terra[i]->load( "Heightmap.png" );
			}

			// Free memory that is only used during Terra::load()
			sharedResources->freeStaticMemory();

			// Every frame
			for( i < numTerras )
				terra[i]->update( lightDir );

			// On shutdown:
			for( i < numTerras )
				delete terra[i];
			delete sharedResources;
		@endcode
	*/
	
	
	struct TerraSharedResources
	{
		/**
		@brief getTempTexture
			Retrieves a cached texture to be shared with all Terras.
			If no such texture exists, creates it.

			If sharedResources is a nullptr (i.e. no sharing) then we create a temporary
			texture that will be freed by TerraSharedResources::destroyTempTexture

			If sharedResources is not nullptr, then the texture will be freed much later on
			either by TerraSharedResources::freeStaticMemory or the destructor
		@param terra
		@param sharedResources
			Can be nullptr
		@param temporaryUsage
			Type of texture to use
		@param baseTemplate
			A TextureGpu that will be used for basis of constructing our temporary RTT
		@return
			A valid ptr
		*/
		static TextureGpu *getTempTexture( const char *texName, IdType id,
										   TextureGpu *baseTemplate,
										   uint32 flags );
		/**
		@brief destroyTempTexture
			Destroys a texture created by TerraSharedResources::getTempTexture ONLY IF sharedResources
			is nullptr
		@param sharedResources
		@param tmpRtt
		*/
		static void destroyTempTexture( TextureGpu *tmpRtt );
	};
}
