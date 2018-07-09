#ifdef _MSC_VER
#pragma warning(disable:4819)
#endif

#ifdef _MSC_VER
#pragma comment( lib, "OpenMayaUI" ) 
#pragma comment( lib, "Shell32.lib" )
#endif

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#define _CRT_SECURE_NO_WARNINGS
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



#include <memory>
#include <sstream>
#include <fstream>
#include <set>
#include <filesystem>

#include <string.h> 
#include <sys/types.h>

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
extern "C" int strcasecmp (const char *, const char *);
extern "C" Boolean createMacFile (const char *fileName, FSRef *fsRef, long creator, long type);
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
#else
#error "Not implemented"
	return "";
#endif
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
#if defined (OSMac_)

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
	else
	{
		return kNotMyFileType;
	}
}


MStatus glTFExporter::writer ( const MFileObject& file, const MString& options, FileAccessMode mode )

{
    MStatus status;
	MString mname = file.fullName(), unitName;
    
#if defined (OSMac_)
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
	int make_preload_texture = 0;	//0:off, 1:on
	int output_buffer = 1;			//0:bin, 1:draco, 2:bin/draco
	int convert_texture_format = 0; //0:no convert, 1:jpeg, 2:png

	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	opts->SetInt("recalc_normals", recalc_normals);
	opts->SetInt("output_onefile", output_onefile);
	opts->SetInt("output_glb", output_glb);
	opts->SetInt("make_preload_texture", make_preload_texture);
	opts->SetInt("output_buffer", output_buffer);
	opts->SetInt("convert_texture_format", convert_texture_format);
	
    
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
			if (theOption[0] == MString("make_preload_texture") && theOption.length() > 1) {
				make_preload_texture = theOption[1].asInt();
			}
			if (theOption[0] == MString("output_buffer") && theOption.length() > 1) {
				output_buffer = theOption[1].asInt();
			}
			if (theOption[0] == MString("convert_texture_format") && theOption.length() > 1) {
				convert_texture_format = theOption[1].asInt();
			}
		}
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
std::shared_ptr<kml::Node> OutputPolygons( 
        MDagPath& mdagPath,
        MObject&  mComponent
)
{
	MStatus status = MS::kSuccess;
	
	MSpace::Space space = MSpace::kWorld;
	//int i;

	MFnMesh fnMesh( mdagPath, &status );
	if ( MS::kSuccess != status) {
		fprintf(stderr,"Failure in MFnMesh initialization.\n");
		return std::shared_ptr<kml::Node>();
	}

	MItMeshPolygon polyIter( mdagPath, mComponent, &status );
	if ( MS::kSuccess != status) {
		fprintf(stderr,"Failure in MItMeshPolygon initialization.\n");
		return std::shared_ptr<kml::Node>();
	}
	MItMeshVertex vtxIter( mdagPath, mComponent, &status );
	if ( MS::kSuccess != status) {
		fprintf(stderr,"Failure in MItMeshVertex initialization.\n");
		return std::shared_ptr<kml::Node>();
	}

	/*
	int objectIdx = -1, length;
	MString mdagPathNodeName = fnMesh.name();
	// Find i such that objectGroupsTablePtr[i] corresponds to the
	// object node pointed to by mdagPath
	length = objectNodeNamesArray.length();
	for( i=0; i<length; i++ ) {
		if( objectNodeNamesArray[i] == mdagPathNodeName ) {
			objectIdx = i;
			break;
		}
	}
	*/

    // Write out the vertex table
    //
	std::vector<glm::vec3> positions;
	for ( ; !vtxIter.isDone(); vtxIter.next() ) 
	{
		MPoint p = vtxIter.position( space );
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
	fnMesh.getUVs( uArray, vArray );
    int uvLength = uArray.length();
	for (int x = 0; x < uvLength; x++ )
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
	    MStatus stat = fnMesh.getNormals( norms, space );
		if(stat == MStatus::kSuccess)
		{
			int normsLength = norms.length();
			for ( int t=0; t<normsLength; t++ ) 
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
	for ( ; !polyIter.isDone(); polyIter.next() )
	{
        // Write out the smoothing group that this polygon belongs to
        // We only write out the smoothing group if it is different
        // from the last polygon.
        //
		/*
        if ( smoothing ) {
           	int compIdx = polyIter.index();
            int smoothingGroup = polySmoothingGroups[ compIdx ];
            
            if ( lastSmoothingGroup != smoothingGroup ) {
                if ( NO_SMOOTHING_GROUP == smoothingGroup ) {
                    fprintf(fp,"s off\n");
                }
                else {
                    fprintf(fp,"s %d\n", smoothingGroup );
                }
                lastSmoothingGroup = smoothingGroup;
            }
        }
		*/
		/*
        // Write out all the sets that this polygon belongs to
        //
		if ((groups || materials) && (objectIdx >= 0)) {
			int compIdx = polyIter.index();
			outputSetsAndGroups( mdagPath, compIdx, false, objectIdx );
		}
		*/
                
        // Write out vertex/uv/normal index information
        //
        int polyVertexCount = polyIter.polygonVertexCount();
		facenums.push_back(polyVertexCount);
		for ( int vtx=0; vtx < polyVertexCount; vtx++ ) 
		{
			int pidx = polyIter.vertexIndex( vtx );
			int tidx = -1;
			int nidx = -1;
			if ( fnMesh.numUVs() > 0 ) 
			{
    			int uvIndex;
    			if ( polyIter.getUVIndex(vtx, uvIndex) )
				{
					tidx = uvIndex;
                }
			}
            
			if ( fnMesh.numNormals() > 0 ) 
			{
				nidx = polyIter.normalIndex( vtx );
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

	std::shared_ptr < kml::Node > node(new kml::Node());
	node->SetName(mdagPath.fullPathName().asChar());
	node->GetTransform()->SetMatrix(glm::mat4(1.0f));
	node->SetMesh(mesh);
	node->SetBound(kml::CalculateBound(mesh));

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
bool getTextureAndColor(MFnDependencyNode& node, MString& name, MString& texpath, MColor& color)
{
	color = MColor(1.0, 1.0, 1.0, 1.0);
	texpath = "";
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
				MString tpath;
				texturePlug.getValue(tpath);
				texpath = tpath.asChar();

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
/*
static
bool getColorAttrib(MFnDependencyNode& node, MString& texpath, MColor& color)
{
	color = MColor(1.0, 1.0, 1.0, 1.0);
	texpath = "";
	MStatus status;
	MPlug paramPlug = node.findPlug("color", &status);
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
				MString tpath;
				texturePlug.getValue(tpath);
				//MStringArray paths;
				//tpath.split('/', paths);
				//texpath = paths[paths.length() - 1];
				texpath = tpath.asChar();

				// if material has texture, set color(1,1,1)
				color.r = 1.0f;
				color.g = 1.0f;
				color.b = 1.0f;
				return true;
			}
			return false;
		} else {
			// other type node is not supported
			return false;
		}
	}
	paramPlug.child(0).getValue(color.r);
	paramPlug.child(1).getValue(color.g);
	paramPlug.child(2).getValue(color.b);
	return true;
}*/

static bool getNormalAttrib(MFnDependencyNode& node, MString& normaltexpath, float& depth)
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

static bool isStingrayPBS(MFnDependencyNode materialDependencyNode)
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
static bool storeStingrayPBS(std::shared_ptr<kml::Material> mat, MFnDependencyNode materialDependencyNode)
{
	// TODO
	return false;
}

static bool isAiStandardSurfaceShader(MFnDependencyNode materialDependencyNode)
{
	return materialDependencyNode.hasAttribute("baseColor") &&
		materialDependencyNode.hasAttribute("metalness") &&
		materialDependencyNode.hasAttribute("normalCamera") &&
		materialDependencyNode.hasAttribute("specularRoughness") &&
		materialDependencyNode.hasAttribute("emissionColor");
}

static bool storeAiStandardSurfaceShader(std::shared_ptr<kml::Material> mat, MFnDependencyNode ainode)
{
	// base
	const float baseWeight = ainode.findPlug("base").asFloat();
	const float baseColorR = ainode.findPlug("baseColorR").asFloat();
	const float baseColorG = ainode.findPlug("baseColorG").asFloat();
	const float baseColorB = ainode.findPlug("baseColorB").asFloat();
	const float diffuseRoughness = ainode.findPlug("diffuseRoughness").asFloat();
	const float metallic = ainode.findPlug("metalness").asFloat();
	MString baseColorTex;
	MColor baseCol;
	if (getTextureAndColor(ainode, MString("baseColor"), baseColorTex, baseCol)) {
		const std::string texName = baseColorTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_baseColor", texName);
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
	MString specularTex;
	MColor specularCol;
	if (getTextureAndColor(ainode, MString("specularColor"), specularTex, specularCol)) {
		const std::string texName = specularTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_specularColor", texName);
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
	const float transmission = ainode.findPlug("transmission").asFloat();
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
	MString transmissionTex;
	MColor transmissionCol;
	if (getTextureAndColor(ainode, MString("transmissionColor"), transmissionTex, transmissionCol)) {
		const std::string texName = transmissionTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_specularColor", texName);
		}
	}
	MString transmissionScatterTex;
	MColor transmissionScatterCol;
	if (getTextureAndColor(ainode, MString("transmissionScatter"), transmissionScatterTex, transmissionScatterCol)) {
		const std::string texName = transmissionScatterTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_transmissionScatter", texName);
		}
	}
	mat->SetFloat("ai_transmission", transmission);
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
	MString subsurfaceTex;
	MColor subsurfaceCol;
	if (getTextureAndColor(ainode, MString("subsurfaceColor"), subsurfaceTex, subsurfaceCol)) {
		const std::string texName = subsurfaceTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_subsurfaceColor", texName);
		}
	}
	MString subsurfaceRadiusTex;
	MColor subsurfaceRadiusCol;
	if (getTextureAndColor(ainode, MString("subsurfaceRadius"), subsurfaceRadiusTex, subsurfaceRadiusCol)) {
		const std::string texName = subsurfaceRadiusTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_subsurfaceRadius", texName);
		}
	}
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
	const float coatWeight = ainode.findPlug("coatWeight").asFloat();
	const float coatColorR = ainode.findPlug("coatColorR").asFloat();
	const float coatColorG = ainode.findPlug("coatColorG").asFloat();
	const float coatColorB = ainode.findPlug("coatColorB").asFloat();
	const float coatRoughness = ainode.findPlug("coatRoughness").asFloat();
	const float coatIOR = ainode.findPlug("coatIOR").asFloat();
	const float coatNormalX = ainode.findPlug("coatNormalX").asFloat();
	const float coatNormalY = ainode.findPlug("coatNormalY").asFloat();
	const float coatNormalZ = ainode.findPlug("coatNormalZ").asFloat();
	MString coatColorTex;
	MColor coatCol;
	if (getTextureAndColor(ainode, MString("coatColor"), coatColorTex, coatCol)) {
		const std::string texName = coatColorTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_coatColor", texName);
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
	MString emissionColorTex;
	MColor emissionCol;
	if (getTextureAndColor(ainode, MString("emissionColor"), emissionColorTex, emissionCol)) {
		const std::string texName = emissionColorTex.asChar();
		if (!texName.empty()) {
			mat->SetString("ai_emissionColor", texName);
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
			mat->SetString("Normal", texName);
		}
	}

	// --- store glTF standard material ---
	mat->SetFloat("Diffuse.R", baseCol.r * baseWeight);
	mat->SetFloat("Diffuse.G", baseCol.g * baseWeight);
	mat->SetFloat("Diffuse.B", baseCol.b * baseWeight);
	mat->SetFloat("Diffuse.A", transmission);
	if (baseColorTex.length() == 0) {
		mat->SetString("Diffuse", baseColorTex.asChar());
	}
	mat->SetFloat("metallicFactor", metallic);
	mat->SetFloat("roughnessFactor", specularRoughness);

	mat->SetFloat("Emission.R", emissionColorR * emissionWeight);
	mat->SetFloat("Emission.G", emissionColorG * emissionWeight);
	mat->SetFloat("Emission.B", emissionColorB * emissionWeight);
	if (emissionColorTex.length() == 0) {
		mat->SetString("Emission", emissionColorTex.asChar());
	}
	return true;
}

static
std::shared_ptr<kml::Material> ConvertMaterial(MObject& shaderObject)
{
	std::shared_ptr<kml::Material> mat = std::shared_ptr<kml::Material>(new kml::Material());

	mat->SetFloat("Diffuse.R", 1.0f);
	mat->SetFloat("Diffuse.G", 1.0f);
	mat->SetFloat("Diffuse.B", 1.0f);
	mat->SetFloat("Diffuse.A", 1.0f);

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

				mat->SetFloat("metallicFactor", 0.0f);
				mat->SetFloat("roughnessFactor", 1.0);
				MString coltexpath;
				MString normaltexpath;
				MColor col;
				if (getTextureAndColor(shader, MString("color"), coltexpath, col))
				{
					std::string texName = coltexpath.asChar();
					if (!texName.empty())
					{
						mat->SetString("Diffuse", texName);
					}
					else
					{
						mat->SetFloat("Diffuse.R", col.r);
						mat->SetFloat("Diffuse.G", col.g);
						mat->SetFloat("Diffuse.B", col.b);
						mat->SetFloat("Diffuse.A", col.a);
					}
				}

				MColor tra = getColor(shader, "transparency");
				mat->SetFloat("Diffuse.A", 1.0f - tra.r);

				// Normal map
				float depth;
				if (getNormalAttrib(shader, normaltexpath, depth))
				{
					std::string texName = normaltexpath.asChar();
					if (!texName.empty())
					{
						mat->SetString("Normal", texName);
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
MStatus WriteLodGLTF(
	TexturePathManager& texManager,
	std::map<int, std::shared_ptr<kml::Material> >& materials,
	std::vector< std::shared_ptr<kml::Node> >& nodes,
	const MString& dirname, MDagPath& dagPath, MObject& component)
{
	MStatus status = MS::kSuccess;
	std::shared_ptr<kml::Node> node = OutputPolygons(dagPath, component);
	if (status != MS::kSuccess)
	{
		fprintf(stderr, "Error: exporting geom failed, check your selection.\n");
		return MS::kFailure;
	}

	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	bool onefile = opts->GetInt("output_onefile") > 0;
	bool recalc_normals = opts->GetInt("recalc_normals") > 0;
	bool make_preload_texture = opts->GetInt("make_preload_texture") > 0;


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
						auto keys = mat->GetStringKeys();
						for (size_t j = 0; j < keys.size(); j++)
						{
							std::string key = keys[j];
							std::string orgPath    = mat->GetString(key);
							std::string copiedPath = MakeConvertTexturePath(texManager.GetCopiedPath(orgPath));
							if (!texManager.HasOriginalPath(orgPath))
							{
								copiedPath = std::string(dirname.asChar()) + "/" + GetFileExtName(copiedPath);
								texManager.SetPathPair(orgPath, copiedPath);
							}

							std::string dstPath;
							if (onefile)
							{
								dstPath += "./";
							}
							else
							{
								dstPath += "../";
							}
							dstPath += GetFileExtName(copiedPath);
							mat->SetString(key, dstPath);

							if (make_preload_texture)
							{
								std::string cachePath = GetCacheTexturePath(dstPath);
								mat->SetString(key + "S0", cachePath);
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
	int k = 0;
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

		if (2 <= tnodes.size())
		{
			tnodes[i]->SetBound(kml::CalculateBound(tnodes[i]->GetMesh()));
		}


		if (!onefile)
		{
			char buffer[32] = {};
			sprintf(buffer, "%d", k + 1);
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
		k++;
	}

	return status;
}

static
MStatus WriteLodGLTFThread(
	TexturePathManager& texManager,
	std::map<int, std::shared_ptr<kml::Material> >& materials,
	std::vector< std::shared_ptr<kml::Node> >& nodes,
	const MString& dirname, MDagPath& dagPath, MObject& component)
{
	MStatus status;
	std::thread th([&] { status = WriteLodGLTF(texManager, materials, nodes, dirname, dagPath, component) ; });
	th.join();
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
std::shared_ptr<kml::Node> CombineNodes(const std::vector< std::shared_ptr<kml::Node> >& nodes)
{
	std::shared_ptr<kml::Node> node(new kml::Node());

	//node->SetName(mdagPath.fullPathName().asChar());
	node->GetTransform()->SetMatrix(glm::mat4(1.0f));
	//node->SetMesh(mesh);
	
	for (size_t i = 0; i < nodes.size(); i++)
	{
		node->AddChild(nodes[i]);
	}

	glm::vec3 min = nodes[0]->GetBound()->GetMin();
	glm::vec3 max = nodes[0]->GetBound()->GetMax();

	for (size_t i = 1; i < nodes.size(); i++)
	{
		glm::vec3 cmin = nodes[i]->GetBound()->GetMin();
		glm::vec3 cmax = nodes[i]->GetBound()->GetMax();
		for (int j = 0; j < 3; j++)
		{
			min[j] = std::min(min[j], cmin[j]);
			max[j] = std::max(max[j], cmax[j]);
		}
	}
	node->SetBound( std::shared_ptr<kml::Bound>( new kml::Bound(min, max)));

	return node;
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

			if (!status) {
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

				if (dagPath.hasFn(MFn::kNurbsSurface))
				{
					status = MS::kSuccess;
					fprintf(stderr, "Warning: skipping Nurbs Surface.\n");
				}
				else if ((dagPath.hasFn(MFn::kMesh)) &&
					(dagPath.hasFn(MFn::kTransform)))
				{
					// We want only the shape, 
					// not the transform-extended-to-shape.
					continue;
				}
				else if (dagPath.hasFn(MFn::kMesh))
				{
					// Build a lookup table so we can determine which 
					// polygons belong to a particular edge as well as
					// smoothing information
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

		if ((  dagPath.hasFn(MFn::kNurbsSurface)) &&
			(  dagPath.hasFn(MFn::kTransform)))
		{
			status = MS::kSuccess;
			fprintf(stderr,"Warning: skipping Nurbs Surface.\n");
		}
		else if ((  dagPath.hasFn(MFn::kMesh)) &&
				 (  dagPath.hasFn(MFn::kTransform)))
		{
			continue;
		}
		else if (  dagPath.hasFn(MFn::kMesh))
		{
			dagPaths.push_back(dagPath);
		}
	}

	return exportProcess(fname, dagPaths);
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

MStatus glTFExporter::exportProcess(const MString& fname, const std::vector< MDagPath >& dagPaths)
{
	MStatus status = MS::kSuccess;

	std::string dir_path = GetExportDirectory(fname.asChar());

	MakeDirectory(dir_path);

	std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
	bool onefile = opts->GetInt("output_onefile") > 0;
	bool glb = opts->GetInt("output_glb") > 0;

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
				MObject  component = MObject::kNullObj;
				MDagPath dagPath = dagPaths[i];
				status = WriteLodGLTFThread(texManager, materials, nodes, MString(dir_path.c_str()), dagPath, component);
				progWindow->setProgress(prog * (i + 1));
			}
		}
	}

	if (nodes.empty())
	{
		return MStatus::kSuccess;
	}

	//texture copy
	{
		CopyTextureFiles(texManager);
	}
	//write files when "output_onefile"
	if (onefile)
	{
		int index = 0;
		for (ShaderMapType::iterator it = materials.begin(); it != materials.end(); it++)
		{
			auto& mat = it->second;
			mat->SetInteger("_Index", index);//
			index++;
		}
		for (NodeVecType::iterator it = nodes.begin(); it != nodes.end(); it++)
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
		std::shared_ptr<kml::Node> node = CombineNodes(nodes);
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
		/*
		{
			std::string json_path = dir_path + "/" + GetFileName(fname.asChar()) + ".json";
			picojson::object root;
			NodeToJson(root, node);//!nodes
			std::ofstream ofs(json_path);
			picojson::value(root).serialize(std::ostream_iterator<char>(ofs), true);
		}
		*/
	}
	else
	{
		std::string json_path = dir_path + "/" + GetFileName(fname.asChar()) + ".json";
		picojson::object root;
		NodesToJson(root, nodes);
		std::ofstream ofs(json_path);
		picojson::value(root).serialize(std::ostream_iterator<char>(ofs), true);
	}

	if (glb)
	{
		std::string gltf_path = dir_path + "/" + GetFileName(std::string(fname.asChar())) + ".gltf";
		std::string glb_path  = RemoveExt(std::string(fname.asChar())) + ".glb";
		if (!kml::GLTF2GLB(gltf_path, glb_path))
		{
			status = MStatus::kFailure;
		}
		RemoveDirectory_(dir_path);
	}

	return status;
}

