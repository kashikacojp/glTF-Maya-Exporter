#ifdef _MSC_VER
#pragma warning(disable:4819)
#endif

#ifdef _MSC_VER
#pragma comment( lib, "OpenMaya" )
#pragma comment( lib, "OpenMayaAnim" ) 
#pragma comment( lib, "OpenMayaUI" )
#pragma comment( lib, "Shell32.lib" )
#endif

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#define _CRT_SECURE_NO_WARNINGS
#else // linux / macOS
#include <libgen.h>
#endif

#include <picojson/picojson.h>

#include "glTFExporter.h"

#include <kml/Mesh.h>
#include <kml/Node.h>
#include <kml/TriangulateMesh.h>
#include <kml/CalculateNormalsMesh.h>
#include <kml/FlatIndicesMesh.h>
#include <kml/NodeExporter.h>
#include <kml/glTFExporter.h>
#include <kml/CalculateBound.h>
#include <kml/SplitNodeByMaterialID.h>
#include <kml/Options.h>
#include <kml/GLTF2GLB.h>
#include <kml/Texture.h>

#include <kil/CopyTextureFile.h>
#include <kil/ResizeTextureFile.h>

#include <maya/MStatus.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MObjectArray.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MDistance.h>
#include <maya/MIntArray.h>
#include <maya/MIOStream.h>
#include <maya/MItDag.h>
#include <maya/MObjectHandle.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MFnLambertShader.h>
#include <maya/MFnPhongEShader.h>
#include <maya/MFnTransform.h>
#include <maya/MFnIkJoint.h>
#include <maya/MVector.h>
#include <maya/MQuaternion.h>


#include <maya/MMatrix.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MStreamUtils.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MItGeometry.h>
#include <maya/MFnMatrixData.h>

#include <memory>
#include <sstream>
#include <fstream>
#include <set>
#include <iostream>

#include <string.h> 
#include <sys/types.h>

#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <Shellapi.h>
#endif

#include <thread>

#include "murmur3.h"
#include "ProgressWindow.h"

#if defined (_WIN32)
//#define strcasecmp stricmp
#elif defined  (OSMac_)
#if defined(MAC_CONVERT_FILE)
#include <sys/param.h>
#	if USING_MAC_CORE_LIB
#		include <Files.h>
#		include <CFURL.h>
		extern "C" int strcasecmp (const char *, const char *);
		extern "C" Boolean createMacFile (const char *fileName, FSRef *fsRef, long creator, long type);
#	endif
#endif
#endif

#define NO_SMOOTHING_GROUP      -1
#define INITIALIZE_SMOOTHING    -2
#define INVALID_ID              -1


static
std::string GetDirectoryPath(const std::string& path)
{
#ifdef _WIN32
	char szDrive[_MAX_DRIVE];
	char szDir[_MAX_DIR];
	_splitpath(path.c_str(), szDrive, szDir, NULL, NULL);
	std::string strRet1;
	strRet1 += szDrive;
	strRet1 += szDir;
	return strRet1;
#else
	return path.substr(0, path.find_last_of('/')) + "/";
#endif
}

static
std::string GetFileName(const std::string& path)
{
#ifdef _WIN32
	char szDrive[_MAX_DRIVE];
	char szDir[_MAX_DIR];
	char szFname[_MAX_FNAME];
	_splitpath(path.c_str(), NULL, NULL, szFname, NULL);
	std::string strRet1;
	strRet1 += szFname;
	return strRet1;
#else
	return path.substr(path.find_last_of('/') + 1, path.find_last_of('.'));
#endif
}

static
std::string GetFileExtName(const std::string& path)
{
#ifdef _WIN32
	char szFname[_MAX_FNAME];
	char szExt[_MAX_EXT];
	_splitpath(path.c_str(), NULL, NULL, szFname, szExt);
	std::string strRet1;
	strRet1 += szFname;
	strRet1 += szExt;
	return strRet1;
#else
	int i = path.find_last_of('/');
	if (i != std::string::npos)
	{
		return path.substr(i + 1);
	}
	else
	{
		return path;
	}
#endif
}

static
std::string RemoveExt(const std::string& path)
{
#ifdef _WIN32
	char szDrive[_MAX_DRIVE];
	char szDir[_MAX_DIR];
	char szFname[_MAX_FNAME];
	_splitpath(path.c_str(), szDrive, szDir, szFname, NULL);
	std::string strRet1;
	strRet1 += szDrive;
	strRet1 += szDir;
	strRet1 += szFname;
	return strRet1;
#else
	return path.substr(0, path.find_last_of('.'));
#endif

}

static
char ReplaceForPath(char c)
{
	if (c == '|')return '_';
	if (c == '\\')return '_';
	if (c == '/')return '_';
	if (c == ':')return '_';
	if (c == '*')return '_';
	if (c == '?')return '_';
	if (c == '"')return '_';
	if (c == '<')return '_';
	if (c == '>')return '_';

	return c;
}

static
std::string ReplaceForPath(const std::string& path)
{
	std::stringstream ss;

	for (size_t i = 0; i < path.size(); i++)
	{
		ss << ReplaceForPath(path[i]);
	}
	return ss.str();
}

static
char ToChar(int n)
{
	n = std::abs(n) % 50;
	if (0 <= n && n < 25)
	{
		return 'a' + n;
	}
	else
	{
		return 'A' + (n - 25);
	}
}

static
std::string MakeForPath(const std::string& path)
{
	static const int MAX_FNAME = 32;// _MAX_FNAME 256 / 8
	static const uint32_t SEED = 42;
	if (path.size() <= MAX_FNAME)
	{
		return path;
	}
	else
	{
		char buffer[16 + 1] = {};
		MurmurHash3_x64_128(path.c_str(), path.size(), SEED, (void*)buffer);
		for (int k = 0; k < 16; k++)
		{
			buffer[k] = ToChar(buffer[k]);
		}
		std::string path2 = buffer;
		return path2;
	}
}

static
std::string MakeDirectoryPath(const std::string& path)
{
	std::string tpath = ReplaceForPath(path);
	tpath = MakeForPath(tpath);
	tpath = ReplaceForPath(tpath);
	return tpath;
}

static
void MakeDirectory(const std::string& path)
{
#ifdef _WIN32
	::CreateDirectoryA(path.c_str(), NULL);
#else
  mode_t mode = 0755;
  int ret = mkdir(path.c_str(), mode);
  if (ret == 0) {
    // ok
    return;
  }
  std::cerr << "Directory creation error : " << path << std::endl;
#endif
}

static
std::string GetExt(const std::string& filepath)
{
	if (filepath.find_last_of(".") != std::string::npos)
		return filepath.substr(filepath.find_last_of("."));
	return "";
}

static
std::string MakeConvertTexturePath(const std::string& path)
{
	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	int convert_texture_format = opts->GetInt("convert_texture_format");
	switch (convert_texture_format)
	{
	case 0:return path;
	case 1:return RemoveExt(path) + ".jpg";
	case 2:return RemoveExt(path) + ".png";
	}
	return path;
}

static
bool RemoveFile(const std::string& path)
{
#ifdef _WIN32
	::DeleteFileA(path.c_str());
	return true;
#else

	return true;
#endif
}

static
bool RemoveDirectory_(const std::string& path)
{
#ifdef _WIN32
	std::string cmd;
	cmd += "rmdir /Q /S";
	cmd += " ";
	cmd += "\"";
	cmd += path;
	cmd += "\"";
	::system(cmd.c_str());
	return true;
#else
	return true;
#endif
}

static
std::string GetTempDirectory()
{
#ifdef _WIN32
	char dir1[_MAX_PATH + 1] = {};
	char dir2[_MAX_PATH + 1] = {};
	GetTempPathA(_MAX_PATH + 1, dir1);

	GetTempFileNameA(dir1, "tmp", 0, dir2);
	RemoveFile(dir2);

	return RemoveExt(dir2);
#else // Linux and macOS
	const char* tmpdir = getenv("TMPDIR");
	return std::string(tmpdir);
#endif
}

static
bool IsFileExist(const std::string& filepath)
{
	std::ifstream ifs(filepath);
	return ifs.is_open();
}

//////////////////////////////////////////////////////////////

void* glTFExporter::creator()
{
    return new glTFExporter();
}

//////////////////////////////////////////////////////////////

MStatus glTFExporter::reader ( const MFileObject& file,
                                const MString& options,
                                FileAccessMode mode)
{
    fprintf(stderr, "glTFExporter::reader called in error\n");
    return MS::kFailure;
}

//////////////////////////////////////////////////////////////
#if defined (OSMac_) && defined(MAC_CONVERT_FILE)
// Convert file system representations
// Possible styles: kCFURLHFSPathStyle, kCFURLPOSIXPathStyle
// kCFURLHFSPathStyle = Emerald:aw:Maya:projects:default:scenes:eagle.ma
// kCFURLPOSIXPathStyle = /Volumes/Emerald/aw/Maya/projects/default/scenes/eagle.ma
// The conversion will be done in place, so make sure fileName is big enough
// to hold the result
//
static Boolean
convertFileRepresentation (char *fileName, short inStyle, short outStyle)
{
	if (fileName == NULL) {
		return (false);
	}
	if (inStyle == outStyle) {
		return (true);
	}

	CFStringRef rawPath = CFStringCreateWithCString (NULL, fileName, kCFStringEncodingUTF8);
	if (rawPath == NULL) {
		return (false);
	}
	CFURLRef baseURL = CFURLCreateWithFileSystemPath (NULL, rawPath, (CFURLPathStyle)inStyle, false);
	CFRelease (rawPath);
	if (baseURL == NULL) {
		return (false);
	}
	CFStringRef newURL = CFURLCopyFileSystemPath (baseURL, (CFURLPathStyle)outStyle);
	CFRelease (baseURL);
	if (newURL == NULL) {
		return (false);
	}
	char newPath[MAXPATHLEN];
	CFStringGetCString (newURL, newPath, MAXPATHLEN, kCFStringEncodingUTF8);
	CFRelease (newURL);
	strcpy (fileName, newPath);
	return (true);
}
#endif

//////////////////////////////////////////////////////////////

bool glTFExporter::haveReadMethod () const
{
    return false;
}
//////////////////////////////////////////////////////////////

bool glTFExporter::haveWriteMethod () const
{
    return true;
}
//////////////////////////////////////////////////////////////

MString glTFExporter::defaultExtension () const
{
    return "gltf";
}
//////////////////////////////////////////////////////////////

MPxFileTranslator::MFileKind glTFExporter::identifyFile (
                                        const MFileObject& fileName,
                                        const char* buffer,
                                        short size) const
{
    const char * name = fileName.name().asChar();
    int   nameLength = (int)strlen(name);
    
	if ((nameLength > 5) && !strcasecmp(name + nameLength - 5, ".gltf"))
	{
		return kIsMyFileType;
	}
	else if ((nameLength > 4) && !strcasecmp(name + nameLength - 4, ".glb"))
	{
		return kIsMyFileType;
	}
	else if ((nameLength > 4) && !strcasecmp(name + nameLength - 4, ".vrm"))
	{
		return kIsMyFileType;
	}
	else
	{
		return kNotMyFileType;
	}
}


MStatus glTFExporter::writer ( const MFileObject& file, const MString& options, FileAccessMode mode )

{
    MStatus status;
	MString mname = file.fullName(), unitName;
    
#if defined (OSMac_) && defined (MAC_CONVERT_FILE)
	char fname[MAXPATHLEN];
	strcpy (fname, file.fullName().asChar());
	FSRef notUsed;
	//Create a file else convertFileRep will fail.
	createMacFile (fname, &notUsed, 0, 0);
	convertFileRepresentation (fname, kCFURLPOSIXPathStyle, kCFURLHFSPathStyle);
#else
    const char *fname = mname.asChar();
#endif

#if 1
    // Options
    //
	int recalc_normals = 0;			//0:no, 1:recalc
	int output_onefile = 1;			//0:sep, 1:one file
	int output_glb = 0;				//0:json,1:glb
	int vrm_export = 0;             //0:gltf or glb, 1:vrm
	int make_preload_texture = 0;	//0:off, 1:on
	int output_buffer = 1;			//0:bin, 1:draco, 2:bin/draco
	int convert_texture_format = 0; //0:no convert, 1:jpeg, 2:png
	int transform_space = 1;		//0:world_space, 1:local_space
    int bake_mesh_transform = 0;    //0:no_bake, 1:bake

	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
    
	if (options.length() > 0)
	{
		int i, length;
		// Start parsing.
		MStringArray optionList;
		MStringArray theOption;
		options.split(';', optionList); // break out all the options.

		length = optionList.length();
		for( i = 0; i < length; ++i )
		{
			theOption.clear();
			optionList[i].split( '=', theOption );

			if( theOption[0] == MString("recalc_normals") && theOption.length() > 1 ) {
				recalc_normals = theOption[1].asInt();
			}
			if (theOption[0] == MString("output_onefile") && theOption.length() > 1) {
				output_onefile = theOption[1].asInt();
			}
			if (theOption[0] == MString("output_glb") && theOption.length() > 1) {
				output_glb = theOption[1].asInt();
			}
			if (theOption[0] == MString("vrm_export") && theOption.length() > 1) {
				vrm_export = theOption[1].asInt();
			}
			if (theOption[0] == MString("make_preload_texture") && theOption.length() > 1) {
				make_preload_texture = theOption[1].asInt();
			}
			if (theOption[0] == MString("output_buffer") && theOption.length() > 1) {
				output_buffer = theOption[1].asInt();
			}
			if (theOption[0] == MString("convert_texture_format") && theOption.length() > 1) {
				convert_texture_format = theOption[1].asInt();
			}
			if (theOption[0] == MString("transform_space") && theOption.length() > 1) {
				transform_space = theOption[1].asInt();
			}
            if (theOption[0] == MString("bake_mesh_transform") && theOption.length() > 1) {
                bake_mesh_transform = theOption[1].asInt();
            }
		}
	}

	if (vrm_export) {
		output_buffer = 0; // disable Draco
		output_glb = 1;    // force GLB format
	}

	if (output_glb)
	{
		output_onefile = 1;			//enforce onefile
		make_preload_texture = 0;	//no cache texture
	}

	opts->SetInt("recalc_normals", recalc_normals);
	opts->SetInt("output_onefile", output_onefile);
	opts->SetInt("output_glb", output_glb);
	opts->SetInt("make_preload_texture", make_preload_texture);
	opts->SetInt("output_buffer", output_buffer);
	opts->SetInt("convert_texture_format", convert_texture_format);
	opts->SetInt("transform_space", transform_space);
    opts->SetInt("bake_mesh_transform", bake_mesh_transform);
	opts->SetInt("vrm_export", vrm_export);

    /* print current linear units used as a comment in the obj file */
    //setToLongUnitName(MDistance::uiUnit(), unitName);
    //fprintf( fp, "# This file uses %s as units for non-parametric coordinates.\n\n", unitName.asChar() ); 
    //fprintf( fp, "# The units used in this file are %s.\n", unitName.asChar() );
#endif
    if( ( mode == MPxFileTranslator::kExportAccessMode ) ||
        ( mode == MPxFileTranslator::kSaveAccessMode ) )
    {
        return exportAll(fname);
    }
    else if( mode == MPxFileTranslator::kExportActiveAccessMode )
    {
        return exportSelected(fname);
    }

    return MS::kSuccess;
}

//////////////////////////////////////////////////////////////

static
std::vector<std::string> SplitPath(const std::string& str, const std::string& delim)
{
	std::vector<std::string> tokens;
	size_t prev = 0, pos = 0;
	do
	{
		pos = str.find(delim, prev);
		if (pos == std::string::npos) pos = str.length();
		std::string token = str.substr(prev, pos - prev);
		if (!token.empty()) tokens.push_back(token);
		prev = pos + delim.length();
	} while (pos < str.length() && prev < str.length());
	return tokens;
}


static
std::vector<MDagPath> GetDagPathList(const std::string& fullPathNameh)
{
	std::vector<MDagPath> pathList;
	std::vector<std::string> tokens = SplitPath(fullPathNameh.c_str() + 1, "|");

	std::vector<std::string> paths;
	for (int i = 0; i < tokens.size(); i++)
	{
		std::string path = "";
		for (int j = 0; j <= i; j++)
		{
			path += "|";
			path += tokens[j];
		}
		paths.push_back(path);
	}

	MSelectionList selectionList;
	for (int i = 0; i < paths.size(); i++)
	{
		selectionList.add(MString(paths[i].c_str()));
	}
	MItSelectionList iterator = MItSelectionList(selectionList, MFn::kDagNode);
	while (!iterator.isDone())
	{
		MDagPath path;
		iterator.getDagPath(path);
		pathList.push_back(path);
		iterator.next();
	}

	return pathList;
}


static
std::vector<MDagPath> GetDagPathList(const MDagPath& mdagPath)
{
	return GetDagPathList(mdagPath.fullPathName().asChar());
}

static
MObject GetOriginalMesh(const MDagPath& dagpath)
{
	MFnDependencyNode node(dagpath.node());
	MPlugArray mpa;
	MPlug mp = node.findPlug("inMesh");
	mp.connectedTo(mpa, true, false);

	MObject vtxobj = MObject::kNullObj;
	int isz = mpa.length();
	if (isz < 1)
		return vtxobj;

	MFnDependencyNode dnode(mpa[0].node());
	if (dnode.typeName() == "skinCluster")
	{
		MFnSkinCluster skinC(mpa[0].node());
		skinC.findPlug("input").elementByLogicalIndex(skinC.indexForOutputShape(dagpath.node())).child(0).getValue(vtxobj);
	}
	return vtxobj;
}

static
std::shared_ptr<kml::Mesh> CreateMesh(const MDagPath& mdagPath, const MSpace::Space& space)
{
	MStatus status = MS::kSuccess;

    MFnMesh fnMesh(mdagPath);
    MItMeshPolygon polyIter(mdagPath);
    MItMeshVertex  vtxIter(mdagPath);

	// Write out the vertex table
	//
	std::vector<glm::vec3> positions;
	for (; !vtxIter.isDone(); vtxIter.next())
	{
		MPoint p = vtxIter.position(space);
		/*
		if (ptgroups && groups && (objectIdx >= 0)) {
		int compIdx = vtxIter.index();
		outputSetsAndGroups( mdagPath, compIdx, true, objectIdx );
		}
		*/
		// convert from internal units to the current ui units
		p.x = MDistance::internalToUI(p.x);
		p.y = MDistance::internalToUI(p.y);
		p.z = MDistance::internalToUI(p.z);

		positions.push_back(glm::vec3(p.x, p.y, p.z));
	}

	// Write out the uv table
	//
	std::vector<glm::vec2> texcoords;
	MFloatArray uArray, vArray;
	fnMesh.getUVs(uArray, vArray);
	int uvLength = uArray.length();
	for (int x = 0; x < uvLength; x++)
	{
		float u = uArray[x];
		float v = 1.0f - vArray[x];
		texcoords.push_back(glm::vec2(u, v));
	}

	// Write out the normal table
	//
	std::vector<glm::vec3> normals;
	{
		MFloatVectorArray norms;
		MStatus stat = fnMesh.getNormals(norms, space);
		if (stat == MStatus::kSuccess)
		{
			int normsLength = norms.length();
			for (int t = 0; t<normsLength; t++)
			{
				MFloatVector tmpf = norms[t];
				normals.push_back(glm::vec3(tmpf.x, tmpf.y, tmpf.z));
			}
		}
	}

	// For each polygon, write out: 
	//    s  smoothing_group
	//    sets/groups the polygon belongs to 
	//    f  vertex_index/uvIndex/normalIndex
	//
	//int lastSmoothingGroup = INITIALIZE_SMOOTHING;

	std::vector<int> pos_indices;
	std::vector<int> tex_indices;
	std::vector<int> nor_indices;
	std::vector<unsigned char> facenums;
	for (; !polyIter.isDone(); polyIter.next())
	{

		// Write out vertex/uv/normal index information
		//
		int polyVertexCount = polyIter.polygonVertexCount();
		facenums.push_back(polyVertexCount);
		for (int vtx = 0; vtx < polyVertexCount; vtx++)
		{
			int pidx = polyIter.vertexIndex(vtx);
			int tidx = -1;
			int nidx = -1;
			if (fnMesh.numUVs() > 0)
			{
				int uvIndex;
				if (polyIter.getUVIndex(vtx, uvIndex))
				{
					tidx = uvIndex;
				}
			}

			if (fnMesh.numNormals() > 0)
			{
				nidx = polyIter.normalIndex(vtx);
			}
			pos_indices.push_back(pidx);
			tex_indices.push_back(tidx);
			nor_indices.push_back(nidx);
		}
	}

	std::vector<int> materials(facenums.size());
	for (size_t i = 0; i < materials.size(); i++)
	{
		materials[i] = 0;
	}


	std::shared_ptr < kml::Mesh > mesh(new kml::Mesh());
	mesh->facenums.swap(facenums);
	mesh->pos_indices.swap(pos_indices);
	mesh->tex_indices.swap(tex_indices);
	mesh->nor_indices.swap(nor_indices);
	mesh->positions.swap(positions);
	mesh->texcoords.swap(texcoords);
	mesh->normals.swap(normals);
	mesh->materials.swap(materials);
	//mesh->name = mdagPath.partialPathName().asChar();

	return mesh;
}

static
std::shared_ptr<kml::Mesh> GetOriginalVertices(std::shared_ptr<kml::Mesh>& mesh, MObject& orgMeshObj)
{
	MFnMesh fnMesh(orgMeshObj);
	MPointArray vtx_pos;
	fnMesh.getPoints(vtx_pos, MSpace::kObject);
	MFloatVectorArray norm;
	fnMesh.getNormals(norm, MSpace::kObject);

	std::vector<glm::vec3> positions;
	for(int i = 0; i < vtx_pos.length(); i++)
	{
		MPoint p = vtx_pos[i];

		p.x = MDistance::internalToUI(p.x);
		p.y = MDistance::internalToUI(p.y);
		p.z = MDistance::internalToUI(p.z);

		positions.push_back(glm::vec3(p.x, p.y, p.z));
	}

	std::vector<glm::vec3> normals;
	for (int i = 0; i < norm.length(); i++)
	{
		MFloatVector n = norm[i];
		normals.push_back(glm::vec3(n.x, n.y, n.z));
	}

	mesh->positions.swap(positions);
	mesh->normals.swap(normals);

	return mesh;
}

static
std::shared_ptr<kml::Mesh> GetSkinWeights(std::shared_ptr<kml::Mesh>& mesh, const MDagPath& dagpath)
{
	MFnDependencyNode node(dagpath.node());
	MPlugArray mpa;
	MPlug mp = node.findPlug("inMesh");
	mp.connectedTo(mpa, true, false);

	MObject vtxobj = MObject::kNullObj;
	int isz = mpa.length();
	if (isz < 1)
	{
		return mesh;
	}

	MFnDependencyNode dnode(mpa[0].node());
	if (dnode.typeName() != "skinCluster")
	{
		return mesh;
	}

	MFnSkinCluster skinC(mpa[0].node());
	skinC.findPlug("input").elementByLogicalIndex(skinC.indexForOutputShape(dagpath.node())).child(0).getValue(vtxobj);

	std::shared_ptr<kml::SkinWeights> skin_weights(new kml::SkinWeights());
    skin_weights->name = skinC.name().asChar();
	skin_weights->weights.resize(mesh->positions.size());
	MDagPathArray dpa;
	MStatus stat;
	int jsz = skinC.influenceObjects(dpa, &stat);
	int ksz = skinC.numOutputConnections();
	//assert(ksz == 1); // maybe 1 only
	ksz = std::min<int>(ksz, 1);//TODO
	for (int k = 0; k < ksz; k++)
	{
		const unsigned int index = skinC.indexForOutputConnection(k);

		MDagPath dp;
		skinC.getPathAtIndex(index, dp);

		std::string weight_table_name = dp.partialPathName().asChar();

		std::vector<std::string> joint_paths;
		for (int j = 0; j < jsz; j++)
		{
			std::string joint_path = dpa[j].fullPathName().asChar();
            joint_paths.push_back(joint_path);
		}

		{
			MItGeometry geoit(dagpath);
			int weightVnum = geoit.count();

			while (!geoit.isDone())
			{
				MObject cp = geoit.component(); // per vertex
				MFloatArray ws;
				unsigned int lsz;
				skinC.getWeights(dp, cp, ws, lsz);
				if (lsz != 0)
				{
					unsigned int index = geoit.index();
					for (int l = 0; l < lsz; l++)
					{
						skin_weights->weights[index][joint_paths[l]] = ws[l];
					}
				}
				geoit.next();
			}
		}

        struct StringSizeSorter
        {
            bool operator()(const std::string& a, const std::string& b)const
            {
                return a.size() < b.size();
            }
        };

        std::sort(joint_paths.begin(), joint_paths.end());
        joint_paths.erase(std::unique(joint_paths.begin(), joint_paths.end()), joint_paths.end());
        std::sort(joint_paths.begin(), joint_paths.end(), StringSizeSorter());
        skin_weights->joint_paths = joint_paths;
	}

    {
        std::map<std::string, glm::mat4> path_mat_map;
        MPlug mp_m = skinC.findPlug("bindPreMatrix", &stat);
        for (int j = 0; j < jsz; j++)
        {
            std::string joint_path = dpa[j].fullPathName().asChar();
            MPlug mp2 = mp_m.elementByLogicalIndex(skinC.indexForInfluenceObject(dpa[j], NULL), &stat);

            MObject obj;
            mp2.getValue(obj);
            MFnMatrixData matData(obj);
            MMatrix mmat = matData.matrix();
            double dest[4][4];
            mmat.get(dest);
            glm::mat4 mat(
                dest[0][0], dest[0][1], dest[0][2], dest[0][3],
                dest[1][0], dest[1][1], dest[1][2], dest[1][3],
                dest[2][0], dest[2][1], dest[2][2], dest[2][3],
                dest[3][0], dest[3][1], dest[3][2], dest[3][3]
            );
            path_mat_map[joint_path] = mat;
        }

        for(int j = 0; j < skin_weights->joint_paths.size(); j++)
        { 
            glm::mat4 mat = path_mat_map[skin_weights->joint_paths[j]];
            skin_weights->joint_bind_matrices.push_back(mat);
        }
    }

	mesh->skin_weights = skin_weights;

	return mesh;
}

static
glm::vec3 Transform(const glm::mat4& mat, const glm::vec3& v)
{
    glm::vec4 t = mat * glm::vec4(v, 1.0f);
    t[0] /= t[3];
    t[1] /= t[3];
    t[2] /= t[3];
    return glm::vec3(t[0], t[1], t[2]);
}

static
std::shared_ptr<kml::Mesh> TransformMesh(std::shared_ptr<kml::Mesh>& mesh, const glm::mat4& mat)
{
    auto& positions = mesh->positions;
    for (size_t i = 0; i < positions.size(); i++)
    {
        positions[i] = Transform(mat, positions[i]);
    }
    glm::mat4 im = glm::transpose(glm::inverse(mat));
    im[3][0] = 0.0f;
    im[3][1] = 0.0f;
    im[3][2] = 0.0f;
    auto& normals = mesh->normals;
    for (size_t i = 0; i < normals.size(); i++)
    {
        normals[i] = glm::normalize(Transform(im, normals[i]));
    }
    return mesh;
}


static
std::shared_ptr<kml::Node> CreateMeshNode(const MDagPath& mdagPath)
{
	MStatus status = MS::kSuccess;
	
	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	int transform_space = opts->GetInt("transform_space");
    bool bake_mesh_transform = opts->GetInt("bake_mesh_transform") > 0;

	MSpace::Space space = MSpace::kWorld;
	if (transform_space == 1)
	{
		space = MSpace::kObject;
	}
	
	std::shared_ptr<kml::Mesh> mesh = CreateMesh(mdagPath, space);

	if (!mesh.get())
	{
		return std::shared_ptr<kml::Node>();
	}

	if (transform_space == 1)
	{
		MObject orgMeshObj = GetOriginalMesh(mdagPath);//T-pose
		if (orgMeshObj.hasFn(MFn::kMesh))
		{
			mesh = GetOriginalVertices(mesh, orgMeshObj);	//dynamic
			mesh = GetSkinWeights(mesh, mdagPath);			//dynamic
		}
	}

	mesh->name = mdagPath.partialPathName().asChar();

	std::shared_ptr < kml::Node > node(new kml::Node());
	
	if (transform_space == 0)
	{
		node->GetTransform()->SetMatrix(glm::mat4(1.0f));
	}
	else if (transform_space == 1)
	{
        std::vector<MDagPath> dagPathList = GetDagPathList(mdagPath);
        dagPathList.pop_back();//shape

        MFnTransform fnTransform(dagPathList.back());
        MMatrix mmat = fnTransform.transformationMatrix();//local
        double dest[4][4];
        mmat.get(dest);
        glm::mat4 mat(
            dest[0][0], dest[0][1], dest[0][2], dest[0][3],
            dest[1][0], dest[1][1], dest[1][2], dest[1][3],
            dest[2][0], dest[2][1], dest[2][2], dest[2][3],
            dest[3][0], dest[3][1], dest[3][2], dest[3][3]
        );

        if (!bake_mesh_transform)
        {
            node->GetTransform()->SetMatrix(mat);
        }
        else
        {
            mesh = TransformMesh(mesh, mat);
            node->GetTransform()->SetMatrix(glm::mat4(1.0f));
        }
	}
	node->SetMesh(mesh);
	node->SetBound(kml::CalculateBound(mesh));

	if (transform_space == 0)
	{
		std::vector<MDagPath> dagPathList = GetDagPathList(mdagPath);
		dagPathList.pop_back();//shape
		MDagPath transPath = dagPathList.back();
		node->SetName(transPath.partialPathName().asChar());
		node->SetPath(transPath.fullPathName().asChar());

		return node;
	}
	else
	{
		std::vector<MDagPath> dagPathList = GetDagPathList(mdagPath);
		dagPathList.pop_back();//shape
		MDagPath transPath = dagPathList.back();
		node->SetName(transPath.partialPathName().asChar());
		node->SetPath(transPath.fullPathName().asChar());

		dagPathList.pop_back();//transform
		if (dagPathList.size() > 0)
		{
			std::vector< std::shared_ptr<kml::Node> > nodes;
			for (size_t i = 0; i < dagPathList.size(); i++)
			{
				MDagPath path = dagPathList[i];
				std::shared_ptr < kml::Node > n(new kml::Node());
				n->SetName(path.partialPathName().asChar());
				n->SetPath(path.fullPathName().asChar());

                MFnTransform fnTransform(path);
                MMatrix mmat = fnTransform.transformationMatrix();//local
				double dest[4][4];
				mmat.get(dest);
				glm::mat4 mat(
					dest[0][0], dest[0][1], dest[0][2], dest[0][3],
					dest[1][0], dest[1][1], dest[1][2], dest[1][3],
					dest[2][0], dest[2][1], dest[2][2], dest[2][3],
					dest[3][0], dest[3][1], dest[3][2], dest[3][3]
				);
				n->GetTransform()->SetMatrix(mat);
				nodes.push_back(n);
			}

			for (size_t i = 0; i < nodes.size() - 1; i++)
			{
				nodes[i]->AddChild(nodes[i + 1]);
			}
			nodes.back()->AddChild(node);

			return nodes[0];
		}
		else
		{
			return node;
		}
	}

	return node;
}

//////////////////////////////////////////////////////////////


static
void GetMaterials(std::vector<MObject>& shaders, std::vector<int>& faceIndices, const MDagPath& dagPath)
{
	typedef std::map<unsigned int, MObject> MapType;
	MapType shader_map;
	{
		MStatus status = MStatus::kSuccess;
		MFnMesh fnMesh(dagPath, &status);
		if (MS::kSuccess != status)return;

		int numInstances = 1;// fnMesh.parentCount();
		for (int j = 0; j < numInstances; j++)
		{
			MObjectArray Shaders;
			MIntArray    FaceIndices;
			fnMesh.getConnectedShaders(j, Shaders, FaceIndices);
			for (int k = 0; k < Shaders.length(); k++)
			{
				//unsigned int key = MObjectHandle::objectHashCode(Shaders[k]);
				//shader_map[key] = Shaders[k];
				shaders.push_back(Shaders[k]);
			}
			for (int k = 0; k < FaceIndices.length(); k++)
			{
				faceIndices.push_back(FaceIndices[k]);
			}
		}
	}
}

static
MColor getColor(MFnDependencyNode& node, const char* name)
{
	MPlug p;
	MString r = MString(name) + "R";
	MString g = MString(name) + "G";
	MString b = MString(name) + "B";
	MString a = MString(name) + "A";
	MColor color;
	p = node.findPlug(r.asChar());
	p.getValue(color.r);
	p = node.findPlug(g.asChar());
	p.getValue(color.g);
	p = node.findPlug(b.asChar());
	p.getValue(color.b);
	p = node.findPlug(a.asChar());
	p.getValue(color.a);
	p = node.findPlug(name);
	return color;
}



static
bool getTextureAndColor(const MFnDependencyNode& node, const MString& name, std::shared_ptr<kml::Texture>& tex, MColor& color)
{
	color = MColor(1.0, 1.0, 1.0, 1.0);
	MStatus status;
	MPlug paramPlug = node.findPlug(name, &status);
	if (status != MS::kSuccess)
		return false;

	MPlugArray connectedPlugs;
	if (paramPlug.connectedTo(connectedPlugs, true, false, &status)
		&& connectedPlugs.length()) {
		MFn::Type apiType = connectedPlugs[0].node().apiType();
		if (apiType == MFn::kFileTexture) {
			MFnDependencyNode texNode(connectedPlugs[0].node());
			MPlug texturePlug = texNode.findPlug("fileTextureName", &status);
			if (status == MS::kSuccess)
			{
				tex = std::shared_ptr<kml::Texture>(new kml::Texture);
				MString tpath;
				texturePlug.getValue(tpath);
				std::string texpath = tpath.asChar();
				
				// project path delimitor
				const std::string pathDelimiter = "//";
				size_t delim = texpath.find(pathDelimiter);
				if (delim != std::string::npos) {
					texpath.erase(0, delim + pathDelimiter.size());
				}
				tex->SetFilePath(texpath);
			
				// filter 
				const int filterType = texNode.findPlug("filter").asInt();
				if (filterType == 0) { // None
					tex->SetFilter(kml::Texture::FILTER_NEAREST);
				} else { // Linear
					tex->SetFilter(kml::Texture::FILTER_LINEAR);
				}

				// repeart
				const float repeatU = texNode.findPlug("repeatU").asFloat();
				const float repeatV = texNode.findPlug("repeatV").asFloat();
				tex->SetRepeat(repeatU, repeatV);

				// offset 
				const float offsetU = texNode.findPlug("offsetU").asFloat();
				const float offsetV = texNode.findPlug("offsetV").asFloat();
				tex->SetRepeat(offsetU, offsetV);

				// wrap
				const bool wrapU = texNode.findPlug("wrapU").asBool();
				const bool wrapV = texNode.findPlug("wrapV").asBool();
				tex->SetWrap(wrapU, wrapV);

				// UDIM
				const int tilingMode = texNode.findPlug("uvTilingMode").asInt();
				if (tilingMode == 0) { // OFF
					tex->SetUDIMMode(false);
				} else if (tilingMode == 3) { // UDIM
					tex->SetUDIMMode(true);
					
					// get <UDIM> tag filepath
					MPlug compNamePlug = texNode.findPlug("computedFileTextureNamePattern", &status);
					MString ctpath;
					compNamePlug.getValue(ctpath);
					std::string ctexpath = ctpath.asChar();
					tex->SetUDIMFilePath(ctexpath);

				} else { // Not Support
					fprintf(stderr, "Error: Not support texture tiling mode.\n");
				}

				// if material has texture, set color(1,1,1)
				color.r = 1.0f;
				color.g = 1.0f;
				color.b = 1.0f;
				return true;
			}
			return false;
		}
		else {
			// other type node is not supported
			return false;
		}
	}
	paramPlug.child(0).getValue(color.r);
	paramPlug.child(1).getValue(color.g);
	paramPlug.child(2).getValue(color.b);
	return true;
}

static bool getNormalAttrib(const MFnDependencyNode& node, MString& normaltexpath, float& depth)
{
	depth = 0.0f;
	normaltexpath = "";

	MStatus status;
	MPlug paramPlug;
	paramPlug = node.findPlug("normalCamera", &status); // normalmap
	if (status != MS::kSuccess)
		return false;

	MPlugArray connectedPlugs;
	bool isConnectedPlug = paramPlug.connectedTo(connectedPlugs, true, false, &status) && connectedPlugs.length();
	if (!isConnectedPlug)
		return false;

	MFn::Type apiType = connectedPlugs[0].node().apiType();
	if (apiType == MFn::kBump) {
		MFnDependencyNode bumpNode(connectedPlugs[0].node());
		MPlug bumpPlug = bumpNode.findPlug("bumpDepth", &status);
		if (status != MS::kSuccess)
			return false;
		bumpPlug.getValue(depth);

		paramPlug = bumpNode.findPlug("bumpValue", &status);
		if (status != MS::kSuccess)
			return false;

		MString pn = paramPlug.name();
		bool isConnectedTex = paramPlug.connectedTo(connectedPlugs, true, false, &status) && connectedPlugs.length();
		if (!isConnectedTex)
			return false;

		if (connectedPlugs[0].node().apiType() != MFn::kFileTexture)
			return false;

		MFnDependencyNode texNode(connectedPlugs[0].node());
		MString na = texNode.name();
		MPlug texPlug = texNode.findPlug("fileTextureName", &status);
		if (status != MS::kSuccess)
			return false;

		texPlug.getValue(normaltexpath);

		return true;
	}
	else if (apiType == MFn::kFileTexture) {
		MFnDependencyNode texNode(connectedPlugs[0].node());
		MPlug texturePlug = texNode.findPlug("fileTextureName", &status);
		if (status != MS::kSuccess) {
			return false;
		}

		texturePlug.getValue(normaltexpath);

		return true;
	} else {
		return false;
	}
}

static bool isStingrayPBS(const MFnDependencyNode& materialDependencyNode)
{
	MString graphAttribute("graph");
	if (materialDependencyNode.hasAttribute(graphAttribute))
	{
		MString graphValue;
		MPlug plug = materialDependencyNode.findPlug(graphAttribute);
		plug.getValue(graphValue);
		MString stingrayStr("stingray");
		return graphValue == stingrayStr;
	}
	else
	{
		return false;
	}
}
static bool storeStingrayPBS(std::shared_ptr<kml::Material> mat, const MFnDependencyNode& materialDependencyNode)
{
	// TODO
	return false;
}

static bool isAiStandardSurfaceShader(const MFnDependencyNode& materialDependencyNode)
{
	return materialDependencyNode.hasAttribute("baseColor") &&
		materialDependencyNode.hasAttribute("metalness") &&
		materialDependencyNode.hasAttribute("normalCamera") &&
		materialDependencyNode.hasAttribute("specularRoughness") &&
		materialDependencyNode.hasAttribute("emissionColor");
}

static bool storeAiStandardSurfaceShader(std::shared_ptr<kml::Material> mat, const MFnDependencyNode& ainode)
{
	// store aishader name
	std::string shadername = ainode.name().asChar();
	mat->SetName(shadername);

	// base
	const float baseWeight = ainode.findPlug("base").asFloat();
	const float baseColorR = ainode.findPlug("baseColorR").asFloat();
	const float baseColorG = ainode.findPlug("baseColorG").asFloat();
	const float baseColorB = ainode.findPlug("baseColorB").asFloat();
	const float diffuseRoughness = ainode.findPlug("diffuseRoughness").asFloat();
	const float metallic = ainode.findPlug("metalness").asFloat();
	MColor baseCol;
	std::shared_ptr<kml::Texture> baseColorTex(nullptr);
	if (getTextureAndColor(ainode, MString("baseColor"), baseColorTex, baseCol)) {
		if (baseColorTex) {
			mat->SetTexture("ai_baseColor", baseColorTex);
		}
	}
	
	mat->SetFloat("ai_baseWeight", baseWeight);
	mat->SetFloat("ai_baseColorR", baseColorR);
	mat->SetFloat("ai_baseColorG", baseColorG);
	mat->SetFloat("ai_baseColorB", baseColorB);
	mat->SetFloat("ai_diffuseRoughness", diffuseRoughness);
	mat->SetFloat("ai_metalness", metallic);

	// specular
	const float specularWeight = ainode.findPlug("specular").asFloat();
	const float specularColorR = ainode.findPlug("specularColorR").asFloat();
	const float specularColorG = ainode.findPlug("specularColorG").asFloat();
	const float specularColorB = ainode.findPlug("specularColorB").asFloat();
	const float specularRoughness = ainode.findPlug("specularRoughness").asFloat();
	const float specularIOR = ainode.findPlug("specularIOR").asFloat();
	const float specularRotation = ainode.findPlug("specularRotation").asFloat();
	const float specularAnisotropy = ainode.findPlug("specularAnisotropy").asFloat();
	MColor specularCol;
	std::shared_ptr<kml::Texture> specularTex(nullptr);
	if (getTextureAndColor(ainode, MString("specularColor"), specularTex, specularCol)) {
		if (specularTex) {
			mat->SetTexture("ai_specularColor", specularTex);
		}
	}
	mat->SetFloat("ai_specularWeight", specularWeight);
	mat->SetFloat("ai_specularColorR", specularColorR);
	mat->SetFloat("ai_specularColorG", specularColorG);
	mat->SetFloat("ai_specularColorB", specularColorB);
	mat->SetFloat("ai_specularRoughness", specularRoughness);
	mat->SetFloat("ai_specularIOR", specularIOR);
	mat->SetFloat("ai_specularRotation", specularRotation);
	mat->SetFloat("ai_specularAnisotropy", specularAnisotropy);

	// transmission
	const float transmissionWeight = ainode.findPlug("transmission").asFloat();
	const float transmissionColorR = ainode.findPlug("transmissionColorR").asFloat();
	const float transmissionColorG = ainode.findPlug("transmissionColorG").asFloat();
	const float transmissionColorB = ainode.findPlug("transmissionColorB").asFloat();
	const float transmissionDepth = ainode.findPlug("transmissionDepth").asFloat();
	const float transmissionScatterR = ainode.findPlug("transmissionScatterR").asFloat();
	const float transmissionScatterG = ainode.findPlug("transmissionScatterG").asFloat();
	const float transmissionScatterB = ainode.findPlug("transmissionScatterB").asFloat();
	const float transmissionScatterAnisotropy = ainode.findPlug("transmissionScatterAnisotropy").asFloat();
	const float transmissionDispersion = ainode.findPlug("transmissionDispersion").asFloat();
	const float transmissionExtraRoughness = ainode.findPlug("transmissionExtraRoughness").asFloat();
	const int   transmissionAovs = ainode.findPlug("transmissionAovs").asInt();
	MColor transmissionCol;
	std::shared_ptr<kml::Texture> transmissionTex(nullptr);
	if (getTextureAndColor(ainode, MString("transmissionColor"), transmissionTex, transmissionCol)) {
		if (transmissionTex) {
			mat->SetTexture("ai_transmissionColor", transmissionTex);
		}
	}
	MColor transmissionScatterCol;
	std::shared_ptr<kml::Texture> transmissionScatterTex(nullptr);
	if (getTextureAndColor(ainode, MString("transmissionScatter"), transmissionScatterTex, transmissionScatterCol)) {
		if (transmissionScatterTex) {
			mat->SetTexture("ai_transmissionScatter", transmissionScatterTex);
		}
	}
	mat->SetFloat("ai_transmissionWeight", transmissionWeight);
	mat->SetFloat("ai_transmissionColorR", transmissionColorR);
	mat->SetFloat("ai_transmissionColorG", transmissionColorG);
	mat->SetFloat("ai_transmissionColorB", transmissionColorB);
	mat->SetFloat("ai_transmissionDepth", transmissionDepth);
	mat->SetFloat("ai_transmissionScatterR", transmissionScatterR);
	mat->SetFloat("ai_transmissionScatterG", transmissionScatterG);
	mat->SetFloat("ai_transmissionScatterB", transmissionScatterB);
	mat->SetFloat("ai_transmissionScatterAnisotropy", transmissionScatterAnisotropy);
	mat->SetFloat("ai_transmissionExtraRoughness", transmissionExtraRoughness);
	mat->SetFloat("ai_transmissionDispersion", transmissionDispersion);
	mat->SetInteger("ai_transmissionAovs", transmissionAovs);

	// Subsurface
	const float subsurfaceWeight = ainode.findPlug("subsurface").asFloat();
	const float subsurfaceColorR = ainode.findPlug("subsurfaceColorR").asFloat();
	const float subsurfaceColorG = ainode.findPlug("subsurfaceColorG").asFloat();
	const float subsurfaceColorB = ainode.findPlug("subsurfaceColorB").asFloat();
	const float subsurfaceRadiusR = ainode.findPlug("subsurfaceRadiusR").asFloat();
	const float subsurfaceRadiusG = ainode.findPlug("subsurfaceRadiusG").asFloat();
	const float subsurfaceRadiusB = ainode.findPlug("subsurfaceRadiusB").asFloat();
	const float subsurfaceScale = ainode.findPlug("subsurfaceScale").asFloat();
	const int subsurfaceType = ainode.findPlug("subsurfaceType").asInt();
	const float subsurfaceAnisotropy = ainode.findPlug("subsurfaceAnisotropy").asFloat();
	MColor subsurfaceCol;
	std::shared_ptr<kml::Texture> subsurfaceTex(nullptr);
	if (getTextureAndColor(ainode, MString("subsurfaceColor"), subsurfaceTex, subsurfaceCol)) {
		if (subsurfaceTex) {
			mat->SetTexture("ai_subsurfaceColor", subsurfaceTex);
		}
	}
	MColor subsurfaceRadiusCol;
	std::shared_ptr<kml::Texture> subsurfaceRadiusTex(nullptr);
	if (getTextureAndColor(ainode, MString("subsurfaceRadius"), subsurfaceRadiusTex, subsurfaceRadiusCol)) {
		if (subsurfaceRadiusTex) {
			mat->SetTexture("ai_subsurfaceRadius", subsurfaceRadiusTex);
		}
	}
	mat->SetFloat("ai_subsurfaceWeight", subsurfaceWeight);
	mat->SetFloat("ai_subsurfaceColorR", subsurfaceColorR);
	mat->SetFloat("ai_subsurfaceColorG", subsurfaceColorG);
	mat->SetFloat("ai_subsurfaceColorB", subsurfaceColorB);
	mat->SetFloat("ai_subsurfaceRadiusR", subsurfaceRadiusR);
	mat->SetFloat("ai_subsurfaceRadiusG", subsurfaceRadiusG);
	mat->SetFloat("ai_subsurfaceRadiusB", subsurfaceRadiusB);
	mat->SetInteger("ai_subsurfaceType", subsurfaceType);
	mat->SetFloat("ai_subsurfaceScale", subsurfaceScale);
	mat->SetFloat("ai_subsurfaceAnisotropy", subsurfaceAnisotropy);

	// Coat
	const float coatWeight = ainode.findPlug("coat").asFloat();
	const float coatColorR = ainode.findPlug("coatColorR").asFloat();
	const float coatColorG = ainode.findPlug("coatColorG").asFloat();
	const float coatColorB = ainode.findPlug("coatColorB").asFloat();
	const float coatRoughness = ainode.findPlug("coatRoughness").asFloat();
	const float coatIOR = ainode.findPlug("coatIOR").asFloat();
	const float coatNormalX = ainode.findPlug("coatNormalX").asFloat();
	const float coatNormalY = ainode.findPlug("coatNormalY").asFloat();
	const float coatNormalZ = ainode.findPlug("coatNormalZ").asFloat();
	MColor coatCol;
	std::shared_ptr<kml::Texture> coatColorTex(nullptr);
	if (getTextureAndColor(ainode, MString("coatColor"), coatColorTex, coatCol)) {
		if (coatColorTex) {
			mat->SetTexture("ai_coatColor", coatColorTex);
		}
	}
	mat->SetFloat("ai_coatWeight", coatWeight);
	mat->SetFloat("ai_coatColorR", coatColorR);
	mat->SetFloat("ai_coatColorG", coatColorG);
	mat->SetFloat("ai_coatColorB", coatColorB);
	mat->SetFloat("ai_coatRoughness", coatRoughness);
	mat->SetFloat("ai_coatIOR", coatIOR);
	mat->SetFloat("ai_coatNormalX", coatNormalX);
	mat->SetFloat("ai_coatNormalY", coatNormalY);
	mat->SetFloat("ai_coatNormalZ", coatNormalZ);

	// Emissive
	const float emissionWeight = ainode.findPlug("emission").asFloat();
	const float emissionColorR = ainode.findPlug("emissionColorR").asFloat();
	const float emissionColorG = ainode.findPlug("emissionColorG").asFloat();
	const float emissionColorB = ainode.findPlug("emissionColorB").asFloat();
	MColor emissionCol;
	std::shared_ptr<kml::Texture> emissionColorTex(nullptr);
	if (getTextureAndColor(ainode, MString("emissionColor"), emissionColorTex, emissionCol)) {
		if (emissionColorTex) {
			mat->SetTexture("ai_emissionColor", emissionColorTex);
		}
	}
	mat->SetFloat("ai_emissionWeight", emissionWeight);
	mat->SetFloat("ai_emissionColorR", emissionColorR);
	mat->SetFloat("ai_emissionColorG", emissionColorG);
	mat->SetFloat("ai_emissionColorB", emissionColorB);


	// Normal map
	float depth;
	MString normaltexpath;
	if (getNormalAttrib(ainode, normaltexpath, depth))
	{
		std::string texName = normaltexpath.asChar();
		if (!texName.empty())
		{
			std::shared_ptr<kml::Texture> tex(new kml::Texture());
			tex->SetFilePath(texName);
			mat->SetTexture("Normal", tex);
		}
	}

	// --- store glTF standard material ---
	mat->SetFloat("BaseColor.R", baseCol.r * baseWeight);
	mat->SetFloat("BaseColor.G", baseCol.g * baseWeight);
	mat->SetFloat("BaseColor.B", baseCol.b * baseWeight);
	mat->SetFloat("BaseColor.A", 1.0 - transmissionWeight);
	if (baseColorTex) {
		mat->SetTexture("BaseColor", baseColorTex);
	}
	mat->SetFloat("metallicFactor", metallic);
	mat->SetFloat("roughnessFactor", specularRoughness);

	mat->SetFloat("Emission.R", emissionCol.r * emissionWeight);
	mat->SetFloat("Emission.G", emissionCol.g * emissionWeight);
	mat->SetFloat("Emission.B", emissionCol.b * emissionWeight);
	if (emissionColorTex) {
		mat->SetTexture("Emission", emissionColorTex);
	}
	return true;
}

static
std::shared_ptr<kml::Material> ConvertMaterial(MObject& shaderObject)
{
	std::shared_ptr<kml::Material> mat = std::shared_ptr<kml::Material>(new kml::Material());

	mat->SetFloat("BaseColor.R", 1.0f);
	mat->SetFloat("BaseColor.G", 1.0f);
	mat->SetFloat("BaseColor.B", 1.0f);
	mat->SetFloat("BaseColor.A", 1.0f);

	//color
	{
		MPlug mp;
		MPlugArray mpa;
		MFnDependencyNode node(shaderObject);

		mp = node.findPlug("aiSurfaceShader"); // first check aiSurfaceShader
		mp.connectedTo(mpa, true, false);

		if (mpa.length() == 0) {
			mp = node.findPlug("surfaceShader");// next check, shader Group
			mp.connectedTo(mpa, true, false);
		}

		const int ksz = mpa.length();
		for (int k = 0; k < ksz; k++)
		{
			if (mpa[k].node().hasFn(MFn::kLambert)) { // All material(Lambert, phongE)
				MFnLambertShader shader(mpa[k].node());
				std::string shadername = shader.name().asChar();
				
				mat->SetName(shadername);
				mat->SetFloat("metallicFactor", 0.0f);
				mat->SetFloat("roughnessFactor", 1.0);
				MString normaltexpath;
				MColor col;
				std::shared_ptr<kml::Texture> coltex(nullptr);
				if (getTextureAndColor(shader, MString("color"), coltex, col))
				{
					if (coltex)
					{
						mat->SetTexture("BaseColor", coltex);
					}
					else
					{
						mat->SetFloat("BaseColor.R", col.r);
						mat->SetFloat("BaseColor.G", col.g);
						mat->SetFloat("BaseColor.B", col.b);
						mat->SetFloat("BaseColor.A", col.a);
					}
				}

				MColor tra = getColor(shader, "transparency");
				mat->SetFloat("BaseColor.A", 1.0f - tra.r);

				// Normal map
				float depth;
				if (getNormalAttrib(shader, normaltexpath, depth))
				{
					std::string texName = normaltexpath.asChar();
					if (!texName.empty())
					{
						std::shared_ptr<kml::Texture> tex(new kml::Texture());
						tex->SetFilePath(texName);
						mat->SetTexture("Normal", tex);
					}
				}
			}

			if (mpa[k].node().hasFn(MFn::kPhongExplorer)) { // PhongE only

				// default value
				float roughness = 0.0f;
				float metallic = 0.5f;

				MFnPhongEShader pshader(mpa[k].node());
				MStatus status;
				MPlug roughnessPlug = pshader.findPlug("roughness", &status);
				if (status == MS::kSuccess) {
					roughnessPlug.getValue(roughness);
				}

				mat->SetFloat("metallicFactor", metallic);
				mat->SetFloat("roughnessFactor", roughness);
			}

			if (isStingrayPBS(mpa[k].node())) {
				storeStingrayPBS(mat, mpa[k].node());
			}

			if (isAiStandardSurfaceShader(mpa[k].node())) {
				storeAiStandardSurfaceShader(mat, mpa[k].node());
			}
		}

	}

	return mat;
}

static
std::string GetCacheTexturePath(const std::string& path)
{
#ifdef _WIN32
	char szDrive[_MAX_DRIVE];
	char szDir[_MAX_DIR];
	char szFname[_MAX_FNAME];
	char szExt[_MAX_EXT];
	_splitpath(path.c_str(), szDrive, szDir, szFname, szExt);
	std::string strRet;
	strRet += szDrive;
	strRet += szDir;
	strRet += szFname;
	strRet += "_s0";
	if (std::string(szExt) != ".png")
	{
		strRet += ".jpg";
	}
	else
	{
		strRet += szExt;
	}
	return strRet;
#else 
	//std::string tmp = GetTempDirectory();
	std::string strRet;
	strRet += dirname(const_cast<char*>(path.c_str()));  // force const cast for error of : it would lose const qualifier
	strRet += basename(const_cast<char*>(path.c_str())); // force const cast for error of : it would lose const qualifier
	strRet += std::string("_s0.jpg");
	return strRet;
#endif
}

class TexturePathManager
{
public:
	typedef std::map<std::string, std::string> MapType;
public:
	bool HasOriginalPath(const std::string& path)const
	{
		MapType::const_iterator it = texMap_.find(path);
		if (it != texMap_.end())
		{
			return true;
		}
		return false;
	}
	std::string GetCopiedPath(const std::string& path)
	{
		MapType::const_iterator it = texMap_.find(path);
		if (it != texMap_.end())
		{
			return it->second;
		}
		else
		{
			std::string fname = GetFileName(path);
			if (fileNameSet_.find(fname) != fileNameSet_.end())
			{
				if (countMap_.find(fname) == countMap_.end())
				{
					countMap_[fname] = 2;
				}
				else
				{
					countMap_[fname]++;
				}
				int count = countMap_[fname];
				return GetDirectoryPath(path) + fname + std::string("_") + IToS(count) + GetExt(path);
			}
			else
			{
				return path;
			}
		}
	}
	void SetPathPair(const std::string& orgPath, const std::string& cpyPath)
	{
		texMap_[orgPath] = cpyPath;
		fileNameSet_.insert(GetFileName(cpyPath));
	}
	std::set<std::string> GetOriginalPaths()const
	{
		std::set<std::string> s;
		for (MapType::const_iterator it = texMap_.begin(); it != texMap_.end(); it++)
		{
			s.insert(it->first);
		}
		return s;
	}
	std::set<std::string> GetCopiedPaths()const
	{
		std::set<std::string> s;
		for (MapType::const_iterator it = texMap_.begin(); it != texMap_.end(); it++)
		{
			s.insert(it->second);
		}
		return s;
	}

	const MapType& GetPathPairs()const
	{
		return texMap_;
	}
protected:
	static
	std::string IToS(int n)
	{
		char buffer[16] = {};
#ifdef _WIN32
		_snprintf(buffer, 16, "%d", n);
#else
		snprintf(buffer, 16, "%d", n);
#endif
		return buffer;
	}
private:
	MapType texMap_;
	std::set<std::string> fileNameSet_;
	std::map<std::string, int> countMap_;
};

typedef std::map<int, std::shared_ptr<kml::Material> > ShaderMapType;

static
std::shared_ptr<kml::Node> GetLeafNode(const std::shared_ptr<kml::Node>& node)
{
	if (node->GetChildren().size() > 0)
	{
		return GetLeafNode(node->GetChildren()[0]);
	}
	else
	{
		return node;
	}
}

static
std::shared_ptr<kml::Node> GetParentNode(const std::shared_ptr<kml::Node>& root_node, const std::shared_ptr<kml::Node>& node)
{
	if (root_node->GetChildren().size() > 0)
	{
		for (size_t i = 0; i < root_node->GetChildren().size(); i++)
		{
			if (root_node->GetChildren()[i].get() == node.get())
			{
				return root_node;
			}
		}

		for (size_t i = 0; i < root_node->GetChildren().size(); i++)
		{
			auto n = GetParentNode(root_node->GetChildren()[i], node);
			if (n.get() != NULL)
			{
				return n;
			}
		}
	}
	return std::shared_ptr<kml::Node>();
}

static
glm::mat4 GetRootNodeGlobalMatrix(const std::shared_ptr<kml::Node>& node, const glm::mat4& mat = glm::mat4(1.0))
{
	glm::mat4 m = mat * node->GetTransform()->GetMatrix();
	if (node->GetChildren().size() > 0)
	{
		return GetRootNodeGlobalMatrix(node->GetChildren()[0], m);
	}
	return m;
}

static
MStatus WriteGLTF(
	TexturePathManager& texManager,
	std::map<int, std::shared_ptr<kml::Material> >& materials,
	std::vector< std::shared_ptr<kml::Node> >& nodes,
	const MString& dirname, const MDagPath& dagPath)
{
	MStatus status = MS::kSuccess;

	std::shared_ptr<kml::Node> root_node = CreateMeshNode(dagPath);
	std::shared_ptr<kml::Node> node = GetLeafNode(root_node);

	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	bool onefile = opts->GetInt("output_onefile") > 0;
	bool recalc_normals = opts->GetInt("recalc_normals") > 0;
	bool make_preload_texture = opts->GetInt("make_preload_texture") > 0;

	std::string mesh_name = node->GetMesh()->name;

	glm::mat4 global_matrix = GetRootNodeGlobalMatrix(root_node);

	{
		std::vector<MObject> shaders;
		std::vector<int> faceIndices;
		GetMaterials(shaders, faceIndices, dagPath);
		if (!shaders.empty())
		{
			for (size_t i = 0; i < shaders.size(); i++)
			{
				int shaderID = (int)MObjectHandle::objectHashCode(shaders[i]);
				ShaderMapType::iterator it = materials.find(shaderID);
				if (it != materials.end())
				{
					std::shared_ptr<kml::Material> mat = it->second;
					node->AddMaterial(mat);
				}
				else
				{
					std::shared_ptr<kml::Material> mat = ConvertMaterial(shaders[i]);
					{
						auto keys = mat->GetTextureKeys();
						for (size_t j = 0; j < keys.size(); j++)
						{
							std::string key = keys[j];
							std::shared_ptr<kml::Texture> tex = mat->GetTexture(key);

							std::shared_ptr<kml::Texture> dstTex(tex->clone());

							std::vector<std::string> texPath_vec;

							// UDIM texturing
							if (tex->GetUDIMMode()) {
								tex->ClearUDIM_ID();
								for (int udimID = 1001; udimID < 1100; udimID++) {
									std::string orgPath = tex->MakeUDIMFilePath(udimID);
									if (IsFileExist(orgPath)) { // find UDIM files
										dstTex->AddUDIM_ID(udimID);
										texPath_vec.push_back(orgPath);
									}
								}
								std::string udimpath = tex->GetUDIMFilePath();
								std::string copiedUDIMPath = MakeConvertTexturePath(texManager.GetCopiedPath(udimpath));
								std::string dstUDIMPath = (onefile) ? "./" : "../";
								dstUDIMPath += GetFileExtName(copiedUDIMPath);
								dstTex->SetUDIMFilePath(dstUDIMPath);
							}

							// ordinary texturing
							std::string orgPath = tex->GetFilePath();
							texPath_vec.push_back(orgPath);
							std::string copiedPath = MakeConvertTexturePath(texManager.GetCopiedPath(orgPath));
							std::string dstPath = (onefile) ? "./" : "../";
							dstPath += GetFileExtName(copiedPath);
							dstTex->SetFilePath(dstPath);
							mat->SetTexture(key, dstTex);


							// if need convert, change image extension.
							for (size_t t = 0; t < texPath_vec.size(); ++t) {
								const std::string tmpOrgPath = texPath_vec[t];
								std::string tempCopiedPath = MakeConvertTexturePath(texManager.GetCopiedPath(tmpOrgPath));
								if (!texManager.HasOriginalPath(tmpOrgPath))
								{
									tempCopiedPath = std::string(dirname.asChar()) + "/" + GetFileExtName(tempCopiedPath);
									texManager.SetPathPair(tmpOrgPath, tempCopiedPath); // register for texture copy
								}
							}

							
							if (make_preload_texture)
							{
								std::string cachePath = GetCacheTexturePath(dstPath);
								std::shared_ptr<kml::Texture> cacheTex(dstTex->clone());
								cacheTex->SetFilePath(cachePath);
								mat->SetTexture(key + "S0", cacheTex);
							}
							
						}
					}

					mat->SetInteger("ID", shaderID);
					materials[shaderID] = mat;

					node->AddMaterial(mat);
				}
			}
			if (faceIndices.size() != node->GetMesh()->facenums.size())
			{
				faceIndices.resize(node->GetMesh()->facenums.size());
			}
			{
				bool exist_nomaterial = false;
				for (size_t i = 0; i < faceIndices.size(); i++)
				{
					int matID = faceIndices[i];
					if (matID < 0 || node->GetMaterials().size() <= matID)
					{
						exist_nomaterial = true;
						break;
					}
				}
				if (exist_nomaterial)
				{
					int nMat = (int)node->GetMaterials().size();
					for (size_t i = 0; i < faceIndices.size(); i++)
					{
						int matID = faceIndices[i];
						if (matID < 0 || node->GetMaterials().size() <= matID)
						{
							faceIndices[i] = nMat;
						}
					}

					int shaderID = 0;
					ShaderMapType::iterator it = materials.find(shaderID);
					if (it != materials.end())
					{
						std::shared_ptr<kml::Material> mat = it->second;
						node->AddMaterial(mat);
					}
					else
					{
						std::shared_ptr<kml::Material> mat = std::shared_ptr<kml::Material>(new kml::Material());
						mat->SetInteger("ID", shaderID);
						materials[shaderID] = mat;

						node->AddMaterial(mat);
					}
				}
			}
			node->GetMesh()->materials = faceIndices;
		}
		else
		{
			int shaderID = 0;
			ShaderMapType::iterator it = materials.find(shaderID);
			if (it != materials.end())
			{
				std::shared_ptr<kml::Material> mat = it->second;
				node->AddMaterial(mat);
			}
			else
			{
				std::shared_ptr<kml::Material> mat = std::shared_ptr<kml::Material>(new kml::Material());
				mat->SetInteger("ID", shaderID);
				materials[shaderID] = mat;

				node->AddMaterial(mat);
			}
		}
	}

	if (!kml::TriangulateMesh(node->GetMesh()))
	{
		return MS::kFailure;
	}
	if (!kml::RemoveNoAreaMesh(node->GetMesh()))
	{
		return MS::kFailure;
	}
	if (recalc_normals)
	{
		node->GetMesh()->normals.clear();
	}
	if (!kml::CalculateNormalsMesh(node->GetMesh()))
	{
		return MS::kFailure;
	}
	if (!kml::FlatIndicesMesh(node->GetMesh()))
	{
		return MS::kFailure;
	}

	std::vector< std::shared_ptr<kml::Node> > tnodes;
	{
		tnodes = kml::SplitNodeByMaterialID(node);
	}

	{
		std::vector< std::shared_ptr<kml::Node> > tnodes2;
		for (int i = 0; i < tnodes.size(); i++)
		{
			if (tnodes[i]->GetMesh()->facenums.empty())
			{
				continue;
			}
			if (!kml::FlatIndicesMesh(tnodes[i]->GetMesh()))
			{
				return MS::kFailure;
			}

			tnodes[i]->SetBound(kml::CalculateBound(tnodes[i]->GetMesh(), global_matrix));

			tnodes2.push_back(tnodes[i]);
		}

		tnodes.swap(tnodes2);
	}

	{
		for (int i = 0; i < tnodes.size(); i++)
		{
			tnodes[i]->GetMesh()->name = mesh_name;
		}
	}

	if (root_node == node)
	{
		for (int i = 0; i < tnodes.size(); i++)
		{
			if (!onefile)
			{
				char buffer[32] = {};
				sprintf(buffer, "%d", i + 1);
				std::string number = buffer;

				std::string base_path = dirname.asChar();
				std::string node_path = MakeDirectoryPath(dagPath.fullPathName().asChar()) + std::string("_") + number;
				std::string dir_path = base_path + "/" + node_path;
				std::string gltf_path = MakeDirectoryPath(std::string(dagPath.partialPathName().asChar())) + ".gltf";

				std::string gltfFullPath = dir_path + "/" + gltf_path;
				std::string gltfAbsPath = node_path + "/" + gltf_path;
				node->SetPath(gltfAbsPath);

				MakeDirectory(dir_path);
				//cerr << "Wrinting Internal Files into " << gltfFullPath << std::endl;

				kml::glTFExporter exporter;
				if (!exporter.Export(gltfFullPath, tnodes[i], kml::Options::GetGlobalOptions()))
				{
					return MS::kFailure;
				}
				tnodes[i]->SetMesh(std::shared_ptr<kml::Mesh>());//Clear Mesh
			}
			nodes.push_back(tnodes[i]);
		}
	}
	else
	{
		auto parent = GetParentNode(root_node, node);
		if (parent.get())
		{
			parent->ClearChildren();
		}
		for (int i = 0; i < tnodes.size(); i++)
		{
			parent->AddChild(tnodes[i]);
		}
		nodes.push_back(root_node);
	}


	return status;
}


static
void NodeToJson(picojson::object& item, const std::shared_ptr<kml::Node>& node)
{
	picojson::array maxarray, minarray, childarray;
	for (int i = 0; i < node->GetChildren().size(); ++i)
	{
		picojson::object citem;
		NodeToJson(citem, node->GetChildren()[i]);
		childarray.push_back(picojson::value(citem));
	}
	std::shared_ptr<kml::Bound> bound = node->GetBound();
	glm::vec3 min = bound->GetMin();
	glm::vec3 max = bound->GetMax();

	minarray.push_back(picojson::value(min[0]));
	minarray.push_back(picojson::value(min[1]));
	minarray.push_back(picojson::value(min[2]));
	maxarray.push_back(picojson::value(max[0]));
	maxarray.push_back(picojson::value(max[1]));
	maxarray.push_back(picojson::value(max[2]));

	item["name"] = picojson::value(node->GetName());
	item["path"] = picojson::value(node->GetPath());
	item["min"] = picojson::value(minarray);
	item["max"] = picojson::value(maxarray);
	
	if (childarray.size() > 0) 
	{
		double childradius = (double)glm::length(max - min); // hack radius
		item["childradius"] = picojson::value(childradius);
		item["children"] = picojson::value(childarray);
	}
}

static
void NodesToJson(picojson::object& root, const std::vector< std::shared_ptr<kml::Node> >& nodes)
{
	root["scene"] = picojson::value((double)0);
	picojson::array scenes;
	for(size_t i = 0; i < nodes.size(); i++)
	{
		picojson::object item;
		NodeToJson(item, nodes[i]);
		scenes.push_back(picojson::value(item));
	}
	
	root["scenes"] = picojson::value(scenes);
}

static
std::string GetFullPath(const std::string& path)
{
#ifdef _WIN32
	char buffer[_MAX_PATH + 2] = {};
	GetFullPathNameA(path.c_str(), _MAX_PATH, buffer, NULL);
	return std::string(buffer);
#else
	return path;
#endif
}

static
void CopyTextureFiles(const TexturePathManager& texManager)
{
	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	bool make_preload_texture = opts->GetInt("make_preload_texture") > 0;
	const auto& texMap = texManager.GetPathPairs();
	for (std::map<std::string, std::string>::const_iterator it = texMap.begin(); it != texMap.end(); it++)
	{
		std::string orgPath = GetFullPath(it->first);
		std::string dstPath = GetFullPath(it->second);
		if (orgPath != dstPath)
		{
			kil::CopyTextureFile(orgPath, dstPath);
			if (make_preload_texture)
			{
				std::string cachePath = GetCacheTexturePath(dstPath);
				kil::ResizeTextureFile(orgPath, cachePath, 256, 128);
			}
		}
	}
}


static
void GetMeshNodes(std::vector< std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
{
	if (node->GetChildren().size() > 0)
	{
		for (size_t i = 0; i < node->GetChildren().size(); i++)
		{
			GetMeshNodes(nodes, node->GetChildren()[i]);
		}
	}
	{
		auto mesh = node->GetMesh();
		if (mesh.get())
		{
			nodes.push_back(node);
		}
	}
}

static
void GetAllNodes(std::vector< std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
{
	if (node->GetChildren().size() > 0)
	{
		for (size_t i = 0; i < node->GetChildren().size(); i++)
		{
			GetAllNodes(nodes, node->GetChildren()[i]);
		}
	}
	{
		nodes.push_back(node);
	}
}

static
std::shared_ptr<kml::Bound> ExpandBound(std::shared_ptr<kml::Node>& node)
{
	if (!node->GetBound().get())
	{
		std::vector< std::shared_ptr<kml::Bound> > bounds;
		for (size_t i = 0; i < node->GetChildren().size(); i++)
		{
			auto n = node->GetChildren()[i];
			bounds.push_back( ExpandBound( n ) );
		}

		static float MIN_ = -std::numeric_limits<float>::max();
		static float MAX_ = +std::numeric_limits<float>::max();
		glm::vec3 m0(MAX_, MAX_, MAX_);
		glm::vec3 m1(MIN_, MIN_, MIN_);
		for (size_t i = 0; i < bounds.size(); i++)
		{
			glm::vec3 c0 = bounds[i]->GetMin();
			glm::vec3 c1 = bounds[i]->GetMax();
			for (int j = 0; j < 3; j++)
			{
				m0[j] = std::min(c0[j], m0[j]);
				m1[j] = std::max(c1[j], m1[j]);
			}
		}
		node->SetBound(std::shared_ptr<kml::Bound>(new kml::Bound(m0, m1)));
	}
	return node->GetBound();
}

static
void AddChildUnique(std::shared_ptr<kml::Node>& parent, std::shared_ptr<kml::Node>& child)
{
	auto children = parent->GetChildren();
	bool hasChild = false;
	for (size_t i = 0; i < children.size(); i++)
	{
		if (children[i].get() == child.get())
		{
			hasChild = true;
			break;
		}
	}
	if (!hasChild)
	{
		parent->AddChild(child);
	}
}


static
void SetNode(std::map<std::string, std::shared_ptr<kml::Node> >& pathMap, const std::string& path, const std::shared_ptr<kml::Node>& node)
{
    typedef std::map<std::string, std::shared_ptr<kml::Node> > PathMap;
    typedef PathMap::iterator iterator;
    //pathMap[path] = node;
    iterator it = pathMap.find(path);
    if (it == pathMap.end())
    {
        pathMap[path] = node;
    }
    else
    {
        const std::shared_ptr<kml::Node>& a = it->second;
        const std::shared_ptr<kml::Node>& b = node;

        std::shared_ptr<kml::Mesh> amesh = a->GetMesh();
        std::shared_ptr<kml::Mesh> bmesh = b->GetMesh();

        if (!amesh.get() && bmesh.get())
        {
            pathMap[path] = node;
        }
    }
}

static
std::shared_ptr<kml::Node> CombineNodes(const std::vector< std::shared_ptr<kml::Node> >& nodes)
{
	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	int transform_space = opts->GetInt("transform_space");

	std::shared_ptr<kml::Node> node(new kml::Node());
	node->GetTransform()->SetMatrix(glm::mat4(1.0f));

	{
		//Local space
		typedef std::map<std::string, std::shared_ptr<kml::Node> > PathMap;
		std::vector<PathMap> pathMapList;
		std::vector< std::vector<std::string> > pathList;
		for (size_t i = 0; i < nodes.size(); i++)
		{
			auto& node = nodes[i];
			std::string path = node->GetPath();
			std::vector<std::string> pathvec = SplitPath(path, "|");
			if (pathvec.front() == "")
			{
				std::vector<std::string> tvec(std::next(pathvec.begin()), pathvec.end());
				pathvec.swap(tvec);
			}

			pathList.push_back(pathvec);
		}

		for (size_t i = 0; i < pathList.size(); i++)
		{
			const auto& pathvec = pathList[i];
			int sz = pathvec.size();

			for (size_t j = 0; j < pathvec.size(); j++)
			{
				if (pathMapList.size() <= j)
				{
					pathMapList.push_back(PathMap());
				}
			}

            SetNode(pathMapList[sz - 1], pathvec[sz - 1], nodes[i]);
		}

		for (size_t i = 0; i < pathList.size(); i++)
		{
			const auto& pathvec = pathList[i];
			for (size_t j = 0; j < pathvec.size(); j++)
			{
				auto iter = pathMapList[j].find(pathvec[j]);
				if (iter == pathMapList[j].end())
				{
					pathMapList[j][pathvec[j]] = std::shared_ptr<kml::Node>(new kml::Node());
				}
			}
		}

		for (size_t i = 0; i < pathList.size(); i++)
		{
			const auto& pathvec = pathList[i];
			int sz = pathvec.size();
			for (int j = 0; j < sz; j++)
			{
				pathMapList[j][pathvec[j]]->ClearChildren();
			}
		}

		for (size_t i = 0; i < pathList.size(); i++)
		{
			const auto& pathvec = pathList[i];
			int sz = pathvec.size();
			for (int j = 1; j < sz; j++)
			{
				AddChildUnique(pathMapList[j - 1][pathvec[j - 1]], pathMapList[j][pathvec[j]]);
			}
		}

		{
			PathMap& m = pathMapList[0];
			for (PathMap::iterator it = m.begin(); it != m.end(); it++)
			{
				node->AddChild(it->second);
			}
		}
		ExpandBound(node);
	}
	

	return node;
}


static
std::string GetExportDirectory(const std::string& fname)
{
	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	bool glb = opts->GetInt("output_glb") > 0;
	if (glb)
	{
		return GetTempDirectory();
	}
	else
	{
		return RemoveExt(fname);
	}
}

MStatus glTFExporter::exportSelected(const MString& fname)
{
	MStatus status = MS::kSuccess;

	// Create an iterator for the active selection list
	//
	MSelectionList slist;
	MGlobal::getActiveSelectionList( slist );
	MItSelectionList iter( slist );

	if (iter.isDone()) 
	{
	    fprintf(stderr,"Error: Nothing is selected.\n");
	    return MS::kFailure;
	}

    // We will need to interate over a selected node's heirarchy 
    // in the case where shapes are grouped, and the group is selected.
    MItDag dagIterator( MItDag::kDepthFirst, MFn::kInvalid, &status);

	if (MS::kSuccess != status)
	{
		fprintf(stderr, "Failure in DAG iterator setup.\n");
		return MS::kFailure;
	}

	std::vector< MDagPath > dagPaths;

	// Selection list loop
	for (; !iter.isDone(); iter.next())
	{
		MDagPath objectPath;
		// get the selected node
		status = iter.getDagPath(objectPath);

		// reset iterator's root node to be the selected node.
		status = dagIterator.reset(objectPath.node(),
			MItDag::kDepthFirst, MFn::kInvalid);


		// DAG iteration beginning at at selected node
		for (; !dagIterator.isDone(); dagIterator.next())
		{
			MDagPath dagPath;
			status = dagIterator.getPath(dagPath);

			if (!status) 
			{
				fprintf(stderr, "Failure getting DAG path.\n");
				return MS::kFailure;
			}

			if (status)
			{
				// skip over intermediate objects
				//
				MFnDagNode dagNode(dagPath, &status);
				if (dagNode.isIntermediateObject())
				{
					continue;
				}

				if ((dagPath.hasFn(MFn::kNurbsSurface)) &&
					(dagPath.hasFn(MFn::kTransform)))
				{
					continue;
				}
				else if ((dagPath.hasFn(MFn::kMesh)) &&
					     (dagPath.hasFn(MFn::kTransform)))
				{
					continue;
				}
				else if (dagPath.hasFn(MFn::kMesh))
				{
					dagPaths.push_back(dagPath);
				}
			}
		}
	}

	return exportProcess(fname, dagPaths);
}

MStatus glTFExporter::exportAll     (const MString& fname)
{
	MStatus status = MS::kSuccess;

	MItDag dagIterator( MItDag::kBreadthFirst, MFn::kInvalid, &status);

	if ( MS::kSuccess != status) 
	{
	    fprintf(stderr,"Failure in DAG iterator setup.\n");
	    return MS::kFailure;
	}

	std::vector< MDagPath > dagPaths;

	for ( ; !dagIterator.isDone(); dagIterator.next() )
	{
		MDagPath dagPath;
		MObject  component = MObject::kNullObj;
		status = dagIterator.getPath(dagPath);

		if (!status) {
			fprintf(stderr,"Failure getting DAG path.\n");
			return MS::kFailure;
		}

		// skip over intermediate objects
		//
		MFnDagNode dagNode( dagPath, &status );
		if (dagNode.isIntermediateObject()) 
		{
			continue;
		}

		if ((dagPath.hasFn(MFn::kNurbsSurface)) &&
			(dagPath.hasFn(MFn::kTransform)))
		{
			continue;
		}
		else if ((dagPath.hasFn(MFn::kMesh)) &&
				 (dagPath.hasFn(MFn::kTransform)))
		{
			continue;
		}
		else if (dagPath.hasFn(MFn::kMesh))
		{
			dagPaths.push_back(dagPath);
		}
	}

	return exportProcess(fname, dagPaths);
}

static
std::vector< std::shared_ptr < kml::Node > > GetJointNodes(const std::vector< std::shared_ptr < kml::Node > >& lnodes)
{
    std::vector< std::shared_ptr < kml::Node > > mesh_nodes;
    for (size_t i = 0; i < lnodes.size(); i++)
    {
        GetMeshNodes(mesh_nodes, lnodes[i]);
    }
	//create nodes from skin weights' name
	std::vector<std::string> joint_paths;
	for (size_t i = 0; i < mesh_nodes.size(); i++)
	{
		auto mesh = mesh_nodes[i]->GetMesh();
		if (mesh.get())
		{
			if (mesh->skin_weights.get())
			{
				auto& skin_weights = mesh->skin_weights;
				for (size_t j = 0; j < skin_weights->joint_paths.size(); j++)
				{
                    joint_paths.push_back(skin_weights->joint_paths[j]);
				}
			}
		}
	}

	std::sort(joint_paths.begin(), joint_paths.end());
    joint_paths.erase(std::unique(joint_paths.begin(), joint_paths.end()), joint_paths.end());

	std::vector< std::shared_ptr < kml::Node > > tnodes;
	for (size_t j = 0; j < joint_paths.size(); j++)
	{
		std::vector<MDagPath> dagPathList = GetDagPathList(joint_paths[j]);
		std::vector< std::shared_ptr<kml::Node> > nodes;
		for (size_t i = 0; i < dagPathList.size(); i++)
		{
			MDagPath path = dagPathList[i];
			std::shared_ptr < kml::Node > n(new kml::Node());
			n->SetName(path.partialPathName().asChar());
			n->SetPath(path.fullPathName().asChar());

            MFnTransform fnTransform(path);
			
#if 1
            MVector mT = fnTransform.getTranslation(MSpace::kTransform);
            MQuaternion mR, mOR, mJO;
			fnTransform.getRotation(mR, MSpace::kTransform);
			MStatus ret;
			mOR = fnTransform.rotateOrientation(MSpace::kTransform, &ret); // get Rotation Axis
			if (ret == MS::kSuccess) {
				mR = mOR * mR;
			}

			MFnIkJoint fnJoint(path);
			MStatus joret = fnJoint.getOrientation(mJO); // get jointOrient
			if (joret == MS::kSuccess) {
				mR *= mJO;
			}
            
			double mS[3];
            fnTransform.getScale(mS);

            glm::vec3 vT(mT.x, mT.y, mT.z);
            glm::quat vR(mR.w, mR.x, mR.y, mR.z);//wxyz
            glm::vec3 vS(mS[0], mS[1], mS[2]);
            n->GetTransform()->SetTRS(vT, vR, vS);
#else
            MMatrix mmat = fnTransform.transformationMatrix();//local
            double dest[4][4];
            mmat.get(dest);
            glm::mat4 mat(
                dest[0][0], dest[0][1], dest[0][2], dest[0][3],
                dest[1][0], dest[1][1], dest[1][2], dest[1][3],
                dest[2][0], dest[2][1], dest[2][2], dest[2][3],
                dest[3][0], dest[3][1], dest[3][2], dest[3][3]
            );
            n->GetTransform()->SetMatrix(mat);
#endif
            nodes.push_back(n);
		}

		for (size_t i = 0; i < nodes.size() - 1; i++)
		{
			nodes[i]->AddChild(nodes[i + 1]);
		}

		for (size_t i = 0; i < nodes.size(); i++)
		{
			tnodes.push_back(nodes[i]);
		}
	}
	return tnodes;
}

MStatus glTFExporter::exportProcess(const MString& fname, const std::vector< MDagPath >& dagPaths)
{
	MStatus status = MS::kSuccess;

	std::string dir_path = GetExportDirectory(fname.asChar());

	MakeDirectory(dir_path);

	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	bool onefile = opts->GetInt("output_onefile") > 0;
	bool glb = opts->GetInt("output_glb") > 0;
	bool vrm = opts->GetInt("vrm_export") > 0;
	

	typedef std::vector< std::shared_ptr<kml::Node> > NodeVecType;
	TexturePathManager texManager;
	ShaderMapType materials;
	NodeVecType   nodes;
	std::shared_ptr<ProgressWindow> progWindow;
	{
		int prog = 100;
		if (dagPaths.size())
		{
			int max_size = prog * dagPaths.size();
			progWindow.reset(new ProgressWindow(max_size) );
			for (int i = 0; i < (int)dagPaths.size(); i++)
			{
				if (progWindow->isCancelled())
				{
					return MS::kFailure;
				}

				MDagPath dagPath = dagPaths[i];
				if (dagPath.hasFn(MFn::kMesh))
				{
					status = WriteGLTF(texManager, materials, nodes, MString(dir_path.c_str()), dagPath);
				}
				progWindow->setProgress(prog * (i + 1));
			}
		}
	}

	if (nodes.empty())
	{
		return MStatus::kSuccess;
	}

	{
		std::vector< std::shared_ptr < kml::Node > > joint_nodes = GetJointNodes(nodes);
		for (size_t i = 0; i < joint_nodes.size(); i++)
		{
			nodes.push_back(joint_nodes[i]);
		}
	}

	//texture copy
	{
		CopyTextureFiles(texManager);
	}
	//write files when "output_onefile"
	if (onefile)
	{
		{
			int index = 0;
			for (ShaderMapType::iterator it = materials.begin(); it != materials.end(); it++)
			{
				auto& mat = it->second;
				mat->SetInteger("_Index", index);//
				index++;
			}
		}
		{
			NodeVecType mesh_nodes;
			for (NodeVecType::iterator it = nodes.begin(); it != nodes.end(); it++)
			{
				auto& node = *it;
				GetMeshNodes(mesh_nodes, node);
			}
			for (NodeVecType::iterator it = mesh_nodes.begin(); it != mesh_nodes.end(); it++)
			{
				auto& node = *it;
				auto& mesh = node->GetMesh();
				auto&  mat = node->GetMaterials()[0];
				int index = mat->GetInteger("_Index");
				for (int j = 0; j < mesh->materials.size(); j++)
				{
					mesh->materials[j] = index;
				}
			}
		}

		NodeVecType all_nodes;
		for (NodeVecType::iterator it = nodes.begin(); it != nodes.end(); it++)
		{
			auto& node = *it;
			GetAllNodes(all_nodes, node);
		}

		std::shared_ptr<kml::Node> node = CombineNodes(all_nodes);
		node->SetName(GetFileName(std::string(fname.asChar())));
		for (ShaderMapType::iterator it = materials.begin(); it != materials.end(); it++)
		{
			auto& mat = it->second;
			node->AddMaterial(mat);
		}
		{
			std::string base_path = dir_path;
			std::string gltf_path = dir_path + "/" + GetFileName(std::string(fname.asChar())) + ".gltf";

			node->SetPath(gltf_path);

			kml::glTFExporter exporter;
			if (!exporter.Export(gltf_path, node, kml::Options::GetGlobalOptions()))
			{
				return MS::kFailure;
			}
		}
	}
	else
	{
		std::string json_path = dir_path + "/" + GetFileName(fname.asChar()) + ".json";
		picojson::object root;
		NodesToJson(root, nodes);
		std::ofstream ofs(json_path);
		picojson::value(root).serialize(std::ostream_iterator<char>(ofs), true);
	}

	if (glb || vrm)
	{
		std::string gltf_path = dir_path + "/" + GetFileName(std::string(fname.asChar())) + ".gltf";
		
		std::string extname = ".glb";
		if (vrm) {
			extname = ".vrm";
		}
		std::string glb_path  = RemoveExt(std::string(fname.asChar())) + extname;

		if (!kml::GLTF2GLB(gltf_path, glb_path))
		{
			status = MStatus::kFailure;
		}
		RemoveDirectory_(dir_path);
	}

	return status;
}

