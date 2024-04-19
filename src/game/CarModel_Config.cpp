#include "pch.h"
#include "par.h"
#include "dbl.h"
#include "cardefs.h"
#include "Def_Str.h"
#include "mathvector.h"
#include "game.h"
#include "CGame.h"
#include "CGui.h"
#include "CScene.h"
#include "SceneXml.h"
#include "CarModel.h"
#include "FollowCamera.h"
#include "gameclient.hpp"

using namespace Ogre;
using namespace std;


//  🌟 ctor
//------------------------------------------------------------------------------------------------------
CarModel::CarModel(int index, int colorId,
	eCarType type, const string& name,
	Cam* cam1, App* app)

	:pApp(app), cam(cam1)

	,iIndex(index), iColor(colorId % MAX_Vehicles)
	,sDirname(name)
	,cType(type), vType(V_Car)

	,color(0,1,0)
	,sChkMtr("checkpoint_normal")
{
	SetNumWheels(4);
	int i,w;
	for (w = 0; w < MAX_WHEELS; ++w)
	for (int p=0; p < PAR_ALL; ++p)
		par[p][w] = 0;

	for (i=0; i < PAR_BOOST; ++i)  parBoost[i] = 0;
	for (i=0; i < PAR_THRUST*2; ++i)  parThrust[i] = 0;
	parHit = 0;

	qFixWh[0].Rotate(2*PI_d,0,0,1);
	qFixWh[1].Rotate(  PI_d,0,0,1);

	Defaults();

	pSet = app->pSet;
	pGame = app->pGame;
	sc = app->sc;
	mSceneMgr = app->mSceneMgr;
}

void CarModel::SetNumWheels(int n)
{
	numWheels = n;
	whPos.resize(n);  whRadius.resize(n);  whWidth.resize(n);
	whTrail.resize(n);  whTemp.resize(n);
	ndWh.resize(n);  ndWhE.resize(n);  ndBrake.resize(n);
}

void CarModel::Defaults()
{
	int i,w;
	for (i=0; i < 3; ++i)
	{
		driver_view[i] = 0.f;  hood_view[i] = 0.f;  ground_view[i] = 0.f;
		interiorOfs[i] = 0.f;  boostOfs[i] = 0.f;  exhaustPos[i] = 0.f;
	}
	camDist = 1.f;
	for (i=0; i < PAR_THRUST; ++i)
	{
		for (w=0; w<3; ++w)  thrusterOfs[i][w] = 0.f;
		thrusterSizeZ[i] = 0.f;
		sThrusterPar[i] = "";
	}
	fsBrakes.pos.clear();
	fsBrakes.clr = ColourValue(1,0,0);
	fsBrakes.size = 0.2f;

	fsFlares.pos.clear();
	fsFlares.clr = ColourValue(0.98,1,1);
	fsFlares.size = 1.2f;

	bRotFix = false;
	sBoostParName = "Boost";  boostSizeZ = 1.f;

	for (w=0; w < numWheels; ++w)
	{
		whRadius[w] = 0.3f;  whWidth[w] = 0.2f;
		whTemp[w] = 0.f;
	}
	manualExhaustPos = false;  has2exhausts = false;

	maxangle = 26.f;
	for (w=0; w < 2; ++w)
		posSph[w] = Vector3::ZERO;

	matStart = Matrix4::IDENTITY;
	vStartDist = Vector4(0,0,0,0);
}


//  📄 Load CAR
//------------------------------------------------------------------------------------------------------
void CarModel::Load(int startId, bool loop)
{
	//  names for local play
	// if (isGhostTrk())    sDispName = TR("#{Track}");
	// else if (isGhost())  sDispName = TR("#{Ghost}");
	// else if (eType == CT_LOCAL)
	// 	sDispName = TR("#{Player}") + toStr(iIndex+1);
	

	///  📄 load config .car
	string pathCar;
	bool force = pApp->mClient.get() != 0;  // force orig for newtorked games
	pApp->gui->GetCarPath(&pathCar, 0, 0, sDirname, force);

	LoadConfig(pathCar);

	
	///  🆕 Create CAR (dynamic)
	if (!isGhost())  // ghost has pCar, dont create
	{
		if (startId == -1)  startId = iIndex;
		if (pSet->game.start_order == 1)
		{	//  reverse start order
			int numCars =
				pApp->mClient ? pApp->mClient->getPeerCount()+1 :  // 📡 networked
				pSet->game.local_players;  // 👥 splitscreen
			startId = numCars-1 - startId;
		}
		int i = pSet->game.collis_cars ? startId : 0;  // offset when cars collide

		//  🏁 start pos
		auto st = pApp->scn->sc->GetStart(i, loop);
		auto pos = st.first;  auto rot = st.second;

		if (gPar.carPrv > 0)  // car preview poses
		{	rot = QUATERNION<float>(2.23517e-007, -1.34779e-007, 0.997519, 0.0704623);
		switch (gPar.carPrv)
		{  	//  near, normal
			case 1:  pos = MATHVECTOR<float,3>(-44.794, 106.05, 2.47537);  break;
		  	//  far, for big vehicles:  U8 MO TW
			case 2:  pos = MATHVECTOR<float,3>(-43.7288, 105.017, 2.47537);  break;
			//  close, for small  BE-
			case 3:  pos = MATHVECTOR<float,3>(-45.5525, 106.544, 2.47537);  break;
		}	}
		
		vStartPos = Vector3(pos[0], pos[2], -pos[1]);
		if (pSet->game.track_reversed)
		{	rot.Rotate(PI_d, 0,0,1);  rot[0] = -rot[0];  rot[1] = -rot[1];  }

		pCar = pGame->LoadCar(pathCar, sDirname, pos, rot, true, cType == CT_REMOTE, iIndex);

		if (!pCar)  LogO("Error: Creating CAR: " + sDirname + "  path: " + pathCar);
		else  pCar->pCarM = this;
	}
}


//------------------------------------------------------------------------------------------------------
///  📄 Load .car
//------------------------------------------------------------------------------------------------------
static void ConvertV2to1(float & x, float & y, float & z)
{
	float tx = x, ty = y, tz = z;
	x = ty;  y = -tx;  z = tz;
}
void CarModel::LoadConfig(const string & pathCar)
{
	Defaults();

	///  load  -----
	CONFIGFILE cf;
	if (!cf.Load(pathCar))
	{  LogO("Error: CarModel Can't load .car: "+pathCar);  return;  }


	//  vehicle type
	vType = V_Car;
	string type;
	if (!cf.GetParam("type", type))	vType = V_Car;
	else if (type == "spaceship")	vType = V_Spaceship;
	else if (type == "sphere")		vType = V_Sphere;
	else if (type == "hovercar")	vType = V_Hovercar;
	else if (type == "drone")		vType = V_Drone;
	else if (type == "hovercraft")	vType = V_Hovercraft;


	//  wheel count
	int nw = 0;
	cf.GetParam("wheels", nw);
	if (nw >= 2 && nw <= MAX_WHEELS)
		SetNumWheels(nw);


	//-  custom interior model offset
	cf.GetParam("model_ofs.interior-x", interiorOfs[0]);
	cf.GetParam("model_ofs.interior-y", interiorOfs[1]);
	cf.GetParam("model_ofs.interior-z", interiorOfs[2]);
	cf.GetParam("model_ofs.rot_fix", bRotFix);

	//~  💨 boost offset
	cf.GetParam("model_ofs.boost-x", boostOfs[0]);
	cf.GetParam("model_ofs.boost-y", boostOfs[1]);
	cf.GetParam("model_ofs.boost-z", boostOfs[2]);
	cf.GetParam("model_ofs.boost-size-z", boostSizeZ);
	cf.GetParam("model_ofs.boost-name", sBoostParName);
	
	//  🔥 thruster  spaceship hover  max 4 pairs
	int i;
	for (i=0; i < PAR_THRUST; ++i)
	{
		string s = "model_ofs.thrust";
		if (i > 0)  s += toStr(i);
		cf.GetParam(s+"-x", thrusterOfs[i][0]);
		cf.GetParam(s+"-y", thrusterOfs[i][1]);
		cf.GetParam(s+"-z", thrusterOfs[i][2]);
		cf.GetParam(s+"-size-z", thrusterSizeZ[i]);
		cf.GetParam(s+"-name", sThrusterPar[i]);
	}
	

	//~  🔴 brake flares
	cf.GetParam("flares.lights", numLights);
	float pos[3];
	for (int n=0; n < 2; ++n)
	{
		FlareSet& flr = n ? fsBrakes : fsFlares;
		const string s = n ? "brake" : "front";
		bool ok = true;  i=0;
		while (ok)
		{
			ok = cf.GetParam("flares."+s+"-pos"+toStr(i), pos);
			++i;
			if (ok)  flr.pos.push_back( bRotFix ?
				Vector3(-pos[0], pos[2],pos[1]) :
				Vector3(-pos[1],-pos[2],pos[0]) );
		}
		cf.GetParam("flares."+s+"-color", pos);
		flr.clr = ColourValue(pos[0],pos[1],pos[2]);
		cf.GetParam("flares."+s+"-size", flr.size);
	}
	
	//-  custom exhaust pos for boost particles
	if (cf.GetParam("model_ofs.exhaust-x", exhaustPos[0]))
	{
		manualExhaustPos = true;
		cf.GetParam("model_ofs.exhaust-y", exhaustPos[1]);
		cf.GetParam("model_ofs.exhaust-z", exhaustPos[2]);
	}else
		manualExhaustPos = false;
	if (!cf.GetParam("model_ofs.exhaust-mirror-second", has2exhausts))
		has2exhausts = false;


	//- 🎥 load cameras pos
	cf.GetParamE("driver.view-position", pos);
	driver_view[0]=pos[1]; driver_view[1]=-pos[0]; driver_view[2]=pos[2];
	
	cf.GetParamE("driver.hood-position", pos);
	hood_view[0]=pos[1]; hood_view[1]=-pos[0]; hood_view[2]=pos[2];

	if (cf.GetParam("driver.ground-position", pos))
	{	ground_view[0]=pos[1]; ground_view[1]=-pos[0]; ground_view[2]=pos[2];  }
	else
	{	ground_view[0]=0.f; ground_view[1]=1.6; ground_view[2]=0.4f;  }

	cf.GetParam("driver.dist", camDist);


	//  ⚫ tire params
	float val;
	bool both = cf.GetParam("tire-both.radius", val);

	int axles = std::max(2, numWheels/2);
	for (i=0; i < axles; ++i)
	{
		WHEEL_POSITION wl, wr;  string pos;
		CARDYNAMICS::GetWPosStr(i, numWheels, wl, wr, pos);
		if (both)  pos = "both";
		
		float radius;
		cf.GetParamE("tire-"+pos+".radius", radius);
		whRadius[wl] = radius;  whRadius[wr] = radius;
		
		float width = 0.2f;
		cf.GetParam("tire-"+pos+".width-trail", width);
		whWidth[wl] = width;  whWidth[wr] = width;
	}
	
	//  ⚫ wheel pos
	//  for track's ghost or garage view
	int version = 2;
	cf.GetParam("version", version);
	for (i = 0; i < numWheels; ++i)
	{
		string sPos = sCfgWh[i];
		float pos[3];  MATHVECTOR<float,3> vec;

		cf.GetParamE("wheel-"+sPos+".position", pos);
		if (version == 2)  ConvertV2to1(pos[0],pos[1],pos[2]);
		vec.Set(pos[0],pos[1], pos[2]);
		
		whPos[i] = vec;
	}
	
	//  steer angle
	maxangle = 26.f;
	cf.GetParamE("steering.max-angle", maxangle);
	maxangle *= pGame->GetSteerRange();
}
