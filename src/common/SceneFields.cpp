#include "pch.h"
#include "RenderConst.h"
#include "Def_Str.h"
#include "paths.h"
#include "settings.h"
#include "App.h"
#ifdef SR_EDITOR
	#include "CGui.h"
	#include "Road.h"
#else
	#include "game.h"
#endif

#include "SceneXml.h"
#include "CScene.h"
#include "CData.h"
#include "FluidsXml.h"
#include "PresetsXml.h"
#include "SceneClasses.h"

#include "Axes.h"
#include "BtOgreGP.h"
#include "ShapeData.h"
#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <LinearMath/btSerializer.h>

#include <OgreCommon.h>
#include <OgreItem.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreCamera.h>
using namespace Ogre;
using namespace std;



///  🆕🎆 Create Fields
//-------------------------------------------------------------------------------------------------------
void App::CreateFields()
{
	for (int i=0; i < scn->sc->fields.size(); ++i)
		CreateField(i);
}

void App::CreateField(int i)
{
	SField& f = scn->sc->fields[i];
	// String s = toStr(i);  // counter for names
	/*const PCollect* col = scn->data->pre->GetCollect(c.name);
	if (!col)
	{
		LogO(String("Collect not in presets name: ") + c.name);
		col = scn->data->pre->GetCollect("gem1b");  // use default 1st
	}*/

	//  add to ogre 🟢
	try 
	{	f.it = mSceneMgr->createItem("cube.mesh");  //-
		f.it->setDatablockOrMaterialName(
			f.type == TF_Accel ? "checkpoint_lap" :  // cyan
			f.type == TF_Gravity ? "checkpoint_lastlap" :  // yellow
			f.type == TF_Teleport ? "checkpoint_finish" :  // red
				"checkpoint_normal"  // green
		);
		// c.it->setName("cE"+s);
		// SetTexWrap(c.it);
		f.it->setCastShadows(0);
	}
	catch (Exception& e)
	{
		LogO(String("Create field fail: ") + e.what());
		return;
	}
	f.it->setRenderQueueGroup( RQG_AlphaVegObj );

	//  pos, scale  ----
	f.nd = mSceneMgr->getRootSceneNode(SCENE_DYNAMIC)->createChildSceneNode();
	f.nd->setPosition(f.pos);
	f.nd->setScale(f.size);
	f.nd->attachObject(f.it);  f.it->setVisibilityFlags(RV_Objects);  //?

	//  particles
	f.ps = mSceneMgr->createParticleSystem(
		f.type == TF_Accel ? "FieldAccel" :  // cyan
		f.type == TF_Gravity ? "FieldGravity" :  // yellow
		f.type == TF_Teleport ? "FieldTeleport" :  // red
			"FieldDamp"  // green
	);  // todo par?
	f.ps->setVisibilityFlags(RV_Particles);

	f.UpdEmitter();
	f.ps->getEmitter(0)->setEmissionRate(200);  // todo par ..

	f.nd->attachObject(f.ps);
	f.nd->_getFullTransformUpdated();  //?

#ifdef SR_EDITOR  // ed hide
	bool vis = edMode == ED_Fields && !bMoveCam;
	// c.nd->setVisible(vis);  //?
#endif


	//  add to bullet world (in game)
	#ifndef SR_EDITOR
	{
		///  🏢 static  . . . . . . . . . . . . 
		btCollisionShape* bshp = 0;
		bshp = new btBoxShape(btVector3(f.size.x,f.size.z,f.size.y) * 0.5f);  //par

		size_t id = SU_Collect + i;
		bshp->setUserPointer((void*)id);
		bshp->setMargin(0.1f); //

		btCollisionObject* bco = new btCollisionObject();
		btTransform tr;  tr.setIdentity();  tr.setOrigin(btVector3(f.pos.x,-f.pos.z,f.pos.y));
		bco->setActivationState(DISABLE_SIMULATION);
		bco->setCollisionShape(bshp);	bco->setWorldTransform(tr);

		bco->setCollisionFlags(bco->getCollisionFlags() |
			btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);

		bco->setUserPointer(new ShapeData(ST_Field, 0, 0, 0, 0, &f));  /// *
		pGame->collision.world->addCollisionObject(bco);
		// pGame->collision.shapes.push_back(bshp);  //?
		f.co = bco;
	}
	#endif
}


///  💥🎆 Destroy
//-------------------------------------------------------------------------------------------------------
void App::DestroyFields(bool clear)
{
	for (int i=0; i < scn->sc->fields.size(); ++i)
		DestroyField(i);

	if (clear)
		scn->sc->fields.clear();
}

void App::DestroyField(int i)
{
	SField& f = scn->sc->fields[i];
	//  ogre
	if (f.ps)  mSceneMgr->destroyParticleSystem(f.ps);  f.ps = 0;
	if (f.nd)  mSceneMgr->destroySceneNode(f.nd);  f.nd = 0;
	if (f.it)  mSceneMgr->destroyItem(f.it);  f.it = 0;

	//  bullet
	if (f.co)
	{	delete f.co->getCollisionShape();
		#ifdef SR_EDITOR
		world->removeCollisionObject(f.co);
		#else
		pGame->collision.world->removeCollisionObject(f.co);
		#endif
		delete f.co;  f.co = 0;
	}
}


//  👆 Pick
//-------------------------------------------------------------------------------------------------------
#ifdef SR_EDITOR

void App::UpdFldPick()
{
	int flds = scn->sc->fields.size();
	bool bFields = edMode == ED_Fields && !bMoveCam && flds > 0 && iFldCur >= 0;
	if (flds > 0)
		iFldCur = std::min(iFldCur, flds-1);

	if (!boxField.nd)  return;
	boxField.nd->setVisible(bFields);
	if (!bFields)  return;
	
	const SField& f = scn->sc->fields[iFldCur];
	const Aabb& ab = f.nd->getAttachedObject(0)->getLocalAabb();
	Vector3 s = f.size * ab.getSize();  // * sel fld's node aabb

	boxField.nd->setPosition(f.pos);
	// boxFld.nd->setOrientation(rotO);
	boxField.nd->setScale(s);
	boxField.nd->_getFullTransformUpdated();

	UpdTelepEnd();
}

#if 0
void App::PickField()
{
	const auto& objs = scn->sc->objects;
	if (objs.empty())  return;

	iObjCur = -1;
	const MyGUI::IntPoint& mp = MyGUI::InputManager::getInstance().getMousePosition();
	Real mx = Real(mp.left)/mWindow->getWidth(), my = Real(mp.top)/mWindow->getHeight();
	Ray ray = mCamera->getCameraToViewportRay(mx,my);  // 0..1
	const Vector3& pos = mCamera->getDerivedPosition(), dir = ray.getDirection();

	//  query scene (aabbs are enough)
	RaySceneQuery* rq = mSceneMgr->createRayQuery(ray);
	rq->setSortByDistance(true);
	RaySceneQueryResult& res = rq->execute();

	Real distC = 100000.f;
	int io = -1;
	for (auto it = res.begin(); it != res.end(); ++it)
	{
		const String& s = (*it).movable->getName();
		if (StringUtil::startsWith(s,"oE",false))
		{
			// LogO("RAY "+s+" "+fToStr((*it).distance,2,4));
			int i = -1;
			//  find obj with same name
			for (int o=0; o < objs.size(); ++o)
				if (objs[o].it && s == objs[o].it->getName())
				{	i = o;  break;  }
			
			//  pick
			if (i != -1)
			{
				//Aabb ab = objs[i].it->getLocalAabb();
				//ab.getCenter();  ab.getSize();

				//  closest to obj center  // fixme fails..
				const Vector3 posSph = objs[i].nd->getPosition();
				const Vector3 ps = pos - posSph;
				Vector3 crs = ps.crossProduct(dir);
				Real dC = crs.length() / dir.length();

				//if ((*it).distance < dist)  // closest aabb
				if (dC < distC)  // closest center
				{
					io = i;
					//dist = (*it).distance;
					distC = dC;
			}	}
	}	}
	
	if (io != -1)  //  if none picked
	if (iObjCur == -1)
		iObjCur = io;
	
	//rq->clearResults();
	mSceneMgr->destroyQuery(rq);
}
#endif


//  type add +-1
void App::SetFldType(int add)
{
	auto& fld = scn->sc->fields;
	bool out = iFldCur < 0 || iFldCur >= fld.size();
	auto& type = out ? fldNew.type : fld[iFldCur].type;

	int t = (type +TF_All +add) % TF_All;  // add
	if (!out)
		DestroyField(iFldCur);
	type = (FieldTypes)t;
	if (!out)
		CreateField(iFldCur);
}


///  🆕 add new field
void App::AddNewFld()
{
	SField f = fldNew;

	const Vector3& v = scn->road->posHit;
	f.pos = v;
	f.pos.y += fldNew.pos.y;

	auto& fld = scn->sc->fields;
	fld.push_back(f);
	CreateField(fld.size()-1);
}


//  preview model for insert
void App::SetFldNewType(int tnew)
{
	// iFldTNew = tnew;
	if (fldNew.nd)	{	mSceneMgr->destroySceneNode(fldNew.nd);  fldNew.nd = 0;  }
	if (fldNew.it)	{	mSceneMgr->destroyItem(fldNew.it);  fldNew.it = 0;  }
	
	try
	{	fldNew.it = mSceneMgr->createItem("cube.mesh");
		fldNew.nd = mSceneMgr->getRootSceneNode()->createChildSceneNode();
		fldNew.nd->attachObject(fldNew.it);  fldNew.it->setVisibilityFlags(RV_Vegetation);
		UpdFldNewNode();
	}
	catch (Exception ex)
	{
		LogO("no field! " + ex.getFullDescription());
	}
}

void App::UpdFldNewNode()
{
	if (!scn->road || !fldNew.nd)  return;

	bool vis = scn->road->bHitTer && bEdit() && iFldCur == -1 && edMode == ED_Fields;
	fldNew.nd->setVisible(vis);
	if (!vis)  return;
	
	Vector3 p = scn->road->posHit;  p.y += fldNew.pos[2];
	fldNew.nd->setPosition(p);
	fldNew.nd->setScale(fldNew.size);
	fldNew.nd->_getFullTransformUpdated();
}

#endif  // ed
