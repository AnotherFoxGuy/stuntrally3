#include "pch.h"

#if 0  // OLD reference  --------
#include "Def_Str.h"
#include "BaseApp.h"
#include "paths.h"
#include "settings.h"

// #include "Localization.h"
// #include "SplitScreen.h"
// #include "Compositor.h"

#include "CarModel.h"
#include "FollowCamera.h"
#include "GraphicsSystem.h"

#include <MyGUI_Prerequest.h>
#include <MyGUI.h>
#include <MyGUI_Ogre2Platform.h>

#include <OgreLogManager.h>
#include <OgreTimer.h>
#include <OgreOverlayManager.h>
#include <OgreWindow.h>

// #include "PointerFix.h"
#include "ICSInputControlSystem.h"
using namespace Ogre;
using namespace MyGUI;


//  OLD Run
//-------------------------------------------------------------------------------------
void BaseApp::Run(bool showDialog)
{
	mShowDialog = showDialog;
	if (!setup())
		return;
	
	if (!pSet->limit_fps)
		mRoot->startRendering();  // default
	else
	{	Ogre::Timer tim;
		while (1)
		{
			WindowEventUtilities::messagePump();
			long min_fps = 1000000.0 / std::max(10.f, pSet->limit_fps_val);  // todo?
			if (tim.getMicroseconds() > min_fps)
			{
				tim.reset();
				if (!mRoot->renderOneFrame())
					break;
			}else
			if (pSet->limit_sleep >= 0)
				boost::this_thread::sleep(boost::posix_time::milliseconds(pSet->limit_sleep));
	}	}

	destroyScene();
}


//  OLD config
//-------------------------------------------------------------------------------------
bool BaseApp::configure()
{
	RenderSystem* rs;
	if (rs = mRoot->getRenderSystemByName(pSet->rendersystem))
	{
		mRoot->setRenderSystem(rs);
	}else
	{	LogO("RenderSystem '" + pSet->rendersystem + "' is not available. Exiting.");
		return false;
	}
	if (pSet->rendersystem == "OpenGL Rendering Subsystem")
		mRoot->getRenderSystem()->setConfigOption("RTT Preferred Mode", pSet->buffer);

	mRoot->initialise(false);

	Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE;
	if (SDL_WasInit(flags) == 0)
	{
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
		if (SDL_Init(flags) != 0)
			throw std::runtime_error("Could not initialize SDL! " + std::string(SDL_GetError()));
	}

	//  Enable joystick events  // todo:
	SDL_JoystickEventState(SDL_ENABLE);
	//  Open all available joysticks.  TODO: open them when they are required
	for (int i=0; i<SDL_NumJoysticks(); ++i)
	{
		SDL_Joystick* js = SDL_JoystickOpen(i);
		if (js)
		{
			mJoysticks.push_back(js);
			const char* s = SDL_JoystickName(js);
			int axes = SDL_JoystickNumAxes(js);
			int btns = SDL_JoystickNumButtons(js);
			//SDL_JoystickNumBalls SDL_JoystickNumHats
			LogO(String("<Joystick> name: ")+s+"  axes: "+toStr(axes)+"  buttons: "+toStr(btns));
		}
	}
	SDL_StartTextInput();

	NameValuePairList params;
	params.insert(std::make_pair("title", "Stunt Rally"));
	params.insert(std::make_pair("FSAA", toStr(pSet->fsaa)));
	params.insert(std::make_pair("vsync", pSet->vsync ? "true" : "false"));


	int pos_x = SDL_WINDOWPOS_UNDEFINED,
		pos_y = SDL_WINDOWPOS_UNDEFINED;

#if 0  /// 🧰 _Tool_ rearrange window pos for local netw testing
	SDL_Rect screen;
	if (SDL_GetDisplayBounds(/*pSet.screen_id*/0, &screen) != 0)
		LogO("SDL_GetDisplayBounds errror");
		
	if (pSet->net_local_plr <= 0)
	{	pos_x = 0;  pos_y = 0;
	}else
	{	pos_x = screen.w - pSet->windowx;
		pos_y = screen.h - pSet->windowy;
	}
#endif
	/// \todo For multiple monitors, WINDOWPOS_UNDEFINED is not the best idea. Needs a setting which screen to launch on,
	/// then place the window on that screen (derive x&y pos from SDL_GetDisplayBounds)


	//  Create an application window with the following settings:
	mSDLWindow = SDL_CreateWindow(
		"Stunt Rally", pos_x, pos_y, pSet->windowx, pSet->windowy,
		SDL_WINDOW_SHOWN | (pSet->fullscreen ? SDL_WINDOW_FULLSCREEN : 0) | SDL_WINDOW_RESIZABLE);

	SFO::SDLWindowHelper helper(mSDLWindow, pSet->windowx, pSet->windowy, "Stunt Rally", pSet->fullscreen, params);
	helper.setWindowIcon("stuntrally.png");  // todo:
	mWindow = helper.getWindow();

	return true;
}


//  Setup
//-------------------------------------------------------------------------------------
bool BaseApp::setup()
{
	Ogre::Timer ti,ti2;
	LogO("*** start setup ***");
	
	if (pSet->rendersystem == "Default")
	{
		#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
		pSet->rendersystem = "Direct3D9 Rendering Subsystem";
		#else
		pSet->rendersystem = "OpenGL Rendering Subsystem";
		#endif
	}

	#ifdef _DEBUG
	#define D_SUFFIX ""  // "_d"
	#else
	#define D_SUFFIX ""
	#endif

	//  when show ogre dialog is on, load both rendersystems so user can select
	if (pSet->ogre_dialog)
	{
		mRoot->loadPlugin(PATHS::OgrePluginDir() + "/RenderSystem_GL" + D_SUFFIX);
		#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
		mRoot->loadPlugin(PATHS::OgrePluginDir() + "/RenderSystem_Direct3D9" + D_SUFFIX);
		#endif
	}else{
		if (pSet->rendersystem == "OpenGL Rendering Subsystem")
			mRoot->loadPlugin(PATHS::OgrePluginDir() + "/RenderSystem_GL" + D_SUFFIX);
		else if (pSet->rendersystem == "Direct3D9 Rendering Subsystem")
			mRoot->loadPlugin(PATHS::OgrePluginDir() + "/RenderSystem_Direct3D9" + D_SUFFIX);
	}

	mRoot->loadPlugin(PATHS::OgrePluginDir() + "/Plugin_ParticleFX" + D_SUFFIX);
#if defined(OGRE_VERSION) && OGRE_VERSION >= 0x10B00
    //mRoot->loadPlugin(PATHS::OgrePluginDir() + "/Codec_STBI" + D_SUFFIX);  // only png
    mRoot->loadPlugin(PATHS::OgrePluginDir() + "/Codec_FreeImage" + D_SUFFIX);  // for jpg screenshots
#endif


	#ifdef _DEBUG
	LogManager::getSingleton().setMinLogLevel(LML_TRIVIAL);  // all
	#endif

	setupResources();

	if (!configure())
		return false;


	mSplitMgr = new SplitScr(mSceneMgr, mWindow, pSet);

	createViewports();  // calls mSplitMgr->Align();

	TextureManager::getSingleton().setDefaultNumMipmaps(5);

		LogO(String(":::* Time setup vp: ") + fToStr(ti.getMilliseconds(),0,3) + " ms");  ti.reset();


	loadResources();

		LogO(String(":::* Time resources: ") + fToStr(ti.getMilliseconds(),0,3) + " ms");  ti.reset();  

	LogO(String(":::* Time setup total: ") + fToStr(ti2.getMilliseconds(),0,3) + " ms");
	
	return true;
}


// todo:  Resources
//-------------------------------------------------------------------------------------
void BaseApp::setupResources()
{
	// Load resource paths from config file
	ConfigFile cf;
	std::string s = PATHS::GameConfigDir() +
		(pSet->tex_size > 0 ? "/resources.cfg" : "/resources_s.cfg");
	cf.load(s);

	// Go through all sections & settings in the file
	ConfigFile::SectionIterator seci = cf.getSectionIterator();

	String secName, typeName, archName;
	while (seci.hasMoreElements())
	{
		secName = seci.peekNextKey();
		ConfigFile::SettingsMultiMap *settings = seci.getNext();
		ConfigFile::SettingsMultiMap::iterator i;
		for (i = settings->begin(); i != settings->end(); ++i)
		{
			typeName = i->first;
			archName = i->second;
			ResourceGroupManager::getSingleton().addResourceLocation(
				PATHS::Data() + "/" + archName, typeName, secName);
	}	}
}

#endif
