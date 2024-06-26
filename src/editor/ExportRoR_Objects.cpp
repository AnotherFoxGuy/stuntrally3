#include "pch.h"
#include "ExportRoR.h"

#include "enums.h"
#include "Def_Str.h"
#include "BaseApp.h"
#include "settings.h"
#include "paths.h"

#include "CApp.h"
#include "CGui.h"
#include "CScene.h"
#include "CData.h"
#include "TracksXml.h"
#include "Axes.h"

#include <OgreString.h>
#include <OgreVector3.h>

#include <string>
#include <map>
using namespace Ogre;
using namespace std;


//------------------------------------------------------------------------------------------------------------------------
//  📄📦 Objects  save  .tobj
//------------------------------------------------------------------------------------------------------------------------
void ExportRoR::ExportObjects()
{
	std::vector<string> dirs{"objects", "objects2", "objectsC", "objects0", "obstacles"};
	
	string objFile = path + name + "-obj.tobj";
	ofstream obj;
	obj.open(objFile.c_str(), std::ios_base::out);

	int iodef = 0, iObjMesh = 0;
	std::map<string, int> once;
	hasObjects = !sc->objects.empty();

	for (const auto& o : sc->objects)
	if (!o.dyn)  // skip dynamic
	{
		Vector3 p = Axes::toOgre(o.pos);
		auto q = Axes::toOgreW(o.rot);

		const string mesh = o.name + ".mesh";

		//------------------------------------------------------------
		//  Find mesh  in old SR dirs
		//------------------------------------------------------------
		bool exists = 0;
		string from, to;

		for (auto& d : dirs)
		{
			string file = pSet->pathExportOldSR + d +"/"+ mesh;
			if (PATHS::FileExists(file))
			{	exists = 1;  from = file;  }
			// gui->Exp(CGui::INFO, String("obj: ")+file+ "  ex: "+(exists?"y":"n"));
		}
		if (!exists)  //! skip  RoR would crash
		{
			gui->Exp(CGui::WARN, "object not found in old SR: "+mesh);
			continue;
		}
		if (!AddPackFor(mesh))  //+
			continue;
		

		bool many = 1;  // each mesh own scale
		// bool many = 0;  // or one for all mesh
		string odefName = many ? o.name +"-"+ toStr(iodef) : o.name;

		// if (copyObjs)  // todo
		{
			//  object  save  .odef
			//------------------------------------------------------------
			string odefFile = path + odefName + ".odef";
			// if (!fs::exists(odefFile))	
			if (many || once.find(o.name) == once.end())
			{
				once[o.name] = 1;
				ofstream odef;
				odef.open(odefFile.c_str(), std::ios_base::out);
				
				odef << mesh + "\n";
				//odef << "1, 1, 1\n";
				odef << o.scale.x <<", "<< o.scale.y <<", "<< o.scale.z <<"\n";
				odef << "\n";
				odef << "beginmesh\n";
				odef << "mesh " + mesh + "\n";
				odef << "stdfriction concrete\n";
				odef << "endmesh\n";
				odef << "\n";
				odef << "end\n";
				
				odef.close();  ++iodef;
			
				//  copy
				//----------------------------------
				if (copyObjs && exists)
				{
					to = path + mesh;
					CopyFile(from, to);
					if (CopyFile(from, to))
						++iObjMesh;
					else
						continue;
				}
			}
		}

		//  write  ------
		if (exists)
		{
			p.y += sc->rorCfg.yObjOfs;
			obj << strPos(p) << " ";
			// todo  fix all rot ?
			// obj << fToStr(q.getPitch().valueDegrees() + 90.f,0,3)+", "+fToStr(-q.getYaw().valueDegrees() + 90.f,0,3)+", "+fToStr(q.getRoll().valueDegrees(),0,3)+", ";
			//- obj << fToStr(q.getPitch().valueDegrees() + 90.f,0,3)+", "+fToStr(-q.getYaw().valueDegrees() + 90.f,0,3)+", "+fToStr(q.getRoll().valueDegrees(),0,3)+", ";
			obj << "90, 0, "+fToStr(-q.getYaw().valueDegrees() + 90.f, 1,4)+",  ";  // ok, yaw only
			// todo  scale, new way
			obj << odefName +"\n";
		}
	}
	obj.close();

	gui->Exp(CGui::NOTE, "Objects: "+toStr(sc->objects.size())+"  odef: "+toStr(iodef)+"  meshes: "+toStr(iObjMesh) + "\n");
}
