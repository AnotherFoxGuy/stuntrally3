#include "pch.h"
#include "Def_Str.h"
#include "RenderConst.h"
#include "ShapeData.h"
#include "dbl.h"
#include "Road.h"
#include "Grid.h"
#include "CScene.h"
#ifndef SR_EDITOR
	#include "game.h"
	#include "CGame.h"
	#include "CScene.h"
	#include "SceneXml.h"
#else
	#include "CApp.h"
	#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
	#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
	#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
	#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#endif
#include "Terra.h"
#include <OgreMeshManager2.h>
#include <OgreMesh2.h>
#include <OgreItem.h>
using namespace Ogre;
using std::vector;  using std::min;  using std::max;

// #define USE_GRID_R
// #define USE_GRID_W

//  bridge, Walls:  - cross sections -
//
// +y     road       pipe        decor 1   decor 2   decor 3  
// |      2-1  6-5   2-1\  /6-5   21--07    1/0\7      1-0    
// 0--+x  | 0--7 |    \  07  /    |    |   2<   >6   2|   |7  
//        3______4      \  /      34--56    3\4/5    3|   |6  
//                       34                            4_5    
//  📏 wall front indices
const static int WFid[6][3] =
	{{2,1,0},{3,2,0},{5,4,7},{6,5,7}, {7,3,0},{4,3,7}};
// todo: more types, set per seg,  xml custom?, alternate 2..

//  wall width points, uv+add
struct WallWidthPnt
{	Real x,y, uv, nx,ny;
};
const static int ciwW = 7;  // wall  width steps  todo: any, vector
const static WallWidthPnt wallWidth[ciwW+1][3] = {  // section shape
	//  road wall                       //  pipe wall                      //  on pipe                      
	{{-0.5f, -0.0f, 0.0f,  1.0f, 0.0f}, {-0.0f, -0.2f, 0.0f,  0.0f, 1.0f}, {-0.14f, 1.5f, 0.0f, -1.0f, 0.0f}},
	{{-0.5f,  1.2f, 0.5f,  0.5f, 0.5f}, {-0.2f,  0.27f,0.5f,  0.0f, 1.0f}, {-0.14f, 1.4f, 0.2f, -0.5f, 0.5f}},
	{{-0.56f, 1.2f, 0.2f, -0.5f, 0.5f}, {-0.28f, 0.57f,0.2f, -0.5f, 0.5f}, {-0.14f, 1.3f, 0.2f, -0.5f, 0.0f}},
	{{-0.56f,-0.9f, 1.6f, -0.5f,-0.5f}, {-0.21f,-0.9f, 1.6f, -0.5f,-0.5f}, {-0.1f,  1.2f, 0.5f, -0.1f,-0.5f}},
	{{ 0.56f,-0.9f, 3.0f,  0.5f,-0.5f}, { 0.21f,-0.9f, 3.0f,  0.5f,-0.5f}, { 0.1f,  1.2f, 0.5f,  0.1f,-0.5f}},
	{{ 0.56f, 1.2f, 1.6f,  0.5f, 0.5f}, { 0.28f, 0.57f,1.6f,  0.5f, 0.5f}, { 0.14f, 1.3f, 0.2f,  0.5f, 0.0f}},
	{{ 0.5f,  1.2f, 0.2f, -0.5f, 0.5f}, { 0.2f,  0.27f,0.2f, -0.0f, 1.0f}, { 0.14f, 1.4f, 0.2f,  0.5f, 0.5f}},
	{{ 0.5f, -0.0f, 0.5f, -1.0f, 0.0f}, { 0.0f, -0.2f, 0.5f, -0.0f, 1.0f}, { 0.14f, 1.5f, 0.2f,  1.0f, 0.0f}}};
const static WallWidthPnt wallDecor[ciwW+1][3] = {  // section shape
	//  wall decor 1                    //  decor 2-                       // decor 3-
	{{ 0.5f,  0.5f, 0.0f,  0.0f, 1.0f}, { 0.0f,  1.0f, 0.0f,  0.0f, 1.0f}, { 0.5f,  1.0f, 0.0f,  0.0f, 1.0f}},
	{{-0.5f,  0.5f, 4.0f,  0.0f, 1.0f}, {-0.5f,  0.5f, 1.0f, -0.5f, 0.5f}, {-0.5f,  1.0f, 1.0f,  0.0f, 1.0f}},
	{{-0.5f,  0.5f, 0.0f, -1.0f, 0.0f}, {-1.0f,  0.0f, 1.0f, -1.0f, 0.0f}, {-1.0f,  0.5f, 1.0f, -1.0f, 0.0f}},
	{{-0.5f, -0.5f, 0.5f, -1.0f, 0.0f}, {-0.5f, -0.5f, 1.0f, -0.5f,-0.5f}, {-1.0f, -0.5f, 1.0f, -1.0f, 0.0f}},
	{{-0.5f, -0.5f, 0.0f,  0.0f,-1.0f}, { 0.0f, -1.0f, 1.0f,  0.0f,-1.0f}, {-0.5f, -1.0f, 1.0f,  0.0f,-1.0f}},
	{{ 0.5f, -0.5f, 4.0f,  0.0f,-1.0f}, { 0.5f, -0.5f, 1.0f,  0.5f,-0.5f}, { 0.5f, -1.0f, 1.0f,  0.0f,-1.0f}},
	{{ 0.5f, -0.5f, 0.0f,  1.0f, 0.0f}, { 1.0f,  0.0f, 1.0f,  1.0f, 0.0f}, { 1.0f, -0.5f, 1.0f,  1.0f, 0.0f}},
	{{ 0.5f,  0.5f, 0.5f,  1.0f, 0.0f}, { 0.5f,  0.5f, 1.0f,  0.5f, 0.5f}, { 1.0f,  0.5f, 1.0f,  1.0f, 0.0f}}};

//  Build Segment Geometry
//----------------------------------------------------------------------------------------------------------------------------
void SplineRoad::BuildSeg(
	const DataRoad& DR,
	const DataLod0& DL0, DataLod& DL, StatsLod& ST,
	DataLodMesh& DLM, DataSeg& DS,
	int segM, bool full)
{
	//  iterators
	int seg = (segM + DR.segs) % DR.segs;
	int seg1 = getNext(seg), seg0 = getPrev(seg), seg02 = getAdd(seg,-2);
	DS.seg = seg;  DS.seg1 = seg1;  DS.seg0 = seg0;  // save
	
	//LogR("[Seg]  cur: " + toStr(seg) + "/" /*+ toStr(DR.segM) + "  all:"*/ + toStr(DR.segs));/*if (isLod0*/

	//  on terrain  (whole seg)
	DS.onTer = mP[seg].onTer && mP[seg1].onTer;


	///  📏 jump front wall, ends in air
	//  0,1 for geometry, 2,1 for pacenotes
	//  Test on tracks:  iDir<0: jumps, CrossJumps  iDir>0: Mars, Platforms
	DS.jfw0 = iDir < 0 ?
		!mP[seg ].onTer && mP[seg0].isnt() :
		!mP[seg ].onTer && !mP[seg].isnt() && mP[seg0].isnt();
	DS.jfw1 = iDir < 0 ?
		!mP[seg1].onTer && mP[seg1].isnt() :
		!mP[seg1].onTer && mP[seg1].isnt();
	DS.jfw2 = iDir < 0 ?
		!mP[seg0].onTer && mP[seg02].isnt() :
		!mP[seg0].onTer && !mP[seg0].isnt() && mP[seg02].isnt();

	
	//  on merging segs only for game in whole road rebuild
	//  off for editor (partial, 4segs rebuild)
	bool bNew = true, bNxt = true;
	if (bMerge)
	{	bNew = (segM   == DR.sMin/*1st*/)  || DL.v_bMerge[seg];
		bNxt = (segM+1 == DR.sMax/*last*/) || DL.v_bMerge[seg1];  // next is new
		if (bNew)  LogR("^ merge");
	}
	
	if (bNew)  //> new seg data
		DLM.Clear();

	//  bullet create
	bool blt = true;  // game always
	#ifdef SR_EDITOR  // editor only sel segs for align ter tool, or full for objects sim
		blt = DR.bulletFull || DR.editorAlign && (vSel.find(seg) != vSel.end());
	#endif
	

	///  destroy old
	RoadSeg& rs = vSegs[seg];
	if (!rs.empty && DL.isLod0)
		DestroySeg(seg);


	//  material
	const int mid = mP[seg].idMtr;
	DS.mtrId = max(0,mid);  // on terrain use _ter mtr if it exists
	const String suffix = IsRoad() &&
		DS.onTer && bMtrRoadTer[DS.mtrId] ? "_ter" :"";

	DS.wallId = IsRiver() ? -1 : mP[seg].idWall;
	DS.pipe = isPipe(seg);
	rs.sMtrRd = DS.pipe ? sMtrPipe[DS.mtrId]
						: (sMtrRoad[DS.mtrId] + suffix);

	/// >  blend 2 materials
	DS.hasBlend = false;
	if (mid != mP[seg1].idMtr && !DS.pipe && !isPipe(seg1))
	{
		DS.hasBlend = true;
		int mtrB = max(0,mP[seg1].idMtr);
		rs.sMtrB = sMtrRoad[mtrB] + suffix;
	}
	
	//  skirt /\ not for bridges
	bool useSkirt = DS.onTer || DS.pipe;  // pipe own factor..
	Real skLen = useSkirt ? g_SkirtLen : 0.f, skH = useSkirt ? g_SkirtH : 0.f;

	//  alpha border ...
	rs.alpha = DS.onTer &&  0 &&  //! off rem-? old .mat
		rs.sMtrRd.find("_ter") != String::npos &&
		bMtrRoadTer[DS.mtrId] && bMtrAlpha[DS.mtrId];  // todo: upd in ed
	// rs.alpha = 0;  // test?
	// LogO("bMtrAlpha:: " + rs.sMtrRd +" ="+ (rs.alpha ? "1" : "0"));

	
	//  seg params  -----------------
	const int iwC = g_ColNSides;  // 🏛️ column  polygon steps
				
	//  steps len
	const int il = DL.v_iL[seg];
	const int il0= DL0.v0_iL[seg];
	const Real la = 1.f / il;
	const Real la0= 1.f / il0 * skLen;
	Real l = -la0;

	//  width
	//Real wi1 = fabs(mP[seg].width), wi2 = fabs(mP[seg1].width), wi12 = wi2-wi1;

	///  angles ()__
	Real ay1 = mP[seg].aYaw, ay2 = mP[seg1].aYaw, ay21 = ay2-ay1;
	Real ar1 = mP[seg].aRoll,ar2 = mP[seg1].aRoll,ar21 = ar2-ar1;
	const Real asw = 180;	// more than 180 swirl - wrong at start/end
	while (ay21 > asw)  ay21 -= 2*asw;  while (ay21 <-asw)  ay21 += 2*asw;
	while (ar21 > asw)  ar21 -= 2*asw;	while (ar21 <-asw)  ar21 += 2*asw;

	//  tc begin,range
	Real tcBeg = (seg > 0) ? DL.v_tc[seg-1]  : 0.f,  tcEnd  = DL.v_tc[seg],   tcRng  = tcEnd - tcBeg;
	Real tcBeg0= (seg > 0) ? DL0.v0_tc[seg-1]: 0.f,  tcEnd0 = DL0.v0_tc[seg], tcRng0 = tcEnd0 - tcBeg0;
	Real tcRmul = tcRng0 / tcRng;
	

	//------------------------------------------------------------------------------------
	//  Length  vertices
	//------------------------------------------------------------------------------------
	//LogR( " __len");
	const bool vis = mP[seg].idMtr >= 0;  // visible, -1 hides segment
	if (vis || DL.isPace)
	for (int i = -1; i <= il+1; ++i)  // length +1  +2-gap
	{
		++DLM.iLmrg;
		///  length <dir>  |
		Vector3 vL0 = interpolate(seg, l);
		Vector3 vl = GetLenDir(seg, l, l+la), vw{1,0,0};
		Real len = vl.length();  vl.normalise();
		
		//  len tc
		if (i <= 0)  DL.tcLen = 0;
		Real tc = DL.tcLen * tcRmul + tcBeg0;
		//  skirt tc
		if (i == -1)	tc =-skLen* tcRmul + tcBeg0;
		if (i == il+1)  tc = skLen* tcRmul + tcEnd0;
		
		///  width <dir>   --
		bool onTer = mP[seg].onTer && mP[seg1].onTer;
		if (onTer)
		{	vw = Vector3(vl.z, 0, -vl.x);  }
		else		/// angles ()__
		{	Real ay = ay1 + ay21 * l;  // linear-
			Real ar = ar1 + ar21 * l;
			vw = GetRot(ay,ar);  // from angles
		}
		vw.normalise();
		Vector3 vwn = vw;

		//Real wiMul = wi1 + wi12 * l;  // linear-
		Real wiMul = interpWidth(seg, l);  // spline~
		if (DR.editorAlign)  // wider road for align terrain tool
			wiMul = wiMul*ed_Wmul + ed_Wadd;
		vw *= wiMul;

		//  on terrain ~~
		bool onTer1 = DS.onTer || mP[seg].onTer && i==0 || mP[seg1].onTer && i==il;

		///  normal <dir>  /
		Vector3 vn{0,1,0};
		if (i==0)	vn = DL0.v0_N[seg];  else  // seg start=end
		if (i==il)	vn = DL0.v0_N[seg1];
		else
		{	vn = vl.crossProduct(vw);  vn.normalise();
			//  on pipe inv
			if (mP[seg].onPipe==2)
				vn = -vn;
		}
		

		//  width steps <->
		int iw = DL.v_iWL[seg][i+1];  //i = -1 .. il+1

		//  pipe amount (_)
		Real l01 = max(0.f, min(1.f, Real(i)/Real(il) ));
		Real p1 = mP[seg].pipe, p2 = mP[seg1].pipe;
		Real fPipe = p1 + (p2-p1)*l01;
		
		bool trans = (p1 == 0.f || p2 == 0.f) && !DL.v_iwEq[seg];
		Real trp = (p1 == 0.f) ? 1.f - l01 : l01;

		/*LogR("   il="+toStr(i)+"/"+toStr(il)+"   iw="+toStr(iw)
			+(bNew?"  New ":"") +(bNxt?"  Nxt ":""));/**/
		if (DS.hasBlend && vis)
			++DLM.iLmrgB;
		
		
		///  road ~    Width  vertices
		//--------------------------------------------------------------------------------------------
		Vector3 vH0{0,1,0}, vH1{1,0,0};  //#  positions for bank angle
		int w0 = DS.pipe ? iw/4   : 0,
			w1 = DS.pipe ? iw*3/4 : iw;

		Real tcL = tc * (DS.pipe ? g_tcMulP : g_tcMul),
			tcW = DS.pipe ? 1.f : 1.f;  //g_tcMulWP : g_tcMulW;  // todo:
		for (int w=0; w <= iw; ++w)  // width +1
		{
			//  pos create
			Vector3 vP{0,0,0}, vN{0,1,0};
			Real tcw = Real(w)/Real(iw);

			Real yTer = 0.f;
			if (fPipe == 0.f)
			{	//  flat --
				vP = vL0 + vw * (tcw - 0.5);
				vN = vn;
				yTer = scn ? scn->getTerH(vP.x, vP.z) : 0.f;
				if (onTer1)  //  onTerrain
				{
					vP.y = yTer + g_Height * ((w==0 || w==iw) ? 0.15f : 1.f);
						// * (seg % 2 == 0 ? 1.f : 0.5f);  // a bit better crossings? test on Hole
						// * ((seg % 3) * 0.3f);
					vN = scn ? TerUtil::GetNormalAt(scn->ters[0],  // 1st ter-
						vP.x, vP.z, DL.fLenDim*0.5f /*0.5f*/) : Vector3::UNIT_Y;
				}
			}else
			{	///  pipe (_)
				Real oo = (tcw - 0.5)/0.5 * PI_d * fPipe, so = sinf(oo), co = cosf(oo);
				vP = vL0 + vw  * 0.5 * so +
						 + vn * (0.5 - 0.5 * co) * wiMul;
				vN = vn * co + vwn * so;
				//LogR(toStr(w)+" "+fToStr(so,2,4));

				if (vN.y < 0.f)  vN.y = -vN.y;
				if (trans)  //  transition from flat to pipe
				{	vP += vw * (tcw - 0.5) * trp;  }
				yTer = scn ? scn->getTerH(vP.x, vP.z) : 0.f;
			}
			
			//  skirt ends, gap patch_
			if (i == -1 || i == il+1)
				vP -= vn * skH;
			
			
			/// []()  pace  add marker  ~ ~ ~  ~ ~ ~
			if (DL.isPace)
			{
				if (full && w == 1)  // center
				if (i >= 0 && i < il)
				{	//  add
					PaceM pm;
					bool op = mP[seg].onPipe > 0;
					bool onP = op && (iDir > 0 ?		// start
						mP[seg1].onPipe == 0 && i==il-1 : mP[seg0].onPipe == 0 && i==0);
					bool onPe = op && (iDir > 0 ?		// end
						mP[seg0].onPipe == 0 && i==0    : mP[seg1].onPipe == 0 && i==il-1);
					float h = 3.f;  // above
					if (mP[seg].onPipe == 1 && mP[seg].idMtr >= 0 ||
						mP[seg0].onPipe == 1 && mP[seg0].idMtr >= 0)
						h += wiMul;
					
					pm.pos  = vP + vN * h;  //par  + vw * 0.5f;
					pm.pos2 = vP + vN * (h + 1.f) + vw * 0.5f;  // extra, info
					
					pm.onTer = DS.onTer && fPipe < 0.1f;
					pm.loop = DL0.v0_Loop[seg];
					pm.onPipe = onP;  pm.onPipeE = onPe;
					bool no = mP[seg].notReal;
					pm.jump = no? 0: DS.jfw2;  pm.jumpR = no? 0: DS.jfw1;

					pm.vis = vis;  pm.notReal = no;
					vPace.push_back(pm);
				}

			}else if (vis)// && HasRoad())
			{
				Vector4 c{1,1,1,1};
				///  color  for 🌍 minimap preview  ---~~~====~~~---
				if (!IsTrail())
				{
				#ifdef SR_EDITOR
					Real brdg = min(1.f, std::abs(vP.y - yTer) * 0.4f);  //par ] height diff mul
					Real h = max(0.f, 1.f - std::abs(vP.y - yTer) / 30.f);  // for grass dens tex
					
					bool onP = mP[seg].onPipe > 0;
					// float pp = fPipe;  //-
					float pp = fPipe*0.5f + (onP ? 0.5f : 0.f);  // put onP in pipe
					
					Vector4 mic(brdg, pp,
						std::abs(vN.y), h);  // n
						//mP[seg].notReal ? 0.f : 1.f, h);  // fixme?
					DLM.clrMini.push_back(mic);
				#endif
				}else
				{	//  trail clr
					c = (float(i)/il) * (mP[seg1].clr - mP[seg].clr) + mP[seg].clr;
					c.w = 1.f;
				}
				Vector2 vtc(tcw * tcW,
					rs.alpha ? tcL * 0.03f : tcL);  //** par = alpha tc!

				//>  data road
				DLM.pos.push_back(vP);   DLM.norm.push_back(vN);
				DLM.tcs.push_back(vtc);  DLM.clr.push_back(c);//- ?
				
				if (DS.hasBlend)
				{	//  alpha, blend 2nd mtr
					// c = Vector4(Math::UnitRandom(), Math::UnitRandom(), Math::UnitRandom(), Math::RangeRandom(0.f,1.f));  // test
					c.w = std::max(0.f, std::min(1.f, float(i)/il ));
					DLM.posB.push_back(vP);   DLM.normB.push_back(vN);
					DLM.tcsB.push_back(vtc);  DLM.clrB.push_back(c);
				}
				
				//#  stats
				if (vP.y < ST.stMinH)  ST.stMinH = vP.y;
				if (vP.y > ST.stMaxH)  ST.stMaxH = vP.y;
				if (w==w0)  vH0 = vP;  //#
				if (w==w1)  vH1 = vP;
			}
		}	// width
		
		
		/// []()  normal
		if (!DL.isPace && vis)
		{
			//#  stats  banking angle
			if (DL.isLod0 && i==0)
			{
				float h = (vH0.y - vH1.y), w = vH0.distance(vH1), d = fabs(h/w), a = asin(d)*180.f/PI_d;
				ST.bankAvg += a;
				if (a > ST.bankMax)  ST.bankMax = a;
				/*LogR("RD seg :" + toStr(seg)+ "  h " + fToStr(h,1,3)
					+ "  w " + fToStr(w,1,3)+ "  d " + fToStr(d,1,3)+ "  a " + fToStr(a,1,3) );/**/
			}
			

			///  📏 wall ]
			//------------------------------------------------------------------------------------
			Real uv = 0.f;  // tc
			bool onP = mP[seg].onPipe==2;

			if (HasWall(DS.onTer) && DS.wallId >= 0)
			if (i >= 0 && i <= il)  // length +1
			{
				++DLM.iLmrgW;
				Real tcLW = tc * (DS.pipe ? g_tcMulPW : g_tcMulW);
				for (int w=0; w <= ciwW; ++w)  // width +1
				{
					int pp = (p1 > 0.f || p2 > 0.f) ? (onP ? 2 : 1) : 0;  //  pipe wall
					WallWidthPnt wP = IsDecor() ?
						wallDecor[w][0] :  // todo type per seg..
						wallWidth[w][pp];

					if (trans)
					{
						//  road to pipe, wall transition
						wP.x *= 1.f + 0.5f * trp;  // broader
						wP.y *= 1.f - 1.f * trp;   // flat
						if (!onP)
							wP.y -= 0.02f * trp;  //par move start down
					}
					uv += wP.uv;

					Vector3 vP = IsDecor() ?
						//  wall decor  0 -- .. 0.5 [] .. 1 |  by pipe factor
						vL0 + (1.f-fPipe) * wiMul * vwn * wP.x + fPipe * wiMul * vn * wP.y :
						vL0 + vw * wP.x + vn * wP.y;  // bridge wall
					Vector3 vN = vwn * wP.nx + vn * wP.ny;  vN.normalise();

					//>  data Wall
					DLM.posW.push_back(vP);  DLM.normW.push_back(vN);
					DLM.tcsW.push_back(0.25f * Vector2(uv, tcLW));  //par
				}
			}
			
			
			///  🏛️ columns |
			//------------------------------------------------------------------------------------
			if (HasColumns() && !DS.onTer && mP[seg].cols > 0)
			if (i == il/2)  // middle-
			{	
				++DLM.iLmrgC;
				const Real r = g_ColRadius;  // column radius

				for (int h=0; h <= 1; ++h)  // height
				for (int w=0; w <= iwC; ++w)  // width +1
				{
					Real a = Real(w)/iwC *2*PI_d,  //+PI_d/4.f
						x = r*cosf(a), y = r*sinf(a);

					Vector3 vlXZ(vl.x, 0.f, vl.z);
					Real fl = 1.f/max(0.01f, vlXZ.length());
					Vector3 vP = vL0 + fl * vl * x + vwn * y;
					Real yy;

					if (h==0)  // top below road
					{	yy = vn.y * (onP ? 1.5f : -0.8f);  //par
						vP.y += yy;
					}
					else  // bottom below ground
					{	yy = (scn ? scn->getTerH(vP.x, vP.z) : 0.f) - 0.3f;
						vP.y = yy;
					}

					Vector3 vN(vP.x-vL0.x, 0.f, vP.z-vL0.z);  vN.normalise();

					//>  data Col
					DLM.posC.push_back(vP);  DLM.normC.push_back(vN);
					DLM.tcsC.push_back(Vector2( Real(w)/iwC * 4, vP.y * g_tcMulC ));  //par
				}
			}
		}
		
		
		if (i == -1 || i == il)  // add len
		{	l += la0;  DL.tcLen += len;  }
		else
		{	l += la;  DL.tcLen += len;  }
	}
	//  Length  vertices
	//------------------------------------------------------------------------------------
	

	/// []()  pacenotes
	if (DL.isPace)
		return;  // no mesh
		

	//  lod vis points
	if (DL.isLod0)
	{	int lps = max(2, (int)(DL.v_len[seg] / g_LodPntLen));

		for (int p=0; p <= lps; ++p)
		{
			Vector3 vp = interpolate(seg, Real(p)/Real(lps));
			DLM.posLod.push_back(vp);
		}
	}


	///  create mesh  indices
	//------------------------------------------------------------------------------------------------
	blendTri = false;
	if (bNxt && !DLM.pos.empty())  // Merging
	{
		bltTri = blt;  blendTri = DS.hasBlend;

		createSeg_Meshes(DL,DLM, DS, rs);

		//  copy lod points
		if (DL.isLod0)
		{	for (size_t p=0; p < DLM.posLod.size(); ++p)
				rs.lpos.push_back(DLM.posLod[p]);
			DLM.posLod.clear();
		}
		//#  stats--
		if (ST.stats)
		{
			rs.mrgLod = (st.iMrgSegs % 2)*2+1;  //-
			st.iMrgSegs++;	 // count, full
		}

		//  bullet trimesh  at lod 0
		if (DL.isLod0 && blt && (IsRoad() || IsDecor()))  // todo: river too?
			createSeg_Collision(DLM,DS);
	}
}



//----------------------------------------------------------------------------------------------------------------------------
//   Create Meshes, from merged segments data
//----------------------------------------------------------------------------------------------------------------------------
void SplineRoad::createSeg_Meshes(
	const DataLod& DL,
	const DataLodMesh& DLM,
	DataSeg& DS, RoadSeg& rs)
{
	posBt.clear();
	idx.clear();  // set for addTri
	idxB.clear();
	at_pos = &DLM.pos;  at_size = DLM.pos.size();  at_ilBt = DLM.iLmrg-2;
	int seg = DS.seg, seg1 = DS.seg1, seg0 = DS.seg0;
	
	///  road ~
	int iiw = 0;  //LogR( " __idx");

	//  go through whole merged length
	//  equal width
	if (DL.v_iwEq[seg]==1)
		for (int i = 0; i < DLM.iLmrg-1; ++i)  // length-1 +2gap
		{
			//  |\|  grid  w-1 x l-1
			int iw = DL.v_iW[seg];
			for (int w=0; w < iw; ++w)  // width-1
			{
				//LogR( "   il="+toStr(i)+"/"+toStr(il)+"   iw="+toStr(iw));
				int f0 = iiw + w, f1 = f0 + (iw+1);
				addTri(f0+0,f1+1,f0+1,i);
				addTri(f0+0,f1+0,f1+1,i);
			}
			iiw += iw + 1;
		}
	else
		///  pipe trans  width steps changing in length
		for (int i = 0; i < DLM.iLmrg-1; ++i)  // length-1 +2gap
		{
			int iw = DL.v_iWL[seg][i], iw1 = DL.v_iWL[seg][i+1];
			int d = iw1 - iw, dd = abs(d);

			//  comment out //addTris to test
			if (i==0)  LogR("");
			LogR("   il=" + iToStr(i,3) + " iw=" + iToStr(iw,3) + " d " + toStr(d));

			if (d > 0)	//  inc  iw < iw1
			for (int w=0; w < iw; ++w)  // width-1
			{
				int f0 = iiw + w, f1 = f0 + iw+1;
				//  |\ |  f0+0  f0+1
				//  | \|  f1+0  f1+1
				addTri(f0+0,f1+1,f0+1,i);
				addTri(f0+0,f1+0,f1+1,i);
			}
			else		//  dec  iw1 <= iw
			for (int w=0; w < iw1; ++w)  // width-1
			{
				int f0 = iiw + w, f1 = f0 + iw+1;
				//  |/|
				addTri(f0+0,f1+0,f0+1,i);
				addTri(f0+1,f1+0,f1+1,i);
			}

			///  \|/   fan tris
			///  fix edge gaps, when iw changes

			if (d > 0)  //  inc  iw < iw1
			{
				int f0 = iiw + iw;
				for (int m=0; m < dd; ++m)
				{
					int f1 = f0 + iw+1 +m;
					addTri(f0,f1,f1+1,i);
				}
			}
			if (d < 0)  //  dec  iw1 < iw
			{
				int f0 = iiw + iw + iw1+1;
				for (int m=0; m < dd; ++m)
				{
					int f1 = iiw + iw-1 -m;
					addTri(f1+1,f1,f0,i);
				}
			}
			iiw += iw + 1;
		}

	vSegs[seg].nTri[DL.lod] = idx.size()/3;
	blendTri = false;


	//  create Ogre Meshes
	//-----------------------------------------
	const String sEnd = toStr(idStr) + "_" + toStr(idRd);  ++idStr;
	const String sMesh =
		(!IsTrail() ? "road." : "trl"+toStr(player)+".") + sEnd,
		sMeshW = sMesh + "W", sMeshC = sMesh + "C", sMeshB = sMesh + "B";
	const int lod = DL.lod;

	const bool wall = !DLM.posW.empty();
	const bool pipeGlass = DS.pipe && bMtrPipeGlass[ mP[seg].idMtr ];  // pipe glass mtr
	const bool invisible = strstr(rs.sMtrRd.c_str(), "Invisible");


	if (HasRoad() && !invisible)
	{
		/*LogO("RD- lod "+toStr(lod)+ "  mtr "+rs.sMtrRd+
			"  pos "+toStr(DLM.pos.size())+ "  norm "+toStr(DLM.norm.size())+
			"  clr "+toStr(DLM.clr.size())+ "  tcs "+toStr(DLM.tcs.size())+
			"  idx "+toStr(idx.size()) );*/
	#ifdef USE_GRID_R  //-
		pApp->scn->grid.AddMesh(
			lod,
			DLM.pos[0],  //wip
			GridMtr(rs.sMtrRd),
			DLM.pos, DLM.norm, DLM.clr, DLM.tcs, idx);
	#else
		CreateMesh( lod, rs.road[lod], sMesh,
			rs.sMtrRd, rs.alpha, pipeGlass, false,
			DLM.pos, DLM.norm, DLM.clr, DLM.tcs, idx);
	#endif
	#ifdef SR_EDITOR
		if (lod == 0)  //  🌍 Minimap
		CreateMesh( 0, rs.mini, sMesh+"#",
			rs.sMtrRd, rs.alpha, pipeGlass,  !IsTrail(), //*
			DLM.pos, DLM.norm,  IsTrail() ? DLM.clr : DLM.clrMini,
			DLM.tcs, idx);
	#endif
	}
	if (DS.hasBlend && !invisible)  // Blend >
	{
		/*LogO("RD^ lod "+toStr(lod)+ "  mtr "+rs.sMtrB+
			"  pos "+toStr(DLM.posB.size())+ "  norm "+toStr(DLM.normB.size())+
			"  clr "+toStr(DLM.clrB.size())+ "  tcs "+toStr(DLM.tcsB.size())+
			"  idx "+toStr(idxB.size()) );*/
		CreateMesh( lod, rs.blend[lod], sMeshB,
			rs.sMtrB, rs.alpha, pipeGlass, false,
			DLM.posB, DLM.normB, DLM.clrB, DLM.tcsB, idxB);
		rs.blend[lod].it->setRenderQueueGroup(RQG_RoadBlend);
		// CreateMesh( -1, sm, aabox,  rs.sMtrB);
	}

	bool cols = !DLM.posC.empty() && DL.isLod0;  // cols have no lods
	//*=*/wall = 0;  cols = 0;  // test


	///  📏 wall ]
	//------------------------------------------------------------------------------------
	if (wall)
	{
		idx.clear();
		for (int i = 0; i < DLM.iLmrgW-1; ++i)  // length
		{	int iiW = i* (ciwW+1);

			for (int w=0; w < ciwW; ++w)  // width
			{
				int f0 = iiW + w, f1 = f0 + (ciwW+1);
				idx.push_back(f0+1);  idx.push_back(f1+1);  idx.push_back(f0+0);
				idx.push_back(f1+1);  idx.push_back(f1+0);  idx.push_back(f0+0);
			}
		}
		
		//  front plates start,end  |_|  not in pipes
		int i,f, b = DLM.posW.size()-ciwW-1;
		if (!DS.pipe)
		{
			int ff = DS.jfw0 ? 6 : 4;
			for (f=0; f < ff; ++f)
				for (i=0; i<=2; ++i)  idx.push_back( WFid[f][i] );
			
			ff = DS.jfw1 ? 6 : 4;
			for (f=0; f < ff; ++f)
				for (i=0; i<=2; ++i)  idx.push_back( WFid[f][2-i]+b );

			vSegs[seg].nTri[DL.lod] += idx.size()/3;
		}
		
		// sm = meshW->getSubMesh(0);   // for glass only..
		if (!DLM.posW.empty())
		{
			rs.sMtrWall = !pipeGlass ? sMtrWall : sMtrWallPipe;

		#ifdef USE_GRID_W
			pApp->scn->grid.AddMesh(
				lod,
				DLM.posW[0],  //wip
				GridMtr(rs.sMtrWall),
				DLM.posW, DLM.normW, DLM.clr0, DLM.tcsW, idx);
		#else
			CreateMesh( -1, rs.wall[lod], sMesh+"W",
				rs.sMtrWall, false, false, false,
				DLM.posW, DLM.normW, DLM.clr0, DLM.tcsW, idx);
			rs.wall[lod].it->setCastShadows(true);
		#endif
		}
	}
	
	
	///  🏛️ columns |
	//------------------------------------------------------------------------------------
	const int iwC = g_ColNSides;
	if (cols)
	{
		idx.clear();
		at_pos = &DLM.posC;

		for (int l=0; l < DLM.iLmrgC; ++l)
		for (int w=0; w < iwC; ++w)
		{
			int f0 = w + l*(iwC+1)*2, f1 = f0 + iwC+1;
			addTri(f0+0, f1+1, f0+1, 1);
			addTri(f0+0, f1+0, f1+1, 1);
		}					
		vSegs[DS.seg].nTri[DL.lod] += idx.size()/3;

		if (!DLM.posC.empty())
		{
			GridCellLods::pApp = pApp;
			
			pApp->scn->grid.AddMesh(
				lod,
				DLM.posC[0],  //wip
				GridMtr(sMtrCol),
				DLM.posC, DLM.normC, DLM.clr0, DLM.tcsC, idx);
			/*CreateMesh( -1, rs.col, sMesh+"C",
				sMtrCol, false, false,*/
		}
	}
	

	//  🛣️ road  vis, que  ----
	if (HasRoad())
	{
		auto *it = rs.road[lod].it,
			*it2 = rs.road[lod].it2;
		if (it)
		{	
			bool refra = 0;  // todo ?
			// bool refra = mtr.substr(0,5) == "River"
			// 	&& pApp->pSet->g.water_refract;
			
			auto que = 
				refra ? RQG_Refract : 
				IsRiver() ? RQG_Fluid :
				//IsTrail() ? RQG_RoadBlend /*: RQG_Hud1*/ : // ?
				pipeGlass ? RQG_PipeGlass : RQG_Road;
			// LogO("road que: " + toStr(que));
			it->setRenderQueueGroup(que);
			if (it2)  it2->setRenderQueueGroup(que);
			
			if (IsRiver())
			{	it->setVisibilityFlags( RV_Fluid );
				if (it2)  it2->setVisibilityFlags( RV_Fluid );
			}

			// if (IsTrail())
			// 	it->setVisibilityFlags(RV_Hud3D);

			if (bCastShadow && !DS.onTer && !IsRiver() && !IsTrail())
				it->setCastShadows(true);
	}	}
	if (DS.hasBlend)  // Blend >
	{
		auto *it = rs.blend[lod].it,
			*it2 = rs.blend[lod].it2;
		if (it)
		{	auto que = RQG_RoadBlend;
			it->setRenderQueueGroup(que);
			if (it2)  it2->setRenderQueueGroup(que);
			it->setCastShadows(false);
	}	}

	rs.empty = false;  // new
}


//  🎳 Create Bullet Collision
//----------------------------------------------------------------------------------------------------------------------------
void SplineRoad::createSeg_Collision(
	const DataLodMesh& DLM,
	const DataSeg& DS)
{
	btTransform tr;  tr.setIdentity();  //tr.setOrigin(pc);

if (!IsDecor())
{
	btTriangleMesh* trimesh = new btTriangleMesh();  vbtTriMesh.push_back(trimesh);
	
	#define vToBlt(v)   btVector3(v.x, -v.z, v.y)
	#define addTriB(a,b,c)  trimesh->addTriangle(vToBlt(a), vToBlt(b), vToBlt(c))

	size_t si = posBt.size(), a=0;  // %3!
	for (size_t i=0; i < si/3; ++i,a+=3)
		addTriB(posBt[a], posBt[a+1], posBt[a+2]);

	// if (cols)  // add columns^..
	
	//  Road  ~
	btCollisionShape* shape = new btBvhTriangleMeshShape(trimesh, true);
	
	size_t su = IsRiver() ? SU_Fluid : (DS.pipe ? SU_Pipe : SU_Road) + DS.mtrId;
	shape->setUserPointer((void*)su);  // mark as road/pipe + mtrId
	shape->setMargin(0.01f);  //?
	
	btCollisionObject* bco = new btCollisionObject();
	
	bco->setActivationState(DISABLE_SIMULATION);
	bco->setCollisionShape(shape);	bco->setWorldTransform(tr);
	bco->setFriction(0.8f);   //+
	bco->setRestitution(0.f);
	bco->setCollisionFlags(bco->getCollisionFlags() |
		(IsRiver() ? btCollisionObject::CF_NO_CONTACT_RESPONSE : 0) |
		btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT/**/);
	#ifdef SR_EDITOR
		pApp->world->addCollisionObject(bco);
		bco->setUserPointer((void*)111);  // mark road
	#else
	if (IsRiver())
	{	FluidBox& fb = pGame->app->scn->sc->fluids[0];  /// todo: .. depth buffer, in rivers
		bco->setUserPointer(new ShapeData(ST_Fluid, 0, &fb));  ///~~
	}
		pGame->collision.world->addCollisionObject(bco);
		pGame->collision.shapes.push_back(shape);
	#endif
}
	
	//  📏 Wall  ]
	#ifndef SR_EDITOR  // in Game
	bool wall = !DLM.posW.empty();
	if (wall)
	{	btTriangleMesh* trimesh = new btTriangleMesh();  vbtTriMesh.push_back(trimesh);
		
		for (int i = 0; i < DLM.iLmrgW-1; ++i)  // length
		{	int iiW = i* (ciwW+1);

			for (int w=0; w < ciwW; ++w)  // width
			if (bRoadWFullCol || w==0 || w == ciwW-1)  // only 2 sides|_| optym+
			{
				int f0 = iiW + w, f1 = f0 + (ciwW+1);
				addTriB(DLM.posW[f0+0], DLM.posW[f1+1], DLM.posW[f0+1]);
				addTriB(DLM.posW[f0+0], DLM.posW[f1+0], DLM.posW[f1+1]);
			}
		}
		//  front plates start,end  |_|
		int f, b = DLM.posW.size()-ciwW-1;
		if (!DS.pipe)
		{
			int ff = DS.jfw0 ? 6 : 4;
			for (f=0; f < ff; ++f)
				addTriB(DLM.posW[WFid[f][0]], DLM.posW[WFid[f][1]], DLM.posW[WFid[f][2]]);

			ff = DS.jfw1 ? 6 : 4;
			for (f=0; f < ff; ++f)
				addTriB(DLM.posW[WFid[f][2]+b], DLM.posW[WFid[f][1]+b], DLM.posW[WFid[f][0]+b]);
		}
		
		btCollisionShape* shape = new btBvhTriangleMeshShape(trimesh, true);
		shape->setUserPointer((void*)SU_RoadWall);  //wall and column same object..
		
		btCollisionObject* bco = new btCollisionObject();
		bco->setActivationState(DISABLE_SIMULATION);
		bco->setCollisionShape(shape);	bco->setWorldTransform(tr);
		bco->setFriction(0.1f);   //+
		bco->setRestitution(0.f);
		bco->setCollisionFlags(bco->getCollisionFlags() |
			btCollisionObject::CF_STATIC_OBJECT | btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT/**/);
		pGame->collision.world->addCollisionObject(bco);
		pGame->collision.shapes.push_back(shape);
	}
	#endif
}
