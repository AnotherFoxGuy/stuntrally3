#include "pch.h"
#include "Def_Str.h"
#include "App.h"
#include "settings.h"

#include "GraphicsSystem.h"
#include "GameState.h"
#if OGRE_PLATFORM != OGRE_PLATFORM_APPLE_IOS
	#include "SdlInputHandler.h"
#endif
#include "HlmsPbs2.h"  // our
#include "MainEntryPoints.h"

#include <OgreLogManager.h>
#include <OgreConfigFile.h>
#include <OgreException.h>
#include <OgreRoot.h>
#include <OgreWindow.h>
#include <OgreWindowEventUtilities.h>

#include <OgreFileSystemLayer.h>
#include <OgreAbiUtils.h>
#include <OgrePlatformInformation.h>
#include "System/Android/AndroidSystems.h"

#include <OgreCamera.h>
#include <OgreItem.h>
#include <OgreTextureGpuManager.h>
#include <OgreGpuProgramManager.h>

#include <OgreHlmsUnlit.h>
// #include <OgreHlmsPbs.h>
#include <OgreHlmsManager.h>
#include <Compositor/OgreCompositorManager2.h>
#include <OgreOverlaySystem.h>
#include <OgreOverlayManager.h>

#include <OgreAtmosphereComponent.h>
#include "Atmosphere.h"
#include <fstream>

#if OGRE_USE_SDL2
	#include <SDL_syswm.h>
#endif
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE || OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
	#include "OSX/macUtils.h"
	#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
		#include "System/iOS/iOSUtils.h"
	#else
		#include "System/OSX/OSXUtils.h"
	#endif
#endif
#include "paths.h"
using namespace Ogre;


//**  Global, meh looks worse, too saturated, against PBR
// #define DISABLE_SRGB 1


//  🌟 ctor
//-----------------------------------------------------------------------------------
GraphicsSystem::GraphicsSystem(
		App* app, GameState *gameState,
		String logCfgPath,
		String cachePath,
		String resourcePath,
		String pluginsPath,
		ColourValue backgroundColour ) :
	pApp(app),
	BaseSystem( gameState ),
	mLogicSystem( 0 ),
#if OGRE_USE_SDL2
	mSdlWindow( 0 ),
	mInputHandler( 0 ),
#endif
	mRoot( 0 ),
	mRenderWindow( 0 ),
	mSceneManager( 0 ),
	mCamera( 0 ),
	// mWorkspace( 0 ),

	mLogCfgFolder( logCfgPath ),  // in/out: ogre.cfg  out: Ogre.log
	mCacheFolder( cachePath ),    // out: shaders etc
	mResourcePath( resourcePath ),  // in: for resources3.cfg only
	mPluginsFolder( pluginsPath ),  // in: ./  same as binary

	mOverlaySystem( 0 ),
	mAccumTimeSinceLastLogicFrame( 0 ),
	mCurrentTransformIdx( 0 ),
	mThreadGameEntityToUpdate( 0 ),
	mThreadWeight( 0 ),

	mQuit( false ),
	mAlwaysAskForConfig( app->pSet->ogre_dialog ),  //**  init cfg
	//  hlms cache files, and debug in shaders/
	mUseHlmsDiskCache( app->pSet->cache_hlms ),
	mUseMicrocodeCache( app->pSet->cache_shaders ),
	mDebugShaders( app->pSet->debug_shaders ),  // works in Debug only
	mDebugProperties( app->pSet->debug_properties ),

	mGrabMouse( app->pSet->mouse_capture ),
	mBackgroundColour( backgroundColour )
{
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
	// Note:  macBundlePath works for iOS too. It's misnamed.
	mResourcePath = macBundlePath() + "/Contents/Resources/";
#elif OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
	mResourcePath = macBundlePath() + "/";
#endif
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
	mPluginsFolder = mResourcePath;
#endif

	/*if( isWriteAccessFolder( mPluginsFolder, "Ogre.log" ) )
		mCacheFolder = mPluginsFolder;
	else
	{
		FileSystemLayer filesystemLayer( OGRE_VERSION_NAME );
		mCacheFolder = filesystemLayer.getWritablePath( "" );
	}*/

	const Args& a = MainEntryPoints::args;  //**
	if (a.has("c") || a.has("cfg"))
		mAlwaysAskForConfig = true;
}

//-----------------------------------------------------------------------------------
GraphicsSystem::~GraphicsSystem()
{
	if( mRoot )
	{
		LogManager::getSingleton().logMessage(
			"WARNING: GraphicsSystem::deinitialize() not called!!!", LML_CRITICAL );
	}
}
//-----------------------------------------------------------------------------------
bool GraphicsSystem::isWriteAccessFolder( const String &folderPath,
											const String &fileToSave )
{
	if( !FileSystemLayer::createDirectory( folderPath ) )
		return false;

	std::ofstream of( (folderPath + fileToSave).c_str(),
						std::ios::out | std::ios::binary | std::ios::app );
	if( !of )
		return false;

	return true;
}

//  🆕 Create
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::initialize( const String &windowTitle )
{
#if OGRE_USE_SDL2
	//if( SDL_Init( SDL_INIT_EVERYTHING ) != 0 )
	if( SDL_Init( SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
					SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS ) != 0 )
	{
		OGRE_EXCEPT( Exception::ERR_INTERNAL_ERROR, "Cannot initialize SDL2!",
						"GraphicsSystem::initialize" );
	}
#endif

	String pluginsPath;
	// only use plugins.cfg if not static
#ifndef OGRE_STATIC_LIB
#if OGRE_DEBUG_MODE && !((OGRE_PLATFORM == OGRE_PLATFORM_APPLE) || (OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS))
	pluginsPath = mPluginsFolder + "plugins_d.cfg";
#else
	pluginsPath = mPluginsFolder + "plugins.cfg";
#endif
#endif

	const String cfgPath = 
#if OGRE_PLATFORM != OGRE_PLATFORM_ANDROID
	#ifdef SR_EDITOR
		mLogCfgFolder + "ogre_ed.cfg";
	#else
		mLogCfgFolder + "ogre.cfg";
	#endif
#else
		"";
#endif
#ifndef SR_EDITOR
	int plr = pApp->pSet->net_local_plr;
#endif

	const AbiCookie abiCookie = generateAbiCookie();
	mRoot = OGRE_NEW Root( &abiCookie, pluginsPath, cfgPath,
	#ifdef SR_EDITOR
		mLogCfgFolder + "Ogre_ed.log",
	#else
		mLogCfgFolder + "Ogre"+(plr > 0 ? toStr(plr) : "")+".log",
	#endif
		windowTitle );

	AndroidSystems::registerArchiveFactories();

	mStaticPluginLoader.install( mRoot );


	//  enable sRGB Gamma Conversion mode by default for all renderers,
	//  but still allow to override it via config dialog
	auto itor = mRoot->getAvailableRenderers().begin();
	auto endt = mRoot->getAvailableRenderers().end();
	while( itor != endt )
	{
		RenderSystem *rs = *itor;
		rs->setConfigOption( "sRGB Gamma Conversion", "Yes" );
		++itor;
	}

	//  🪟📄 Config Dialog  --------
	if( mAlwaysAskForConfig || !mRoot->restoreConfig() )
	{
		if( !mRoot->showConfigDialog() )
		{
			mQuit = true;
			return;
		}
	}

	//  disable sRGB Gamma Conversion !
#ifdef DISABLE_SRGB
	itor = mRoot->getAvailableRenderers().begin();
	endt = mRoot->getAvailableRenderers().end();
	while( itor != endt )
	{
		RenderSystem *rs = *itor;
		rs->setConfigOption( "sRGB Gamma Conversion", "No" );
		++itor;
	}
#endif

	//  vulkan, metal-
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
	if(!mRoot->getRenderSystem())
	{
		RenderSystem *renderSystem =
				mRoot->getRenderSystemByName( "Metal Rendering Subsystem" );
		mRoot->setRenderSystem( renderSystem );
	}
#endif
#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID
	if( !mRoot->getRenderSystem() )
	{
		RenderSystem *renderSystem =
			mRoot->getRenderSystemByName( "Vulkan Rendering Subsystem" );
		mRoot->setRenderSystem( renderSystem );
	}
#endif


	//  🌟 Root init  --------
	mRoot->initialise( false, windowTitle );

	ConfigOptionMap& cfgOpts = mRoot->getRenderSystem()->getConfigOptions();

	int width   = 1280;
	int height  = 720;

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
	{
		Vector2 screenRes = iOSUtils::getScreenResolutionInPoints();
		width = static_cast<int>( screenRes.x );
		height = static_cast<int>( screenRes.y );
	}
#endif

	//  get res x y
	ConfigOptionMap::iterator opt = cfgOpts.find( "Video Mode" );
	if( opt != cfgOpts.end() && !opt->second.currentValue.empty() )
	{
		auto cur = opt->second.currentValue;
		const auto start = cur.find_first_of("012356789");
		auto widthEnd = cur.find(' ', start);
		auto heightEnd = cur.find(' ', widthEnd+3);

		width  = StringConverter::parseInt( cur.substr( 0, widthEnd ) );
		height = StringConverter::parseInt(
			cur.substr( widthEnd + 3, heightEnd ) );
	}

	//  pos x y
	NameValuePairList params;
	bool fullscreen = StringConverter::parseBool( cfgOpts["Full Screen"].currentValue );
#if OGRE_USE_SDL2
	int screen = 0;
	int posX = SDL_WINDOWPOS_CENTERED_DISPLAY(screen);
	int posY = SDL_WINDOWPOS_CENTERED_DISPLAY(screen);

	if (fullscreen)
	{
		posX = SDL_WINDOWPOS_UNDEFINED_DISPLAY(screen);
		posY = SDL_WINDOWPOS_UNDEFINED_DISPLAY(screen);
	}

	//  🪟 SDL Window  --------
	mSdlWindow = SDL_CreateWindow(
		windowTitle.c_str(),    // window title
		posX,     // initial x position
		posY,     // initial y position
		width,    // width, in pixels
		height,   // height, in pixels
		SDL_WINDOW_SHOWN | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0) | SDL_WINDOW_RESIZABLE );

	//  Get the native whnd
	SDL_SysWMinfo wmInfo;
	SDL_VERSION( &wmInfo.version );

	if( SDL_GetWindowWMInfo( mSdlWindow, &wmInfo ) == SDL_FALSE )
	{
		OGRE_EXCEPT( Exception::ERR_INTERNAL_ERROR,
						"Couldn't get WM Info! (SDL2)",
						"GraphicsSystem::initialize" );
	}

	String winHandle;
	switch( wmInfo.subsystem )
	{
	#if defined(SDL_VIDEO_DRIVER_WINDOWS)
	case SDL_SYSWM_WINDOWS:
		// Windows code
		winHandle = StringConverter::toString( (uintptr_t)wmInfo.info.win.window );
		break;
	#endif
	#if defined(SDL_VIDEO_DRIVER_WINRT)
	case SDL_SYSWM_WINRT:
		// Windows code
		winHandle = StringConverter::toString( (uintptr_t)wmInfo.info.winrt.window );
		break;
	#endif
	#if defined(SDL_VIDEO_DRIVER_COCOA)
	case SDL_SYSWM_COCOA:
		winHandle  = StringConverter::toString(WindowContentViewHandle(wmInfo));
		break;
	#endif
	#if defined(SDL_VIDEO_DRIVER_X11)
	case SDL_SYSWM_X11:
		winHandle = StringConverter::toString( (uintptr_t)wmInfo.info.x11.window );
		params.insert( std::make_pair(
			"SDL2x11", StringConverter::toString( (uintptr_t)&wmInfo.info.x11 ) ) );
		break;
	#endif
	default:
		OGRE_EXCEPT( Exception::ERR_NOT_IMPLEMENTED,
						"Unexpected WM! (SDL2)",
						"GraphicsSystem::initialize" );
		break;
	}

	//  🪟 params  --------
	#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32 || OGRE_PLATFORM == OGRE_PLATFORM_WINRT
		params.insert( std::make_pair("externalWindowHandle",  winHandle) );
	#else
		params.insert( std::make_pair("parentWindowHandle",  winHandle) );
	#endif
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_ANDROID
		params.insert( std::make_pair(
			"ANativeWindow",
			StringConverter::toString( (uintptr_t)AndroidSystems::getNativeWindow() ) ) );
#endif

	params.insert( std::make_pair("title", windowTitle) );
	params.insert( std::make_pair("gamma", cfgOpts["sRGB Gamma Conversion"].currentValue) );
	// params.insert( std::make_pair("gamma", "true") );
	if( cfgOpts.find( "VSync Method" ) != cfgOpts.end() )
		params.insert( std::make_pair( "vsync_method", cfgOpts["VSync Method"].currentValue ) );
	params.insert( std::make_pair("FSAA", cfgOpts["FSAA"].currentValue) );
	params.insert( std::make_pair("vsync", cfgOpts["VSync"].currentValue) );
	params.insert( std::make_pair("reverse_depth", "Yes" ) );

	initMiscParamsListener( params );

	//  🪟 Render
	mRenderWindow = Root::getSingleton().createRenderWindow(
		windowTitle, width, height, fullscreen, &params );

	mOverlaySystem = OGRE_NEW v1::OverlaySystem();


	//  📄 Resources  --------
#ifdef DISABLE_SRGB
	auto* rndSys = mRoot->getRenderSystem();
	auto* texMgr = rndSys->getTextureGpuManager();
	texMgr->mIgnoreSRgbPreference = false;
#endif

	setupResources();
	loadResources();
	chooseSceneManager();

	createCamera();  // camera 🎥
	// mWorkspace =
	setupCompositor();

#if OGRE_USE_SDL2  // input 🕹️
	mInputHandler = new SdlInputHandler( mSdlWindow, mCurrentGameState,
											mCurrentGameState, mCurrentGameState );
#endif

	BaseSystem::initialize();

#if OGRE_PROFILING
	Profiler::getSingleton().setEnabled( true );
#if OGRE_PROFILING == OGRE_PROFILING_INTERNAL
	Profiler::getSingleton().endProfile( "" );
#endif
#if OGRE_PROFILING == OGRE_PROFILING_INTERNAL_OFFLINE
	Profiler::getSingleton().getOfflineProfiler().setDumpPathsOnShutdown(
				mWriteAccessFolder + "ProfilePerFrame",
				mWriteAccessFolder + "ProfileAccum" );
#endif
#endif
}


//  💥 destroy
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::deinitialize()
{
	BaseSystem::deinitialize();

	saveTextureCache();
	saveHlmsDiskCache();

	if( mSceneManager )
	{
		AtmosphereComponent *atmosphere = mSceneManager->getAtmosphereRaw();
		OGRE_DELETE atmosphere;

		mSceneManager->removeRenderQueueListener( mOverlaySystem );
	}

	OGRE_DELETE mOverlaySystem;
	mOverlaySystem = 0;

#if OGRE_USE_SDL2
	delete mInputHandler;
	mInputHandler = 0;
#endif

	OGRE_DELETE mRoot;
	mRoot = 0;

#if OGRE_USE_SDL2
	if( mSdlWindow )
	{
		// Restore desktop resolution on exit
		SDL_SetWindowFullscreen( mSdlWindow, 0 );
		SDL_DestroyWindow( mSdlWindow );
		mSdlWindow = 0;
	}

	SDL_Quit();
#endif
}


//  💫 Update
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::update( float timeSinceLast )
{
	WindowEventUtilities::messagePump();

#if OGRE_USE_SDL2
	SDL_Event evt;
	while( SDL_PollEvent( &evt ) )
	{
		switch( evt.type )
		{
		case SDL_WINDOWEVENT:
			handleWindowEvent( evt );
			break;
		case SDL_QUIT:
			mQuit = true;
			break;
		default:
			break;
		}

		mInputHandler->_handleSdlEvents( evt );
	}
#endif

	BaseSystem::update( timeSinceLast );

	// renders inactive, but doesn't leak GPU MEM  // fixme?
	// if( mRenderWindow->isVisible() )
		mQuit |= !mRoot->renderOneFrame();

	mAccumTimeSinceLastLogicFrame += timeSinceLast;

	//SDL_SetWindowPosition( mSdlWindow, 0, 0 );
	/*SDL_Rect rect;
	SDL_GetDisplayBounds( 0, &rect );
	SDL_GetDisplayBounds( 0, &rect );*/
}


//  wnd event
//--------------------------------------------------------------------------------------------------------------------------------
#if OGRE_USE_SDL2
void GraphicsSystem::handleWindowEvent( const SDL_Event& evt )
{
	switch( evt.window.event )
	{
		/*case SDL_WINDOWEVENT_MAXIMIZED:
			SDL_SetWindowBordered( mSdlWindow, SDL_FALSE );
			break;
		case SDL_WINDOWEVENT_MINIMIZED:
		case SDL_WINDOWEVENT_RESTORED:
			SDL_SetWindowBordered( mSdlWindow, SDL_TRUE );
			break;*/
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			int w,h;
			SDL_GetWindowSize( mSdlWindow, &w, &h );
#if OGRE_PLATFORM == OGRE_PLATFORM_LINUX
			mRenderWindow->requestResolution( w, h );
#endif
			mRenderWindow->windowMovedOrResized();
			break;
		case SDL_WINDOWEVENT_RESIZED:
#if OGRE_PLATFORM == OGRE_PLATFORM_LINUX
			mRenderWindow->requestResolution( evt.window.data1, evt.window.data2 );
#endif
			mRenderWindow->windowMovedOrResized();
			break;
		case SDL_WINDOWEVENT_CLOSE:
			break;
	case SDL_WINDOWEVENT_SHOWN:
		mRenderWindow->_setVisible( true );
		break;
	case SDL_WINDOWEVENT_HIDDEN:
		mRenderWindow->_setVisible( false );
		break;
	case SDL_WINDOWEVENT_FOCUS_GAINED:
		mRenderWindow->setFocused( true );
		break;
	case SDL_WINDOWEVENT_FOCUS_LOST:
		mRenderWindow->setFocused( false );
		break;
	}
}
#endif


//  📄 resources
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::addResourceLocation( const String &archName, const String &typeName,
											const String &secName )
{
#if (OGRE_PLATFORM == OGRE_PLATFORM_APPLE) || (OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS)
	// OS X does not set the working directory relative to the app,
	// In order to make things portable on OS X we need to provide
	// the loading with it's own bundle path location
	ResourceGroupManager::getSingleton().addResourceLocation(
				String( macBundlePath() + "/" + archName ), typeName, secName );
#else
	ResourceGroupManager::getSingleton().addResourceLocation(
				archName, typeName, secName);
#endif
}

//  📄 setup resources
void GraphicsSystem::setupResources()
{
	// Load resource paths from config file
	ConfigFile cf;  // PATHS::GameConfigDir() +"/"+
	cf.load( AndroidSystems::openFile( mResourcePath + "resources3.cfg" ) );

	// Go through all sections & settings in the file
	ConfigFile::SectionIterator seci = cf.getSectionIterator();

	String secName, typeName, archName;
	while (seci.hasMoreElements())
	{
		secName = seci.peekNextKey();
		ConfigFile::SettingsMultiMap *settings = seci.getNext();

		if (secName != "Hlms")
		{
			ConfigFile::SettingsMultiMap::iterator i;
			for (i = settings->begin(); i != settings->end(); ++i)
			{
				typeName = i->first;
				archName = PATHS::Data() + "/" + i->second;
				addResourceLocation( archName, typeName, secName );
				// std::cout << "  ADD:   "<< archName <<"  "<< typeName <<"  "<< secName << std::endl;
			}
		}
	}
}

//  📄 load resources
void GraphicsSystem::loadResources()
{
	registerHlms();

	loadTextureCache();
	loadHlmsDiskCache();

	// Initialise, parse scripts etc
	ResourceGroupManager::getSingleton().initialiseAllResourceGroups( true );

	// Initialize resources for LTC area lights and accurate specular reflections (IBL)
	Hlms *hlms = mRoot->getHlmsManager()->getHlms( HLMS_PBS );
	OGRE_ASSERT_HIGH( dynamic_cast<HlmsPbs2*>( hlms ) );
	HlmsPbs2 *hlmsPbs2 = static_cast<HlmsPbs2*>( hlms );
	try
	{
		hlmsPbs2->loadLtcMatrix();
	}
	catch( FileNotFoundException &e )
	{
		LogManager::getSingleton().logMessage( e.getFullDescription(), LML_CRITICAL );
		LogManager::getSingleton().logMessage(
			"WARNING: LTC matrix textures could not be loaded. Accurate specular IBL reflections "
			"and LTC area lights won't be available or may not function properly!",
			LML_CRITICAL );
	}
}


//  🌠 Hlms shaders
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::registerHlms()
{
	String rootDir = PATHS::Data()+"/";

	//  At this point rootHlmsFolder should be a valid path to the Hlms data folder

	//  For retrieval of the paths to the different folders needed
	String mainPath;  StringVector paths;

	ArchiveManager& mgr = ArchiveManager::getSingleton();
	const String& type = getMediaReadArchiveType();
	
	{	//  Create & Register HlmsUnlit2  ----
		//  Get the path to all the subdirectories used by HlmsUnlit2
		HlmsUnlit::getDefaultPaths( mainPath, paths );
		Archive* ar = mgr.load( rootDir + mainPath, type, true );

		ArchiveVec dirs;
		for (auto it = paths.begin(); it != paths.end(); ++it)
		{
			Archive* lib = mgr.load( rootDir + *it, type, true );
			dirs.push_back( lib );
		}
		hlmsUnlit2 = OGRE_NEW HlmsUnlit2( ar, &dirs );
		Root::getSingleton().getHlmsManager()->registerHlms( hlmsUnlit2 );
		hlmsUnlit2->setDebugOutputPath(mDebugShaders, mDebugProperties, PATHS::ShadersDir()+"/");
	}

	{	//  Create & Register HlmsPbs2  ----
		HlmsPbs2::getDefaultPaths( mainPath, paths );
		Archive* ar = mgr.load( rootDir + mainPath, type, true );
		ArchiveVec dirs;
		for (auto it = paths.begin(); it != paths.end(); ++it)
		{
			Archive* lib = mgr.load( rootDir + *it, type, true );
			dirs.push_back( lib );
		}
		hlmsPbs2 = OGRE_NEW HlmsPbs2( pApp, ar, &dirs );
		Root::getSingleton().getHlmsManager()->registerHlms( hlmsPbs2 );
		hlmsPbs2->setDebugOutputPath(mDebugShaders, mDebugProperties, PATHS::ShadersDir()+"/");
	}


	//  DX 11-
	RenderSystem *rs = mRoot->getRenderSystem();
	if( rs->getName() == "Direct3D11 Rendering Subsystem" )
	{
		//  Set lower limits 512kb instead of the default 4MB per Hlms in D3D 11.0
		//  and below to avoid saturating AMD's discard limit (8MB) or
		//  saturate the PCIE bus in some low end machines.
		bool support;
		rs->getCustomAttribute(
			"MapNoOverwriteOnDynamicBufferSRV", &support );

		if( !support )
		{
			hlmsPbs2->setTextureBufferDefaultSize( 512 * 1024 );
			hlmsUnlit2->setTextureBufferDefaultSize( 512 * 1024 );
		}
	}
}


//  🏞️ scene mgr
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::chooseSceneManager()
{
#if OGRE_DEBUG_MODE >= OGRE_DEBUG_HIGH
	// const size_t numThreads = 1;  //  Debugging multithreaded code is a PITA, disable it, or ..
	const size_t numThreads = std::max<size_t>( 1, PlatformInformation::getNumLogicalCores() );  //** Debug with high Fps
#else
	//getNumLogicalCores() may return 0 if couldn't detect
	const size_t numThreads = std::max<size_t>( 1, PlatformInformation::getNumLogicalCores() );
#endif
	// Create the SceneManager, in this case a generic one
	mSceneManager = mRoot->createSceneManager( ST_GENERIC,  //ST_EXTERIOR_FAR?
												numThreads,
												"ExampleSMInstance" );

	mSceneManager->addRenderQueueListener( mOverlaySystem );
	mSceneManager->getRenderQueue()->setSortRenderQueue(
				v1::OverlayManager::getSingleton().mDefaultRenderQueueId,
				RenderQueue::StableSort );

	//Set sane defaults for proper shadow mapping
	mSceneManager->setShadowDirectionalLightExtrusionDistance( 1000.0f );  //** par! 500..1500  todo: ?
	mSceneManager->setShadowFarDistance( 1000.0f );
}


//  🎥 Camera
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::createCamera()
{
#ifndef SR_EDITOR
	return;  //! game does not
#endif	// ed
	mCamera = mSceneManager->createCamera( "Main Camera" );

	// Position it at 500 in Z direction
	mCamera->setPosition( Vector3( 0, 5, 15 ) );
	// Look back along -Z
	mCamera->lookAt( Vector3( 0, 0, 0 ) );
	mCamera->setNearClipDistance( 0.2f );
	mCamera->setFarClipDistance( 1000.0f );
	mCamera->setAutoAspectRatio( true );
}

//--------------------------------------------------------------------------------------------------------------------------------
CompositorWorkspace* GraphicsSystem::setupCompositor()
{
	return 0;    
}

//-----------------------------------------------------------------------------------
void GraphicsSystem::initMiscParamsListener( NameValuePairList &params )
{
}

//  🌫️ Atmosphere
//--------------------------------------------------------------------------------------------------------------------------------
void GraphicsSystem::createAtmosphere( Light *sunLight )
{
	AtmosphereComponent *atmosphere = mSceneManager->getAtmosphereRaw();
	OGRE_DELETE atmosphere;

	Atmosphere *atmo = OGRE_NEW Atmosphere(
		mRoot->getRenderSystem()->getVaoManager(), mSceneManager );
}

//-----------------------------------------------------------------------------------
void GraphicsSystem::setAlwaysAskForConfig( bool alwaysAskForConfig )
{
	mAlwaysAskForConfig = alwaysAskForConfig;
}

//-----------------------------------------------------------------------------------
const char *GraphicsSystem::getMediaReadArchiveType() const
{
#if OGRE_PLATFORM != OGRE_PLATFORM_ANDROID
	return "FileSystem";
#else
	return "APKFileSystem";
#endif
}

/*
//-----------------------------------------------------------------------------------
void GraphicsSystem::stopCompositor()
{
	if( mWorkspace )
	{
		LogO("#### stop Compositor");
		CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
		compositorManager->removeWorkspace( mWorkspace );
		mWorkspace = 0;
	}
}

//-----------------------------------------------------------------------------------
void GraphicsSystem::restartCompositor()
{
	LogO("#### restart Compositor");
	stopCompositor();
	mWorkspace = setupCompositor();
}
*/