#include "pch.h"
#include "Def_Str.h"
#include "Road.h"
#include "Cam.h"
#include "App.h"
#ifndef SR_EDITOR
	#include "game.h"
#endif
#include <OgreCamera.h>
#include <OgreMaterialManager.h>
#include <OgreItem.h>
// #include <OgreSubEntity.h>
#include <OgreSceneNode.h>

#include <tinyxml2.h>
using namespace tinyxml2;
using namespace Ogre;


//  🌟 ctor
//---------------------------------------------------------------------------------------------------------------
#ifdef SR_EDITOR
SplineRoad::SplineRoad(App* papp)
	: pApp(papp)
#else
SplineRoad::SplineRoad(GAME* pgame)
	: pGame(pgame), pApp(pgame->app)
#endif
{
	Defaults();
	st.Reset();
	iOldHide = -1;
}

void SplineRoad::Defaults()
{
	type = RD_Road;  trailSegId = -1;
	sTxtDescr = "";  sTxtAdvice = "";
	fScRot = 1.8f;  fScHit = 0.8f;
	
	for (int i=0; i < MTRs; ++i)
	{	sMtrRoad[i] = "";  sMtrPipe[i] = "";
		bMtrAlpha[i] = true;
		bMtrPipeGlass[i] = true;  bMtrRoadTer[0] = false;
	}
	sMtrRoad[0] = "roadNgravel";  bMtrRoadTer[0] = true;
	sMtrPipe[0] = "pipeGlass";
	sMtrWall = "road_wall";  sMtrCol = "road_col";  sMtrWallPipe = "pipe_wall";
	
	g_tcMul = 0.1f;  g_tcMulW = 0.2f;  g_tcMulP = 0.1f;  g_tcMulPW = 0.3f;  g_tcMulC = 0.2f;

	g_LenDim0 = 1.f;  g_iWidthDiv0 = 8;  g_P_iw_mul = 4;  g_P_il_mul = 1;
	g_SkirtLen = 1.f;  g_SkirtH = 0.12f;  g_Height = 0.1f;
	g_MergeLen = 180.f;  bMerge = false;  g_LodPntLen = 10.f;
	g_VisDist = 1000.f;  g_VisBehind = 800.f;
	
	g_ColNSides = 4; g_ColRadius = 2.f;

	iDir = -1;  vStartBoxDim = Vector3(1.5f, 5, 12);  // /long |height -width
	iChkId1 = 0;  iChkId1Rev = 0;
}

//  🌟 ctor stats
void SplineRoad::Stats::Reset()
{
	iMrgSegs = 0;  segsMrg = 0;
	Length = 0.f;  WidthAvg = 0.f;  HeightDiff = 0.f;
	OnTer = 0.f;   Pipes = 0.f;  OnPipe = 0.f;
	bankAvg = 0.f;  bankMax = 0.f;
}


void SplineRoad::ToggleMerge()
{
	bMerge = !bMerge;
	Rebuild(true);
}
	

///  update road lods visibility
//--------------------------------------------------------------------------------------------------------
void SplineRoad::UpdLodVis(float fBias, bool bFull)
{
	if (!mCamera || !mCamera->cam)  return;
	st.iVis = 0;  st.iTris = 0;
	const Real fDist[LODs+1] = {-g_VisBehind, 40, 80, 140, g_VisDist};  //par gui bias..
	
	const Plane& pl = mCamera->cam->getFrustumPlane(FRUSTUM_PLANE_NEAR);
	for (size_t seg = 0; seg < vSegs.size(); ++seg)
	{
		#ifdef SR_EDITOR
		bool bSel = !bFull && ((vSel.size() == 0 && seg == iChosen || vSel.find(seg) != vSel.end()));
		#endif
		
		RoadSeg& rs = vSegs[seg];
		if (rs.empty)  continue;
		
		//  get dist
		Real d = FLT_MAX;
		for (size_t p=0; p < rs.lpos.size(); ++p)
			d = std::min(d, pl.getDistance( rs.lpos[p] ));
		
		//  set 1 item/mesh visible
		// todo:  drop .. use grid mesh with lods
		for (int i=0; i < LODs; ++i)
		{
			bool vis;
			if (bFull)  vis = i==0;  else  // all in 1st lod for preview
			vis = d >= fDist[i] * fBias && d < fDist[i+1] * fBias;  // normal
		#ifndef SR_EDITOR  // ed all
			if (IsTrail() && vis)  // ->--
				vis = mP[seg].nCk >= trailSegId -1
				   && mP[seg].nCk <= trailSegId +3;  // par vis -1..5 far
		#endif

			/*if (bMerge)  vis = rs.mrgLod == i;  // vis mrg test-
			else  vis = i == 3;/**/  // check lod 0
			
		#ifdef SR_EDITOR
			if (vis && rs.road[i].it)  // todo: not each frame-
			{
				auto* rend = rs.road[i].it->getSubItem(0);
				pApp->UpdSelectGlow(rend, bSel);
			}
		#endif
			
			if (rs.road[i].it)   rs.road[i].it->setVisible(vis);
			if (rs.road[i].it2)  rs.road[i].it2->setVisible(vis);
			if (rs.wall[i].it)   rs.wall[i].it->setVisible(vis);
			if (rs.blend[i].it)  rs.blend[i].it->setVisible(vis);
			//if (rs.col.ent && i==0)
			//	rs.col.ent->setVisible(vis);
			if (vis) {  ++st.iVis;  st.iTris += rs.nTri[i];  }
		}
	}
}

void SplineRoad::UpdLodVisMarks(Real distSq, bool vis)
{
	#ifdef SR_EDITOR  // ed markers visible  near only
	auto cam = mCamera->cam->getPosition();
	for (const auto& m : vMarks)
	if (m.nd)
	{
		if (!vis)
			m.nd->setVisible(false);
		else
		{	Real d = cam.squaredDistance(m.nd->getPosition());
			m.nd->setVisible(d < distSq);
	}	}
	#endif
}

void SplineRoad::HideMarks()
{
	if (!ndRot)  return;
	ndSel->setVisible(false);
	ndChosen->setVisible(false);
	ndHit->setVisible(false);
	ndChk->setVisible(false);
	ndRot->setVisible(false);
}

///  set lod0 for  render to tex   * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
/*void SplineRoad::SetForRnd(String sMtr)  // unsed ..
{
	MaterialPtr mat = MaterialManager::getSingleton().getByName(sMtr);
	for (auto& rs : vSegs)
	{
		if (rs.empty)  continue;
		
		for (int i=0; i < LODs; ++i)
		{
			if (rs.road[i].it)
			{	rs.road[i].it->setVisible(i==0);
				rs.road[i].it->setMaterial(mat);
			}
			if (rs.road[i].it2)
			{	rs.road[i].it2->setVisible(i==0);
				rs.road[i].it2->setMaterial(mat);
			}
			if (rs.wall[i].it)
				rs.wall[i].it->setVisible(false);
			if (rs.blend[i].it)
				rs.blend[i].it->setVisible(false);
		}
		if (rs.col.it)
			rs.col.it->setVisible(false);
	}
}
void SplineRoad::UnsetForRnd()  // unsed ..
{
	for (auto& rs : vSegs)
	{
		if (rs.empty)  continue;
		
		MaterialPtr mat = MaterialManager::getSingleton().getByName(  //^opt?..
			rs.sMtrRd);
		for (int i=0; i < LODs; ++i)
		{
			if (rs.road[i].it)
				rs.road[i].it->setMaterial(mat);
			if (rs.road[i].it2)
				rs.road[i].it2->setMaterial(mat);
			// wall auto in updLodVis
		}
		if (rs.col.it)
			rs.col.it->setVisible(true);
	}
}*/
void SplineRoad::SetVisTrail(bool vis)
{
	for (const auto& rs : vSegs)
	{
		if (rs.empty)  continue;

		for (int i=0; i < LODs; ++i)
			if (rs.road[i].it)
				rs.road[i].it->setVisible(vis);
	}
}

//---------------------------------------------------------------------------------------------------------------
///  Load
//---------------------------------------------------------------------------------------------------------------
bool SplineRoad::LoadFile(String fname, bool build)
{
	DestroyMarkers();
	if (build)
		clear();
	
	XMLDocument doc;
	XMLError e = doc.LoadFile(fname.c_str());
	if (e != XML_SUCCESS)  return false;
		
	XMLElement* root = doc.RootElement(), *n = NULL;
	if (!root)  return false;
	const char* a = NULL;
	
	Defaults();
	n = root->FirstChildElement("mtr");	if (n)  {
		a = n->Attribute("river");  if (a)  if (s2i(a) > 0)  type = RD_River;  // old attr
		a = n->Attribute("type");   if (a)  type = (RoadType)s2i(a);

		for (int i=0; i<MTRs; ++i)  {	String si = i==0 ? "" : toStr(i+1);
			a = n->Attribute(String("road"+si).c_str());	if (a)  sMtrRoad[i] = String(a);
			a = n->Attribute(String("pipe"+si).c_str());	if (a)  SetMtrPipe(i, String(a));	}
		a = n->Attribute("wall");   if (a)  sMtrWall = String(a);
		a = n->Attribute("pipeW");  if (a)  sMtrWallPipe = String(a);
		a = n->Attribute("col");    if (a)  sMtrCol = String(a);
	}
	n = root->FirstChildElement("dim");	if (n)  {
		a = n->Attribute("tcMul");  if (a)  g_tcMul = s2r(a);
		a = n->Attribute("tcW");    if (a)  g_tcMulW = s2r(a);
		a = n->Attribute("tcP");    if (a)  g_tcMulP = s2r(a);
		a = n->Attribute("tcPW");   if (a)  g_tcMulPW = s2r(a);
		a = n->Attribute("tcC");    if (a)  g_tcMulC = s2r(a);

		a = n->Attribute("lenDim");      if (a)  g_LenDim0 = s2r(a);
		a = n->Attribute("widthSteps");  if (a)  g_iWidthDiv0 = s2i(a);
		a = n->Attribute("heightOfs");   if (a)  g_Height = s2r(a);
	}
	n = root->FirstChildElement("mrg"); if (n)  {
		a = n->Attribute("loop");        if (a)  isLooped = s2i(a) > 0;
		a = n->Attribute("skirtLen");    if (a)  g_SkirtLen = s2r(a);
		a = n->Attribute("skirtH");      if (a)  g_SkirtH   = s2r(a);

		a = n->Attribute("merge");       if (a)  bMerge  = s2i(a) > 0;  // is always 1
		a = n->Attribute("mergeLen");    if (a)  g_MergeLen = s2r(a);
		a = n->Attribute("lodPntLen");   if (a)  g_LodPntLen = s2r(a);
		
		a = n->Attribute("visDist");     if (a)  g_VisDist = s2r(a);
		a = n->Attribute("visBehind");   if (a)  g_VisBehind = s2r(a);
	}
	int iP1 = 0;
	n = root->FirstChildElement("geom");	if (n)  {
		a = n->Attribute("colN");   if (a)  g_ColNSides = s2i(a);
		a = n->Attribute("colR");   if (a)  g_ColRadius = s2r(a);
		a = n->Attribute("wsPm");   if (a)  g_P_iw_mul = s2r(a);
		a = n->Attribute("lsPm");   if (a)  g_P_il_mul = s2r(a);
		a = n->Attribute("stBox");  if (a)  vStartBoxDim = s2v(a);
		a = n->Attribute("iDir");   if (a)  iDir = s2i(a);
		a = n->Attribute("iChk1");  if (a)  iP1 = s2i(a);
	}
	
	n = root->FirstChildElement("stats");	if (n)  {
		a = n->Attribute("length");    if (a)  st.Length = s2r(a);
		a = n->Attribute("width");     if (a)  st.WidthAvg = s2r(a);
		a = n->Attribute("height");    if (a)  st.HeightDiff = s2r(a);
		  a = n->Attribute("onTer");   if (a)  st.OnTer = s2r(a);
		  a = n->Attribute("pipes");   if (a)  st.Pipes = s2r(a);
		  a = n->Attribute("onPipe");  if (a)  st.OnPipe = s2r(a);
		  a = n->Attribute("bnkAvg");  if (a)  st.bankAvg = s2r(a);
		  a = n->Attribute("bnkMax");  if (a)  st.bankMax = s2r(a);
	}	
	n = root->FirstChildElement("txt");	if (n)  {
		a = n->Attribute("desc");		if (a)  sTxtDescr = String(a);
	}
	n = root->FirstChildElement("adv");	if (n)  {
		a = n->Attribute("ice");		if (a)  sTxtAdvice = String(a);
	}

	n = root->FirstChildElement("P");	//  points
	while (n)
	{
		a = n->Attribute("pos");  newP.pos = s2v(a);
		a = n->Attribute("w");    newP.width = !a ? 6.f : s2r(a);

		a = n->Attribute("a");    if (a)  newP.mYaw = s2r(a);  else  newP.mYaw = 0.f;
		a = n->Attribute("ay");   if (a)  newP.mRoll = s2r(a);  else  {
		  a = n->Attribute("ar"); if (a)  newP.mRoll = s2r(a);  else  newP.mRoll = 0.f;  }
		  
		a = n->Attribute("aT");   if (a)  newP.aType = (AngType)s2i(a);  else  newP.aType = AT_Both;

		a = n->Attribute("onTer"); newP.onTer = (a && a[0]=='0') ? false : true;
		a = n->Attribute("col");   newP.cols = !a ? 1 : s2i(a);

		a = n->Attribute("pipe");  newP.pipe = !a ? 0.f : std::max(0.f, std::min(1.f, s2r(a)));
		a = n->Attribute("mtr");   newP.idMtr = !a ? 0 : std::max(-1, std::min(MTRs-1, s2i(a)));
		a = n->Attribute("wall");  newP.idWall = !a ? 0 : std::max(-1, std::min(MTRs-1, s2i(a)));
		a = n->Attribute("not");   newP.notReal = !a ? 0 : (s2i(a) > 0);
		
		a = n->Attribute("chkR");  newP.chkR = !a ? 0.f : s2r(a);
		a = n->Attribute("ckL");   newP.loop = !a ? 0 : s2i(a);
		a = n->Attribute("onP");   newP.onPipe = !a ? 0 : s2i(a);
				
		//  Add point
		//newP.pos *= 0.7f;  //scale
		if (build)  Insert(INS_End);

		n = n->NextSiblingElement("P");
	}
	//  set 1st chk
	if (iP1 >= 0 && iP1 < getNumPoints())
		mP[iP1].chk1st = true;
	
	/// create loop-
	/*const int q = 8;
	for (int i=0; i < q*3; ++i)
	{
		Real a = Real(i%q)/q *PI_d*2.f, ri = Real(i)/q;
		Vector3 pos(cosf(a), 0.f, -sinf(a));
		pos *= 12.f + ri*5.f;  // r
		pos.z -= 30.f;
		pos.y += 1 + i * 1.5f;  // h
		Insert(INS_End, pos, 7.f, ri*360.f, 10.f +ri*5.f, 0);
	}*/
	
	#ifdef SR_EDITOR
	bMerge = false;  // editor off merge
	#endif
	newP.SetDefault();
	iChosen = -1;  //std::max(0, std::min((int)(mP.size()-1), iChosen));
	
	SetChecks();  // 1st
	if (build)  Rebuild(true);
	return true;
}

///  Save
//---------------------------------------------------------------------------------------------------------------
bool SplineRoad::SaveFile(String fname)
{
	XMLDocument xml;
	XMLElement* root = xml.NewElement("SplineRoad");

	XMLElement* mtr = xml.NewElement("mtr");
		mtr->SetAttribute("type",    toStrC( type ));
		for (int i=0; i<MTRs; ++i)  {	String si = i==0 ? "" : toStr(i+1);
			if (sMtrRoad[i] != "")	mtr->SetAttribute(String("road"+si).c_str(),	sMtrRoad[i].c_str());
			if (sMtrPipe[i] != "")	mtr->SetAttribute(String("pipe"+si).c_str(),	sMtrPipe[i].c_str());  }

		if (sMtrWall != "road_wall")	mtr->SetAttribute("wall",	sMtrWall.c_str());
	if (sMtrWallPipe != "pipe_wall")	mtr->SetAttribute("pipeW",	sMtrWallPipe.c_str());
		if (sMtrCol  != "road_col")		mtr->SetAttribute("col",	sMtrCol.c_str());
	root->InsertEndChild(mtr);
	
	XMLElement* dim = xml.NewElement("dim");
		dim->SetAttribute("tcMul", toStrC( g_tcMul ));
		dim->SetAttribute("tcW",   toStrC( g_tcMulW ));
		dim->SetAttribute("tcP",   toStrC( g_tcMulP ));
		dim->SetAttribute("tcPW",  toStrC( g_tcMulPW ));
		dim->SetAttribute("tcC",   toStrC( g_tcMulC ));

		dim->SetAttribute("lenDim",     toStrC( g_LenDim0 ));
		dim->SetAttribute("widthSteps", toStrC( g_iWidthDiv0 ));
		dim->SetAttribute("heightOfs",  toStrC( g_Height ));
	root->InsertEndChild(dim);

	XMLElement* mrg = xml.NewElement("mrg");
		mrg->SetAttribute("loop",      isLooped ? "1" : "0" );
		mrg->SetAttribute("skirtLen",  toStrC( g_SkirtLen ));
		mrg->SetAttribute("skirtH",    toStrC( g_SkirtH ));

		mrg->SetAttribute("merge",      "1");  // always 1 for game, 0 set in editor
		mrg->SetAttribute("mergeLen",   toStrC( g_MergeLen ));
		mrg->SetAttribute("lodPntLen",  toStrC( g_LodPntLen ));

		mrg->SetAttribute("visDist",    toStrC( g_VisDist ));
		mrg->SetAttribute("visBehind",  toStrC( g_VisBehind ));
	root->InsertEndChild(mrg);

	int num = getNumPoints();
	int iP1 = 0;  // find 1st chk id
	for (int i=0; i < num; ++i)
		if (mP[i].chk1st)
			iP1 = i;
	
	XMLElement* geo = xml.NewElement("geom");
		geo->SetAttribute("colN",  toStrC( g_ColNSides ));
		geo->SetAttribute("colR",  toStrC( g_ColRadius ));
		geo->SetAttribute("wsPm",  toStrC( g_P_iw_mul ));
		geo->SetAttribute("lsPm",  toStrC( g_P_il_mul ));
		geo->SetAttribute("stBox", toStrC( vStartBoxDim ));
		geo->SetAttribute("iDir",  toStrC( iDir ));
		geo->SetAttribute("iChk1", toStrC( iP1 ));
	root->InsertEndChild(geo);

	XMLElement* ste = xml.NewElement("stats");
		ste->SetAttribute("length", toStrC( st.Length ));
		ste->SetAttribute("width",  toStrC( st.WidthAvg ));
		ste->SetAttribute("height", toStrC( st.HeightDiff ));
		ste->SetAttribute("onTer",  toStrC( st.OnTer ));
		ste->SetAttribute("pipes",  toStrC( st.Pipes ));
		if (st.OnPipe > 0.f)
		ste->SetAttribute("onPipe", toStrC( st.OnPipe ));
		ste->SetAttribute("bnkAvg", toStrC( st.bankAvg ));
		ste->SetAttribute("bnkMax", toStrC( st.bankMax ));
	root->InsertEndChild(ste);
		
	XMLElement* txt = xml.NewElement("txt");
		txt->SetAttribute("desc",  sTxtDescr.c_str());
	root->InsertEndChild(txt);
	XMLElement* adv = xml.NewElement("adv");
		adv->SetAttribute("ice",   sTxtAdvice.c_str());
	root->InsertEndChild(adv);

	for (int i=0; i < num; ++i)		//  points
	{
		bool onTer = mP[i].onTer, onTer1 = mP[(i+1)%num].onTer, onTer_1 = mP[(i-1+num)%num].onTer;
		XMLElement* p = xml.NewElement("P");
		{
			Vector3 pos = getPos(i);  if (onTer)  pos.y = 0.f;  // no need to save
			p->SetAttribute("pos", toStrC( pos ));
			p->SetAttribute("w",   toStrC( mP[i].width ));
		
			if (!onTer)
				p->SetAttribute("onTer",	"0");

			if (!onTer || !onTer1 || !onTer_1)
			{	p->SetAttribute("a",  toStrC( mP[i].mYaw ));
				p->SetAttribute("ar", toStrC( mP[i].mRoll ));
			}
			p->SetAttribute("aT", toStrC( (int)mP[i].aType ));

			if (mP[i].cols != 1)
				p->SetAttribute("col", toStrC( mP[i].cols ));
			if (mP[i].pipe > 0.f)
				p->SetAttribute("pipe", toStrC( mP[i].pipe ));

			if (mP[i].idMtr != 0)
				p->SetAttribute("mtr", toStrC( mP[i].idMtr ));
			if (mP[i].idWall != 0)
				p->SetAttribute("wall", toStrC( mP[i].idWall ));

			if (mP[i].notReal)
				p->SetAttribute("not", "1");

			if (mP[i].chkR > 0.f)
				p->SetAttribute("chkR", toStrC( mP[i].chkR ));
			if (mP[i].loop)
				p->SetAttribute("ckL", toStrC( mP[i].loop ));
			
			if (mP[i].onPipe > 0)
				p->SetAttribute("onP", toStrC( mP[i].onPipe ));
		}
		root->InsertEndChild(p);
	}
	
	xml.InsertEndChild(root);
	return xml.SaveFile(fname.c_str());
}
