#define _USE_MATH_DEFINES
#ifdef _MSC_VER
#pragma warning(disable : 4819)
#endif

#ifdef _MSC_VER
#pragma comment(lib, "OpenMaya")
#pragma comment(lib, "OpenMayaAnim")
#pragma comment(lib, "OpenMayaUI")
#pragma comment(lib, "Shell32.lib")
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

#include <kml/CalculateBound.h>
#include <kml/CalculateNormalsMesh.h>
#include <kml/FlatIndicesMesh.h>
#include <kml/GLTF2GLB.h>
#include <kml/Mesh.h>
#include <kml/Node.h>
#include <kml/NodeExporter.h>
#include <kml/Options.h>
#include <kml/SplitNodeByMaterialID.h>
#include <kml/Texture.h>
#include <kml/TriangulateMesh.h>
#include <kml/glTFExporter.h>

#include <kil/CopyTextureFile.h>
#include <kil/ResizeTextureFile.h>

#include <maya/MAnimUtil.h>
#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDistance.h>
#include <maya/MEulerRotation.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVector.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnLambertShader.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnPhongEShader.h>
#include <maya/MFnSet.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MIOStream.h>
#include <maya/MIntArray.h>
#include <maya/MItDag.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MObjectArray.h>
#include <maya/MObjectHandle.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MPxCommand.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MQuaternion.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MStreamUtils.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MStringResource.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MVector.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>

#include <Shellapi.h>
#endif

#include "ProgressWindow.h"
#include "murmur3.h"

#if defined(_WIN32)
//#define strcasecmp stricmp
#elif defined(OSMac_)
#if defined(MAC_CONVERT_FILE)
#include <sys/param.h>
#if USING_MAC_CORE_LIB
#include <CFURL.h>
#include <Files.h>
extern "C" int strcasecmp(const char*, const char*);
extern "C" Boolean createMacFile(const char* fileName, FSRef* fsRef, long creator, long type);
#endif
#endif
#endif

#define NO_SMOOTHING_GROUP -1
#define INITIALIZE_SMOOTHING -2
#define INVALID_ID -1

static std::string GetDirectoryPath(const std::string& path)
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

static std::string RemoveExt(const std::string& path)
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
    int index = path.find_last_of(".");
    if (index != std::string::npos)
    {
        return path.substr(0, index);
    }
    else
    {
        return path;
    }
#endif
}

static std::string GetFileName(const std::string& path)
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
    int index = path.find_last_of("/");
    if (index != std::string::npos)
    {
        return RemoveExt(path.substr(index + 1));
    }
    else
    {
        return RemoveExt(path);
    }
#endif
}

static std::string GetFileExtName(const std::string& path)
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
    int i = path.find_last_of("/");
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

static char ReplaceForPath(char c)
{
    if (c == '|')
        return '_';
    if (c == '\\')
        return '_';
    if (c == '/')
        return '_';
    if (c == ':')
        return '_';
    if (c == '*')
        return '_';
    if (c == '?')
        return '_';
    if (c == '"')
        return '_';
    if (c == '<')
        return '_';
    if (c == '>')
        return '_';

    return c;
}

static std::string ReplaceForPath(const std::string& path)
{
    std::stringstream ss;

    for (size_t i = 0; i < path.size(); i++)
    {
        ss << ReplaceForPath(path[i]);
    }
    return ss.str();
}

static char ToChar(int n)
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

static std::string MakeForPath(const std::string& path)
{
    static const int MAX_FNAME = 32; // _MAX_FNAME 256 / 8
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

static std::string MakeDirectoryPath(const std::string& path)
{
    std::string tpath = ReplaceForPath(path);
    tpath = MakeForPath(tpath);
    tpath = ReplaceForPath(tpath);
    return tpath;
}

static void MakeDirectory(const std::string& path)
{
#ifdef _WIN32
    ::CreateDirectoryA(path.c_str(), NULL);
#else
    mode_t mode = 0755;
    int ret = mkdir(path.c_str(), mode);
    if (ret == 0)
    {
        // ok
        return;
    }
    std::cerr << "Directory creation error : " << path << std::endl;
#endif
}

static std::string GetExt(const std::string& filepath)
{
    if (filepath.find_last_of(".") != std::string::npos)
        return filepath.substr(filepath.find_last_of("."));
    return "";
}

static std::string MakeConvertTexturePath(const std::string& path)
{
    std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
    int convert_texture_format = opts->GetInt("convert_texture_format");
    switch (convert_texture_format)
    {
    case 0:
        return path;
    case 1:
        return RemoveExt(path) + ".jpg";
    case 2:
        return RemoveExt(path) + ".png";
    }
    return path;
}

static bool RemoveFiles(const std::string& path)
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
    std::string cmd;
    cmd += "rm -rf";
    cmd += " ";
    cmd += "\"";
    cmd += path;
    cmd += "\"";
    int ret = ::system(cmd.c_str());
    (void)ret;
    return true;
#endif
}

static std::string GetTempDirectory()
{
#ifdef _WIN32
    char dir1[_MAX_PATH + 1] = {};
    char dir2[_MAX_PATH + 1] = {};
    GetTempPathA(_MAX_PATH + 1, dir1);

    GetTempFileNameA(dir1, "tmp", 0, dir2);
    ::DeleteFileA(dir2);

    return RemoveExt(dir2);
#else // Linux and macOS
    std::string tmpdir = std::string(getenv("TMPDIR")) + "XXXXXX";

    char buffer[1024] = {};
    strcpy(buffer, tmpdir.c_str());
    std::string tmpPath = ::mktemp(buffer);
    std::string cmd = "mkdir -p ";
    cmd += "\"" + tmpPath + "\"";
    int ret = ::system(cmd.c_str());
    (void)ret;
    return tmpPath;
#endif
}

static bool IsFileExist(const std::string& filepath)
{
#ifdef _WIN32
    struct _stat st;
    int nRet = ::_stat(filepath.c_str(), &st);
    return (nRet == 0);
#else
    struct stat st;
    int nRet = ::stat(filepath.c_str(), &st);
    return (nRet == 0);
#endif
}

static bool IsVisible(MFnDagNode& fnDag)
{
    MStatus status;
    if (fnDag.isIntermediateObject())
    {
        return false;
    }

    MPlug visPlug = fnDag.findPlug("visibility", &status);
    if (MStatus::kSuccess != status)
    {
        return false;
    }
    else
    {
        bool visible;
        status = visPlug.getValue(visible);
        if (MStatus::kSuccess != status)
        {
            return false;
        }
        return visible;
    }
}

static bool IsVisible(MDagPath& path)
{
    MStatus status;
    MFnDagNode node(path, &status);
    if (MStatus::kSuccess != status)
    {
        return false;
    }
    else
    {
        return IsVisible(node);
    }
}

//////////////////////////////////////////////////////////////
typedef glTFExporter::Mode Mode;
static const Mode EXPORT_GLTF = glTFExporter::EXPORT_GLTF;
static const Mode EXPORT_GLB = glTFExporter::EXPORT_GLB;
static const Mode EXPORT_VRM = glTFExporter::EXPORT_VRM;
glTFExporter::glTFExporter(Mode mode)
{
    this->mode_ = mode;
}

void* glTFExporter::creatorGLTF()
{
    return new glTFExporter(EXPORT_GLTF);
}

void* glTFExporter::creatorGLB()
{
    return new glTFExporter(EXPORT_GLB);
}

void* glTFExporter::creatorVRM()
{
    return new glTFExporter(EXPORT_VRM);
}

//////////////////////////////////////////////////////////////

MStatus glTFExporter::reader(const MFileObject& file,
                             const MString& options,
                             FileAccessMode mode)
{
    fprintf(stderr, "glTFExporter::reader called in error\n");
    return MS::kFailure;
}

//////////////////////////////////////////////////////////////
#if defined(OSMac_) && defined(MAC_CONVERT_FILE)
// Convert file system representations
// Possible styles: kCFURLHFSPathStyle, kCFURLPOSIXPathStyle
// kCFURLHFSPathStyle = Emerald:aw:Maya:projects:default:scenes:eagle.ma
// kCFURLPOSIXPathStyle = /Volumes/Emerald/aw/Maya/projects/default/scenes/eagle.ma
// The conversion will be done in place, so make sure fileName is big enough
// to hold the result
//
static Boolean
convertFileRepresentation(char* fileName, short inStyle, short outStyle)
{
    if (fileName == NULL)
    {
        return (false);
    }
    if (inStyle == outStyle)
    {
        return (true);
    }

    CFStringRef rawPath = CFStringCreateWithCString(NULL, fileName, kCFStringEncodingUTF8);
    if (rawPath == NULL)
    {
        return (false);
    }
    CFURLRef baseURL = CFURLCreateWithFileSystemPath(NULL, rawPath, (CFURLPathStyle)inStyle, false);
    CFRelease(rawPath);
    if (baseURL == NULL)
    {
        return (false);
    }
    CFStringRef newURL = CFURLCopyFileSystemPath(baseURL, (CFURLPathStyle)outStyle);
    CFRelease(baseURL);
    if (newURL == NULL)
    {
        return (false);
    }
    char newPath[MAXPATHLEN];
    CFStringGetCString(newURL, newPath, MAXPATHLEN, kCFStringEncodingUTF8);
    CFRelease(newURL);
    strcpy(fileName, newPath);
    return (true);
}
#endif

//////////////////////////////////////////////////////////////

bool glTFExporter::haveReadMethod() const
{
    return false;
}
//////////////////////////////////////////////////////////////

bool glTFExporter::haveWriteMethod() const
{
    return true;
}
//////////////////////////////////////////////////////////////

MString glTFExporter::defaultExtension() const
{
    switch ((int)this->mode_)
    {
    case EXPORT_GLTF:
        return "gltf";
    case EXPORT_GLB:
        return "glb";
    case EXPORT_VRM:
        return "vrm";
    }
    return "gltf";
}
//////////////////////////////////////////////////////////////

MPxFileTranslator::MFileKind glTFExporter::identifyFile(
    const MFileObject& fileName,
    const char* buffer,
    short size) const
{
    const char* name = fileName.name().asChar();
    int nameLength = (int)strlen(name);
    switch ((int)this->mode_)
    {
    case EXPORT_GLTF:
    {
        if ((nameLength > 5) && !strcasecmp(name + nameLength - 5, ".gltf"))
        {
            return kIsMyFileType;
        }
        break;
    }
    case EXPORT_GLB:
    {
        if ((nameLength > 4) && !strcasecmp(name + nameLength - 4, ".glb"))
        {
            return kIsMyFileType;
        }
        break;
    }
    case EXPORT_VRM:
    {
        if ((nameLength > 4) && !strcasecmp(name + nameLength - 4, ".vrm"))
        {
            return kIsMyFileType;
        }
        break;
    }
    }

    return kNotMyFileType;
}
//////////////////////////////////////////////////////////////
MString glTFExporter::filter() const
{
    switch ((int)this->mode_)
    {
    case EXPORT_GLTF:
        return "*.gltf";
    case EXPORT_GLB:
        return "*.glb";
    case EXPORT_VRM:
        return "*.vrm";
    }
    return "*.glb";
}

//////////////////////////////////////////////////////////////

static std::string ConvertVRMLicenseType(const std::string& type)
{
    static const struct
    {
        const char* szSrc;
        const char* szDst;
    } LicenseType[]{
        {"Redistribution Prohibited", "Redistribution_Prohibited"},
        {"CC0", "CC0"},
        {"CC BY", "CC_BY"},
        {"CC BY-NC", "CC_BY_NC"},
        {"CC BY-SA", "CC_BY_SA"},
        {"CC BY-NC-SA", "CC_BY_NC_SA"},
        {"CC BY-ND", "CC_BY_ND"},
        {"CC BY-NC-ND", "CC_BY_NC_ND"},
        {"Other", "Other"},
        {NULL, NULL}};

    int i = 0;
    while (LicenseType[i].szSrc != NULL)
    {
        if (strcmp(type.c_str(), LicenseType[i].szSrc) == 0)
        {
            return LicenseType[i].szDst;
        }
        i++;
    }
    return "CC_BY";
}

MStatus glTFExporter::writer(const MFileObject& file, const MString& options, FileAccessMode mode)

{
    MStatus status;
    MString mname = file.fullName(), unitName;

#if defined(OSMac_) && defined(MAC_CONVERT_FILE)
    char fname[MAXPATHLEN];
    strcpy(fname, file.fullName().asChar());
    FSRef notUsed;
    //Create a file else convertFileRep will fail.
    createMacFile(fname, &notUsed, 0, 0);
    convertFileRepresentation(fname, kCFURLPOSIXPathStyle, kCFURLHFSPathStyle);
#else
    const char* fname = mname.asChar();
#endif

#if 1
    // Options
    //
    std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();

    int recalc_normals = 0;                //0:no, 1:recalc
    int output_onefile = 1;                //0:sep, 1:one file
    int output_glb = 0;                    //0:json,1:glb
    int vrm_export = 0;                    //0:gltf or glb, 1:vrm
    int make_preload_texture = 0;          //0:off, 1:on
    int output_buffer = 1;                 //0:bin, 1:draco, 2:bin/draco
    int convert_texture_format = 0;        //0:no convert, 1:jpeg, 2:png
    int transform_space = 1;               //0:world_space, 1:local_space
    int freeze_skinned_mesh_transform = 1; //0:no_bake, 1:bake veritces
    int output_animations = 1;             //0:no output, 1: output animation
    int output_invisible_nodes = 0;        //0:
    int output_single_animation = 0;       //0:off, 1:on

    switch ((int)this->mode_)
    {
    case EXPORT_GLTF:
        output_glb = 0;
        vrm_export = 0;
        break;
    case EXPORT_GLB:
        output_glb = 1;
        vrm_export = 0;
        break;
    case EXPORT_VRM:
        output_glb = 1;
        vrm_export = 1;
        break;
    }

    std::string generator_name = opts->GetString("generator_name");
    std::string generator_version = opts->GetString("generator_version");

    std::string vrm_product_title = "notitle";
    std::string vrm_product_version = "1.00";
    std::string vrm_product_author = "unknown";
    std::string vrm_product_contact_information = "";
    std::string vrm_product_reference = "";

    int vrm_license_allowed_user_name = 2; //everyone
    int vrm_license_violent_usage = 1;     //allow
    int vrm_license_sexual_usage = 1;      //allow
    int vrm_license_commercial_usage = 1;  //allow

    std::string vrm_license_other_permission_url = "";

    std::string vrm_license_license_type = "CC_BY";
    std::string vrm_license_other_license_url = "";

    if (options.length() > 0)
    {
        int i, length;
        // Start parsing.
        MStringArray optionList;
        MStringArray theOption;
        options.split(';', optionList); // break out all the options.

        length = optionList.length();
        for (i = 0; i < length; ++i)
        {
            theOption.clear();
            optionList[i].split('=', theOption);

            std::string debugName = optionList[i].asChar();

            if (theOption[0] == MString("recalc_normals") && theOption.length() > 1)
            {
                recalc_normals = theOption[1].asInt();
            }
            if (theOption[0] == MString("output_onefile") && theOption.length() > 1)
            {
                output_onefile = theOption[1].asInt();
            }
            /*
            if (theOption[0] == MString("output_glb") && theOption.length() > 1) {
                output_glb = theOption[1].asInt();
            }
            if (theOption[0] == MString("vrm_export") && theOption.length() > 1) {
                vrm_export = theOption[1].asInt();
            }
            */
            if (theOption[0] == MString("make_preload_texture") && theOption.length() > 1)
            {
                make_preload_texture = theOption[1].asInt();
            }
            if (theOption[0] == MString("output_buffer") && theOption.length() > 1)
            {
                output_buffer = theOption[1].asInt();
            }
            if (theOption[0] == MString("convert_texture_format") && theOption.length() > 1)
            {
                convert_texture_format = theOption[1].asInt();
            }
            if (theOption[0] == MString("transform_space") && theOption.length() > 1)
            {
                transform_space = theOption[1].asInt();
            }
            if (theOption[0] == MString("freeze_skinned_mesh_transform") && theOption.length() > 1)
            {
                freeze_skinned_mesh_transform = theOption[1].asInt();
            }
            if (theOption[0] == MString("output_animations") && theOption.length() > 1)
            {
                output_animations = theOption[1].asInt();
            }
            if (theOption[0] == MString("output_single_animation") && theOption.length() > 1)
            {
                output_single_animation = theOption[1].asInt();
            }
            if (theOption[0] == MString("output_invisible_nodes") && theOption.length() > 1)
            {
                output_invisible_nodes = theOption[1].asInt();
            }
            if (theOption[0] == MString("generator_name") && theOption.length() > 1)
            {
                generator_name = theOption[1].asChar();
            }
#ifdef ENABLE_VRM

            if (theOption[0] == MString("vrm_product_title") && theOption.length() > 1)
            {
                vrm_product_title = theOption[1].asChar();
            }
            if (theOption[0] == MString("vrm_product_version") && theOption.length() > 1)
            {
                vrm_product_version = theOption[1].asChar();
            }
            if (theOption[0] == MString("vrm_product_author") && theOption.length() > 1)
            {
                vrm_product_author = theOption[1].asChar();
            }
            if (theOption[0] == MString("vrm_product_contact_information") && theOption.length() > 1)
            {
                vrm_product_contact_information = theOption[1].asChar();
            }
            if (theOption[0] == MString("vrm_product_reference") && theOption.length() > 1)
            {
                vrm_product_reference = theOption[1].asChar();
            }

            if (theOption[0] == MString("vrm_license_allowed_user_name") && theOption.length() > 1)
            {
                vrm_license_allowed_user_name = theOption[1].asInt();
            }
            if (theOption[0] == MString("vrm_license_violent_usage") && theOption.length() > 1)
            {
                vrm_license_violent_usage = theOption[1].asInt();
            }
            if (theOption[0] == MString("vrm_license_sexual_usage") && theOption.length() > 1)
            {
                vrm_license_sexual_usage = theOption[1].asInt();
            }
            if (theOption[0] == MString("vrm_license_commercial_usage") && theOption.length() > 1)
            {
                vrm_license_commercial_usage = theOption[1].asInt();
            }
            if (theOption[0] == MString("vrm_license_other_permission_url") && theOption.length() > 1)
            {
                vrm_license_other_permission_url = theOption[1].asChar();
            }

            if (theOption[0] == MString("vrm_license_license_type") && theOption.length() > 1)
            {
                vrm_license_license_type = ConvertVRMLicenseType(theOption[1].asChar());
            }
            if (theOption[0] == MString("vrm_license_other_license_url") && theOption.length() > 1)
            {
                vrm_license_other_license_url = theOption[1].asChar();
            }
#endif
        }
    }

    if (vrm_export)
    {
        output_buffer = 0;                 // disable Draco
        output_glb = 1;                    // force GLB format
        freeze_skinned_mesh_transform = 1; // bake mesh!
        output_animations = 0;             //
    }

    if (output_glb)
    {
        output_onefile = 1;       //enforce onefile
        make_preload_texture = 0; //no cache texture
    }

#ifdef ENABLE_VRM
    if (
        (vrm_license_license_type != "Other" && vrm_license_license_type != "Redistribution_Prohibited") &&
        vrm_license_other_license_url.empty())
    {
        vrm_license_other_license_url = "https://creativecommons.org/licenses/";
    }
#endif

    opts->SetInt("recalc_normals", recalc_normals);
    opts->SetInt("output_onefile", output_onefile);
    opts->SetInt("output_glb", output_glb);
    opts->SetInt("make_preload_texture", make_preload_texture);
    opts->SetInt("output_buffer", output_buffer);
    opts->SetInt("convert_texture_format", convert_texture_format);
    opts->SetInt("transform_space", transform_space);
    opts->SetInt("freeze_skinned_mesh_transform", freeze_skinned_mesh_transform);
    opts->SetInt("output_animations", output_animations);
    opts->SetInt("output_single_animation", output_single_animation);
    opts->SetInt("output_invisible_nodes", output_invisible_nodes);

    opts->SetString("generator_name", generator_name);

    opts->SetInt("vrm_export", vrm_export);

    opts->SetString("vrm_product_title", vrm_product_title);
    opts->SetString("vrm_product_version", vrm_product_version);
    opts->SetString("vrm_product_author", vrm_product_author);
    opts->SetString("vrm_product_contact_information", vrm_product_contact_information);
    opts->SetString("vrm_product_reference", vrm_product_reference);

    opts->SetInt("vrm_license_allowed_user_name", vrm_license_allowed_user_name);
    opts->SetInt("vrm_license_violent_usage", vrm_license_violent_usage);
    opts->SetInt("vrm_license_sexual_usage", vrm_license_sexual_usage);
    opts->SetInt("vrm_license_commercial_usage", vrm_license_commercial_usage);
    opts->SetString("vrm_license_other_permission_url", vrm_license_other_permission_url);

    opts->SetString("vrm_license_license_type", vrm_license_license_type);
    opts->SetString("vrm_license_other_license_url", vrm_license_other_license_url);

    /* print current linear units used as a comment in the obj file */
    //setToLongUnitName(MDistance::uiUnit(), unitName);
    //fprintf( fp, "# This file uses %s as units for non-parametric coordinates.\n\n", unitName.asChar() );
    //fprintf( fp, "# The units used in this file are %s.\n", unitName.asChar() );
#endif
    if ((mode == MPxFileTranslator::kExportAccessMode) ||
        (mode == MPxFileTranslator::kSaveAccessMode))
    {
        return exportAll(fname);
    }
    else if (mode == MPxFileTranslator::kExportActiveAccessMode)
    {
        return exportSelected(fname);
    }

    return MS::kSuccess;
}

//////////////////////////////////////////////////////////////

static std::vector<std::string> SplitPath(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos)
            pos = str.length();
        std::string token = str.substr(prev, pos - prev);
        if (!token.empty())
            tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

static std::vector<MDagPath> GetDagPathList(const std::string& fullPathNameh)
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

static std::vector<MDagPath> GetDagPathList(const MDagPath& dagPath)
{
    return GetDagPathList(dagPath.fullPathName().asChar());
}

static void SetTRS(const MDagPath& dagPath, std::shared_ptr<kml::Node>& node)
{
    MFnTransform fnTransform(dagPath);
    MVector mT = fnTransform.getTranslation(MSpace::kTransform);
    MQuaternion mR, mOR, mJO;
    fnTransform.getRotation(mR, MSpace::kTransform);
    MStatus ret;
    mOR = fnTransform.rotateOrientation(MSpace::kTransform, &ret); // get Rotation Axis
    if (ret == MS::kSuccess)
    {
        mR = mOR * mR;
    }

    MFnIkJoint fnJoint(dagPath);
    MStatus joret = fnJoint.getOrientation(mJO); // get jointOrient
    if (joret == MS::kSuccess)
    {
        mR *= mJO;
    }

    double mS[3];
    fnTransform.getScale(mS);

    glm::vec3 vT(mT.x, mT.y, mT.z);
    glm::quat vR(mR.w, mR.x, mR.y, mR.z); //wxyz
    glm::vec3 vS(mS[0], mS[1], mS[2]);
    node->GetTransform()->SetTRS(vT, vR, vS);
}

static MObject GetOriginalMesh(const MDagPath& dagPath)
{
    MFnDependencyNode node(dagPath.node());
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
        skinC.findPlug("input").elementByLogicalIndex(skinC.indexForOutputShape(dagPath.node())).child(0).getValue(vtxobj);
    }
    return vtxobj;
}

static std::shared_ptr<kml::Mesh> CreateMesh(const MDagPath& dagPath, const MSpace::Space& space)
{
    MStatus status = MS::kSuccess;

    MFnMesh fnMesh(dagPath);
    MItMeshPolygon polyIter(dagPath);
    MItMeshVertex vtxIter(dagPath);

    // Write out the vertex table
    //
    std::vector<glm::vec3> positions;
    for (; !vtxIter.isDone(); vtxIter.next())
    {
        MPoint p = vtxIter.position(space);
        /*
        if (ptgroups && groups && (objectIdx >= 0)) {
        int compIdx = vtxIter.index();
        outputSetsAndGroups( dagPath, compIdx, true, objectIdx );
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
            for (int t = 0; t < normsLength; t++)
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

    std::shared_ptr<kml::Mesh> mesh(new kml::Mesh());
    mesh->facenums.swap(facenums);
    mesh->pos_indices.swap(pos_indices);
    mesh->tex_indices.swap(tex_indices);
    mesh->nor_indices.swap(nor_indices);
    mesh->positions.swap(positions);
    mesh->texcoords.swap(texcoords);
    mesh->normals.swap(normals);
    mesh->materials.swap(materials);
    //mesh->name = dagPath.partialPathName().asChar();

    return mesh;
}

static std::shared_ptr<kml::Mesh> GetOriginalVertices(std::shared_ptr<kml::Mesh>& mesh, MObject& orgMeshObj)
{
    MFnMesh fnMesh(orgMeshObj);
    MPointArray vtx_pos;
    fnMesh.getPoints(vtx_pos, MSpace::kObject);
    MFloatVectorArray norm;
    fnMesh.getNormals(norm, MSpace::kObject);

    std::vector<glm::vec3> positions;
    for (int i = 0; i < vtx_pos.length(); i++)
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

static std::shared_ptr<kml::Mesh> GetSkinWeights(std::shared_ptr<kml::Mesh>& mesh, const MDagPath& dagPath)
{
    MFnDependencyNode node(dagPath.node());
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
    skinC.findPlug("input").elementByLogicalIndex(skinC.indexForOutputShape(dagPath.node())).child(0).getValue(vtxobj);

    std::shared_ptr<kml::SkinWeight> skin_weight(new kml::SkinWeight());
    skin_weight->name = skinC.name().asChar();
    skin_weight->weights.resize(mesh->positions.size());
    MDagPathArray dpa;
    MStatus stat;
    int jsz = skinC.influenceObjects(dpa, &stat);
    int ksz = skinC.numOutputConnections();
    //assert(ksz == 1); // maybe 1 only
    ksz = std::min<int>(ksz, 1); //TODO
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
            MItGeometry geoit(dagPath);
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
                        skin_weight->weights[index][joint_paths[l]] = ws[l];
                    }
                }
                geoit.next();
            }
        }

        struct StringSizeSorter
        {
            bool operator()(const std::string& a, const std::string& b) const
            {
                return a.size() < b.size();
            }
        };

        std::sort(joint_paths.begin(), joint_paths.end());
        joint_paths.erase(std::unique(joint_paths.begin(), joint_paths.end()), joint_paths.end());
        std::sort(joint_paths.begin(), joint_paths.end(), StringSizeSorter());
        skin_weight->joint_paths = joint_paths;
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
                dest[3][0], dest[3][1], dest[3][2], dest[3][3]);
            path_mat_map[joint_path] = mat;
        }

        for (int j = 0; j < skin_weight->joint_paths.size(); j++)
        {
            glm::mat4 mat = path_mat_map[skin_weight->joint_paths[j]];
            skin_weight->joint_bind_matrices.push_back(mat);
        }
    }

    mesh->skin_weight = skin_weight;

    return mesh;
}

static std::shared_ptr<kml::MorphTarget> GetMorphTarget(MObject& obj)
{
    std::shared_ptr<kml::Mesh> mesh(new kml::Mesh());
    mesh = GetOriginalVertices(mesh, obj);
    std::shared_ptr<kml::MorphTarget> target(new kml::MorphTarget());
    target->positions.swap(mesh->positions);
    target->normals.swap(mesh->normals);
    return target;
}

static std::shared_ptr<kml::Mesh> GetMophTargets(std::shared_ptr<kml::Mesh>& mesh, const MDagPath& dagPath)
{
    MStatus status;
    MFnDependencyNode node(dagPath.node(), &status);
    if (status != MStatus::kSuccess)
    {
        return mesh;
    }

    MPlugArray mpa;
    MPlug mp = node.findPlug("inMesh");
    mp.connectedTo(mpa, true, false);

    MObject vtxobj = MObject::kNullObj;
    int isz = mpa.length();
    if (isz < 1)
    {
        return mesh;
    }

    MFnDependencyNode dnode(mpa[0].node(), &status);
    if (status != MStatus::kSuccess)
    {
        return mesh;
    }
    if (dnode.typeName() != "blendShape")
    {
        return mesh;
    }

    MFnBlendShapeDeformer fnBlendShapeDeformer(mpa[0].node(), &status);
    if (status != MStatus::kSuccess)
    {
        return mesh;
    }

    MObjectArray baseArray;
    fnBlendShapeDeformer.getBaseObjects(baseArray);

    int numBase = baseArray.length();
    MIntArray indexList;
    fnBlendShapeDeformer.weightIndexList(indexList);
    int numWeight = indexList.length();

    if (numBase < 1)
    {
        return mesh;
    }

    float envelope = fnBlendShapeDeformer.envelope();
    {
        fnBlendShapeDeformer.setEnvelope(0.0f);
        std::shared_ptr<kml::Mesh> tmesh(new kml::Mesh());
        tmesh = GetOriginalVertices(tmesh, baseArray[0]);
        mesh->positions.swap(tmesh->positions);
        mesh->normals.swap(tmesh->normals);
        fnBlendShapeDeformer.setEnvelope(envelope);
    }

    std::shared_ptr<kml::MorphTargets> morph_targets(new kml::MorphTargets());
    for (int i = 0; i < numWeight; i++)
    {
        MObjectArray targetArray;
        fnBlendShapeDeformer.getTargets(baseArray[0], indexList[i], targetArray);
        if (targetArray.length() < 1)
        {
            continue;
        }
        float weight = fnBlendShapeDeformer.weight(indexList[i]) * envelope;
        for (unsigned int j = 0; j < targetArray.length(); j++)
        {
            MObject targetObj = targetArray[j];
            std::shared_ptr<kml::MorphTarget> target = GetMorphTarget(targetObj);
            MDagPath path;
            MDagPath::getAPathTo(targetObj, path);
            path.pop();
            std::string name = path.partialPathName().asChar();
            morph_targets->targets.push_back(target);
            morph_targets->weights.push_back(weight);
            morph_targets->names.push_back(name);
        }
    }

    mesh->morph_targets = morph_targets;

    return mesh;
}

static glm::vec3 Transform(const glm::mat4& mat, const glm::vec3& v)
{
    glm::vec4 t = mat * glm::vec4(v, 1.0f);
    t[0] /= t[3];
    t[1] /= t[3];
    t[2] /= t[3];
    return glm::vec3(t[0], t[1], t[2]);
}

static std::shared_ptr<kml::Mesh> TransformMesh(std::shared_ptr<kml::Mesh>& mesh, const glm::mat4& mat)
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

static std::shared_ptr<kml::Node> CreateMeshNode(const MDagPath& dagPath)
{
    MStatus status = MS::kSuccess;

    std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
    int transform_space = opts->GetInt("transform_space");
    bool freeze_skinned_mesh_transform = opts->GetInt("freeze_skinned_mesh_transform") > 0;
    bool output_animations = opts->GetInt("output_animations") > 0;
    bool vrm = opts->GetInt("vrm_export") > 0;

    MSpace::Space space = MSpace::kWorld;
    if (transform_space == 1)
    {
        space = MSpace::kObject;
    }

    std::shared_ptr<kml::Mesh> mesh = CreateMesh(dagPath, space);

    if (!mesh.get())
    {
        return std::shared_ptr<kml::Node>();
    }

    if (transform_space == 1)
    {
        if (output_animations)
        {
            MObject orgMeshObj = GetOriginalMesh(dagPath); //T-pose
            if (orgMeshObj.hasFn(MFn::kMesh))
            {
                mesh = GetOriginalVertices(mesh, orgMeshObj); //dynamic
            }
        }
        if (output_animations || vrm)
        {
            mesh = GetSkinWeights(mesh, dagPath); //dynamic
        }

        mesh = GetMophTargets(mesh, dagPath); //dynamic
    }

    mesh->name = dagPath.partialPathName().asChar();

    std::shared_ptr<kml::Node> node(new kml::Node());

    if (transform_space == 0)
    {
        node->GetTransform()->SetIdentity();
    }
    else if (transform_space == 1)
    {
        std::vector<MDagPath> dagPathList = GetDagPathList(dagPath);
        dagPathList.pop_back(); //shape
        MDagPath path = dagPathList.back();
        SetTRS(path, node);
    }
    node->SetMesh(mesh);
    node->SetBound(kml::CalculateBound(mesh));

    if (transform_space == 0)
    {
        std::vector<MDagPath> dagPathList = GetDagPathList(dagPath);
        dagPathList.pop_back(); //shape
        MDagPath transPath = dagPathList.back();
        node->SetName(transPath.partialPathName().asChar());
        node->SetPath(transPath.fullPathName().asChar());

        return node;
    }
    else
    {
        std::vector<MDagPath> dagPathList = GetDagPathList(dagPath);
        dagPathList.pop_back(); //shape
        MDagPath transPath = dagPathList.back();
        node->SetName(transPath.partialPathName().asChar());
        node->SetPath(transPath.fullPathName().asChar());
        node->SetVisiblity(IsVisible(transPath));

        dagPathList.pop_back(); //transform
        if (dagPathList.size() > 0)
        {
            std::vector<std::shared_ptr<kml::Node> > nodes;
            for (size_t i = 0; i < dagPathList.size(); i++)
            {
                MDagPath path = dagPathList[i];
                std::shared_ptr<kml::Node> n(new kml::Node());
                n->SetName(path.partialPathName().asChar());
                n->SetPath(path.fullPathName().asChar());
                n->SetVisiblity(IsVisible(path));

                SetTRS(path, n);
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

static void GetMaterials(std::vector<MObject>& shaders, std::vector<int>& faceIndices, const MDagPath& dagPath)
{
    typedef std::map<unsigned int, MObject> MapType;
    MapType shader_map;
    {
        MStatus status = MStatus::kSuccess;
        MFnMesh fnMesh(dagPath, &status);
        if (MS::kSuccess != status)
            return;

        int numInstances = 1; // fnMesh.parentCount();
        for (int j = 0; j < numInstances; j++)
        {
            MObjectArray Shaders;
            MIntArray FaceIndices;
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

static MColor getColor(MFnDependencyNode& node, const char* name)
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

static std::string GetWorkspacePath()
{
    MString command = "workspace -q -rd;";
    return MGlobal::executeCommandStringResult(command).asChar();
}

static std::string GetTexturePath(const std::string& path)
{
    std::string texpath = path;
    if (IsFileExist(texpath))
    {
        return texpath;
    }

    std::string ws = GetWorkspacePath();

    // project path delimitor
    const std::string pathDelimiter = "//";
    size_t delim = texpath.find(pathDelimiter);
    if (delim != std::string::npos)
    {
        texpath.erase(0, delim + pathDelimiter.size());
    }
    if (IsFileExist(texpath))
    {
        return texpath;
    }

    std::string wsPath = ws + texpath;
    if (IsFileExist(wsPath))
    {
        return wsPath;
    }

    return "";
}

static bool getTextureAndColor(const MFnDependencyNode& node, const MString& name, std::shared_ptr<kml::Texture>& tex, MColor& color)
{
    color = MColor(1.0, 1.0, 1.0, 1.0);
    MStatus status;
    MPlug paramPlug = node.findPlug(name, &status);
    if (status != MS::kSuccess)
        return false;

    MPlugArray connectedPlugs;
    if (paramPlug.connectedTo(connectedPlugs, true, false, &status) && connectedPlugs.length())
    {
        MFn::Type apiType = connectedPlugs[0].node().apiType();
        if (apiType == MFn::kFileTexture)
        {
            MFnDependencyNode texNode(connectedPlugs[0].node());
            MPlug texturePlug = texNode.findPlug("fileTextureName", &status);
            if (status == MS::kSuccess)
            {
                MString tpath;
                texturePlug.getValue(tpath);
                std::string texpath = tpath.asChar();

                texpath = GetTexturePath(texpath);
                if (!texpath.empty())
                {
                    tex = std::shared_ptr<kml::Texture>(new kml::Texture);
                    tex->SetFilePath(texpath);

                    // filter
                    const int filterType = texNode.findPlug("filter").asInt();
                    if (filterType == 0)
                    { // None
                        tex->SetFilter(kml::Texture::FILTER_NEAREST);
                    }
                    else
                    { // Linear
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
                    if (tilingMode == 0)
                    { // OFF
                        tex->SetUDIMMode(false);
                    }
                    else if (tilingMode == 3)
                    { // UDIM
                        tex->SetUDIMMode(true);

                        // get <UDIM> tag filepath
                        MPlug compNamePlug = texNode.findPlug("computedFileTextureNamePattern", &status);
                        MString ctpath;
                        compNamePlug.getValue(ctpath);
                        std::string ctexpath = ctpath.asChar();
                        tex->SetUDIMFilePath(ctexpath);
                    }
                    else
                    { // Not Support
                        fprintf(stderr, "Error: Not support texture tiling mode.\n");
                    }
                    // if material has texture, set color(1,1,1)
                    color.r = 1.0f;
                    color.g = 1.0f;
                    color.b = 1.0f;
                    return true;
                }
                
                // Colorspace
                MPlug colorSpacePlug = texNode.findPlug("colorSpace");
                MString colorSpaceStr;
                colorSpacePlug.getValue(colorSpaceStr);
                const std::string colorSpace = colorSpaceStr.asChar();
                if (!colorSpace.empty()) {
                    tex->SetColorSpace(colorSpace);
                }
                

                // if material has texture, set color(1,1,1)
                color.r = 1.0f;
                color.g = 1.0f;
                color.b = 1.0f;
                return true;
            }
            return false;
        }
        else
        {
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
    if (apiType == MFn::kBump)
    {
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
    else if (apiType == MFn::kFileTexture)
    {
        MFnDependencyNode texNode(connectedPlugs[0].node());
        MPlug texturePlug = texNode.findPlug("fileTextureName", &status);
        if (status != MS::kSuccess)
        {
            return false;
        }

        texturePlug.getValue(normaltexpath);

        return true;
    }
    else
    {
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

static bool isAiStandardHairShader(const MFnDependencyNode& materialDependencyNode)
{
    return materialDependencyNode.hasAttribute("baseColor") &&
           materialDependencyNode.hasAttribute("melanin") &&
           materialDependencyNode.hasAttribute("melaninRedness") &&
           materialDependencyNode.hasAttribute("shift");
}

static bool addTextureIfPresent(const std::string &plugName, const std::string &texName, const MFnDependencyNode &ainode, std::shared_ptr<kml::Material> &mat) {

    MColor dummy;
    std::shared_ptr<kml::Texture> tex(nullptr);
    if (getTextureAndColor(ainode, MString(plugName.c_str()), tex, dummy))
    {
        if (tex)
        {
            mat->SetTexture(texName, tex);
            return true;
        }
    }

    return false;
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
    if (getTextureAndColor(ainode, MString("baseColor"), baseColorTex, baseCol))
    {
        if (baseColorTex)
        {
            mat->SetTexture("ai_baseColor", baseColorTex);
        }
    }

    mat->SetFloat("ai_baseWeight", baseWeight);
    mat->SetFloat("ai_baseColorR", baseColorR);
    mat->SetFloat("ai_baseColorG", baseColorG);
    mat->SetFloat("ai_baseColorB", baseColorB);
    mat->SetFloat("ai_diffuseRoughness", diffuseRoughness);
    mat->SetFloat("ai_metalness", metallic);

    addTextureIfPresent("base", "ai_baseWeightTex", ainode, mat);
    addTextureIfPresent("diffuseRoughness", "ai_diffuseRoughnessTex", ainode, mat);
    addTextureIfPresent("metalness", "ai_metalnessTex", ainode, mat);

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
    if (getTextureAndColor(ainode, MString("specularColor"), specularTex, specularCol))
    {
        if (specularTex)
        {
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

    addTextureIfPresent("specular", "ai_specularWeightTex", ainode, mat);
    addTextureIfPresent("specularRoughness", "ai_specularRoughnessTex", ainode, mat);
    addTextureIfPresent("specularIOR", "ai_specularIORTex", ainode, mat);
    addTextureIfPresent("specularRotation", "ai_specularRotationTex", ainode, mat);
    addTextureIfPresent("specularAnisotropy", "ai_specularAnisotropyTex", ainode, mat);


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
    const int transmissionAovs = ainode.findPlug("transmissionAovs").asInt();
    MColor transmissionCol;
    std::shared_ptr<kml::Texture> transmissionTex(nullptr);
    if (getTextureAndColor(ainode, MString("transmissionColor"), transmissionTex, transmissionCol))
    {
        if (transmissionTex)
        {
            mat->SetTexture("ai_transmissionColor", transmissionTex);
        }
    }
    MColor transmissionScatterCol;
    std::shared_ptr<kml::Texture> transmissionScatterTex(nullptr);
    if (getTextureAndColor(ainode, MString("transmissionScatter"), transmissionScatterTex, transmissionScatterCol))
    {
        if (transmissionScatterTex)
        {
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

    addTextureIfPresent("transmission", "ai_transmissionWeightTex", ainode, mat);
    addTextureIfPresent("transmissionDepth", "ai_transmissionDepthTex", ainode, mat);
    addTextureIfPresent("transmissionScatterAnisotropy", "ai_transmissionScatterAnisotropyTex", ainode, mat);
    addTextureIfPresent("transmissionExtraRoughness", "ai_transmissionExtraRoughnessTex", ainode, mat);
    addTextureIfPresent("transmissionDispersion", "ai_transmissionDispersionTex", ainode, mat);

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
    {
      MColor subsurfaceCol;
      std::shared_ptr<kml::Texture> subsurfaceTex(nullptr);
      if (getTextureAndColor(ainode, MString("subsurfaceColor"), subsurfaceTex, subsurfaceCol))
      {
          if (subsurfaceTex)
          {
              mat->SetTexture("ai_subsurfaceColor", subsurfaceTex);
          }
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

    addTextureIfPresent("subsurface", "ai_subsurfaceWeightTex", ainode, mat);
    addTextureIfPresent("subsurfaceRadius", "ai_subsurfaceRadius", ainode, mat);
    addTextureIfPresent("subsurfaceScale", "ai_subsurfaceScaleTex", ainode, mat);
    addTextureIfPresent("subsurfaceAnisotropy", "ai_subsurfaceAnisotropyTex", ainode, mat);

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
    if (getTextureAndColor(ainode, MString("coatColor"), coatColorTex, coatCol))
    {
        if (coatColorTex)
        {
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

    addTextureIfPresent("coat", "ai_coatWeightTex", ainode, mat);
    addTextureIfPresent("coatRoughness", "ai_coatRoughnessTex", ainode, mat);
    addTextureIfPresent("coatIOR", "ai_coatIORTex", ainode, mat);

    // Emissive
    const float emissionWeight = ainode.findPlug("emission").asFloat();
    const float emissionColorR = ainode.findPlug("emissionColorR").asFloat();
    const float emissionColorG = ainode.findPlug("emissionColorG").asFloat();
    const float emissionColorB = ainode.findPlug("emissionColorB").asFloat();
    MColor emissionCol;
    std::shared_ptr<kml::Texture> emissionColorTex(nullptr);
    if (getTextureAndColor(ainode, MString("emissionColor"), emissionColorTex, emissionCol))
    {
        if (emissionColorTex)
        {
            mat->SetTexture("ai_emissionColor", emissionColorTex);
        }
    }
    mat->SetFloat("ai_emissionWeight", emissionWeight);
    mat->SetFloat("ai_emissionColorR", emissionColorR);
    mat->SetFloat("ai_emissionColorG", emissionColorG);
    mat->SetFloat("ai_emissionColorB", emissionColorB);

    addTextureIfPresent("emission", "ai_emissionWeightTex", ainode, mat);

    // Opacity map
    {
        MColor opacityCol;
        std::shared_ptr<kml::Texture> opacityColorTex(nullptr);
        if (getTextureAndColor(ainode, MString("opacity"), opacityColorTex, opacityCol))
        {
            if (opacityColorTex)
            {
                mat->SetTexture("ai_opacity", opacityColorTex);
            }
        }
    }

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
    if (baseColorTex)
    {
        mat->SetTexture("BaseColor", baseColorTex);
    }
    mat->SetFloat("metallicFactor", metallic);
    mat->SetFloat("roughnessFactor", specularRoughness);

    mat->SetFloat("Emission.R", emissionCol.r * emissionWeight);
    mat->SetFloat("Emission.G", emissionCol.g * emissionWeight);
    mat->SetFloat("Emission.B", emissionCol.b * emissionWeight);
    if (emissionColorTex)
    {
        mat->SetFloat("Emission.R", emissionWeight);
        mat->SetFloat("Emission.G", emissionWeight);
        mat->SetFloat("Emission.B", emissionWeight);

        mat->SetTexture("Emission", emissionColorTex);
    }
    return true;
}

static bool storeAiStandardHairShader(std::shared_ptr<kml::Material> mat, const MFnDependencyNode& ainode)
{
    // store aishader name
    std::string shadername = ainode.name().asChar();
    mat->SetName(shadername);

    // Set hair flag.
    mat->SetInteger("aiStandardHair", 1);

    // base
    const float baseWeight = ainode.findPlug("base").asFloat();
    const float baseColorR = ainode.findPlug("baseColorR").asFloat();
    const float baseColorG = ainode.findPlug("baseColorG").asFloat();
    const float baseColorB = ainode.findPlug("baseColorB").asFloat();
    const float melanin = ainode.findPlug("melanin").asFloat();
    const float melaninRedness = ainode.findPlug("melaninRedness").asFloat();
    const float melaninRandomize = ainode.findPlug("melaninRandomize").asFloat();
    mat->SetFloat("ai_melanin", melanin);
    mat->SetFloat("ai_melaninRedness", melaninRedness);
    mat->SetFloat("ai_melaninRandomize", melaninRandomize);

    MColor baseCol;
    std::shared_ptr<kml::Texture> baseColorTex(nullptr);
    {
      if (getTextureAndColor(ainode, MString("baseColor"), baseColorTex, baseCol))
      {
          if (baseColorTex)
          {
              mat->SetTexture("ai_baseColor", baseColorTex);
          }
      }
    }
    mat->SetFloat("ai_baseWeight", baseWeight);
    mat->SetFloat("ai_baseColorR", baseColorR);
    mat->SetFloat("ai_baseColorG", baseColorG);
    mat->SetFloat("ai_baseColorB", baseColorB);

    // Specular
    const float roughness = ainode.findPlug("roughness").asFloat();
    const float ior = ainode.findPlug("ior").asFloat();
    const float shift = ainode.findPlug("shift").asFloat();
    mat->SetFloat("ai_roughness", roughness);
    mat->SetFloat("ai_ior", ior);
    mat->SetFloat("ai_shift", shift);

    // Tint
    {
      const float specularTintR = ainode.findPlug("specularTintR").asFloat();
      const float specularTintG = ainode.findPlug("specularTintG").asFloat();
      const float specularTintB = ainode.findPlug("specularTintB").asFloat();
      mat->SetFloat("ai_specularTintR", specularTintR);
      mat->SetFloat("ai_specularTintG", specularTintG);
      mat->SetFloat("ai_specularTintB", specularTintB);
    }

    {
      const float specular2TintR = ainode.findPlug("specular2TintR").asFloat();
      const float specular2TintG = ainode.findPlug("specular2TintG").asFloat();
      const float specular2TintB = ainode.findPlug("specular2TintB").asFloat();
      mat->SetFloat("ai_specular2TintR", specular2TintR);
      mat->SetFloat("ai_specular2TintG", specular2TintG);
      mat->SetFloat("ai_specular2TintB", specular2TintB);
    }

    {
      const float transmissionTintR = ainode.findPlug("transmissionTintR").asFloat();
      const float transmissionTintG = ainode.findPlug("transmissionTintG").asFloat();
      const float transmissionTintB = ainode.findPlug("transmissionTintB").asFloat();
      mat->SetFloat("ai_transmissionTintR", transmissionTintR);
      mat->SetFloat("ai_transmissionTintG", transmissionTintG);
      mat->SetFloat("ai_transmissionTintB", transmissionTintB);
    }

    // Diffuse
    const float diffuseWeight = ainode.findPlug("diffuse").asFloat();
    const float diffuseColorR = ainode.findPlug("diffuseColorR").asFloat();
    const float diffuseColorG = ainode.findPlug("diffuseColorG").asFloat();
    const float diffuseColorB = ainode.findPlug("diffuseColorB").asFloat();
    {
      MColor diffuseCol;
      std::shared_ptr<kml::Texture> diffuseColorTex(nullptr);
      if (getTextureAndColor(ainode, MString("diffuseColor"), diffuseColorTex, diffuseCol))
      {
          if (diffuseColorTex)
          {
              mat->SetTexture("ai_diffuseColor", diffuseColorTex);
          }
      }
    }

    // Emissive
    const float emissionWeight = ainode.findPlug("emission").asFloat();
    const float emissionColorR = ainode.findPlug("emissionColorR").asFloat();
    const float emissionColorG = ainode.findPlug("emissionColorG").asFloat();
    const float emissionColorB = ainode.findPlug("emissionColorB").asFloat();
    MColor emissionCol;
    std::shared_ptr<kml::Texture> emissionColorTex(nullptr);
    if (getTextureAndColor(ainode, MString("emissionColor"), emissionColorTex, emissionCol))
    {
        if (emissionColorTex)
        {
            mat->SetTexture("ai_emissionColor", emissionColorTex);
        }
    }
    mat->SetFloat("ai_emissionWeight", emissionWeight);
    mat->SetFloat("ai_emissionColorR", emissionColorR);
    mat->SetFloat("ai_emissionColorG", emissionColorG);
    mat->SetFloat("ai_emissionColorB", emissionColorB);

    // Opacity map
    {
        MColor opacityCol;
        std::shared_ptr<kml::Texture> opacityColorTex(nullptr);
        if (getTextureAndColor(ainode, MString("opacity"), opacityColorTex, opacityCol))
        {
            if (opacityColorTex)
            {
                mat->SetTexture("ai_opacity", opacityColorTex);
            }
        }
    }

    // --- store glTF standard material ---
    mat->SetFloat("BaseColor.R", baseCol.r * baseWeight);
    mat->SetFloat("BaseColor.G", baseCol.g * baseWeight);
    mat->SetFloat("BaseColor.B", baseCol.b * baseWeight);
    mat->SetFloat("BaseColor.A", 1.0f);
    if (baseColorTex)
    {
        mat->SetTexture("BaseColor", baseColorTex);
    }
    //mat->SetFloat("metallicFactor", metallic);
    mat->SetFloat("roughnessFactor", roughness);

    mat->SetFloat("Emission.R", emissionCol.r * emissionWeight);
    mat->SetFloat("Emission.G", emissionCol.g * emissionWeight);
    mat->SetFloat("Emission.B", emissionCol.b * emissionWeight);
    if (emissionColorTex)
    {
        mat->SetTexture("Emission", emissionColorTex);
    }
    return true;
}

static std::shared_ptr<kml::Material> ConvertMaterial(MObject& shaderObject)
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

        if (mpa.length() == 0)
        {
            mp = node.findPlug("surfaceShader"); // next check, shader Group
            mp.connectedTo(mpa, true, false);
        }

        const int ksz = mpa.length();
        for (int k = 0; k < ksz; k++)
        {
            if (mpa[k].node().hasFn(MFn::kLambert))
            { // All material(Lambert, phongE)
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
			
			if (mpa[k].node().hasFn(MFn::kSurfaceShader)) // This is "SurfaceShader" node
			{
                // set unlit mode
                //mat->SetExtraMode("UNLIT"); // TODO!! create unlit interface

                const MFnDependencyNode& ssnode = mpa[k].node();
                MColor col;
                float alpha = 1.0f;
                {
                    const float tR = ssnode.findPlug("outTransparencyR").asFloat();
                    const float tG = ssnode.findPlug("outTransparencyG").asFloat();
                    const float tB = ssnode.findPlug("outTransparencyB").asFloat();
                    alpha = ((1.0f - tR) + (1.0f - tG) + (1.0f - tB)) * 0.333333333f; 
                }
				std::shared_ptr<kml::Texture> coltex(nullptr);
                if (getTextureAndColor(ssnode, MString("outColor"), coltex, col))
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
                        mat->SetFloat("BaseColor.A", alpha);
                    }
                }
			}
            

            if (mpa[k].node().hasFn(MFn::kPhongExplorer))
            { // PhongE only

                // default value
                float roughness = 0.0f;
                float metallic = 0.5f;

                MFnPhongEShader pshader(mpa[k].node());
                MStatus status;
                MPlug roughnessPlug = pshader.findPlug("roughness", &status);
                if (status == MS::kSuccess)
                {
                    roughnessPlug.getValue(roughness);
                }

                mat->SetFloat("metallicFactor", metallic);
                mat->SetFloat("roughnessFactor", roughness);
            }

            if (isStingrayPBS(mpa[k].node()))
            {
                storeStingrayPBS(mat, mpa[k].node());
            }

            if (isAiStandardHairShader(mpa[k].node()))
            {
                storeAiStandardHairShader(mat, mpa[k].node());
            } else if (isAiStandardSurfaceShader(mpa[k].node()))
            {
                storeAiStandardSurfaceShader(mat, mpa[k].node());
            }

        }
    }

    return mat;
}

static std::string GetCacheTexturePath(const std::string& path)
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
    bool HasOriginalPath(const std::string& path) const
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
    std::set<std::string> GetOriginalPaths() const
    {
        std::set<std::string> s;
        for (MapType::const_iterator it = texMap_.begin(); it != texMap_.end(); it++)
        {
            s.insert(it->first);
        }
        return s;
    }
    std::set<std::string> GetCopiedPaths() const
    {
        std::set<std::string> s;
        for (MapType::const_iterator it = texMap_.begin(); it != texMap_.end(); it++)
        {
            s.insert(it->second);
        }
        return s;
    }

    const MapType& GetPathPairs() const
    {
        return texMap_;
    }

protected:
    static std::string IToS(int n)
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

typedef std::map<int, std::shared_ptr<kml::Material> > MaterialMapType;

static std::shared_ptr<kml::Node> GetLeafNode(const std::shared_ptr<kml::Node>& node)
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

static std::shared_ptr<kml::Node> GetParentNode(const std::shared_ptr<kml::Node>& root_node, const std::shared_ptr<kml::Node>& node)
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

static glm::mat4 GetRootNodeGlobalMatrix(const std::shared_ptr<kml::Node>& node, const glm::mat4& mat = glm::mat4(1.0))
{
    glm::mat4 m = mat * node->GetTransform()->GetMatrix();
    if (node->GetChildren().size() > 0)
    {
        return GetRootNodeGlobalMatrix(node->GetChildren()[0], m);
    }
    return m;
}

static bool IsVisibleLeafNode(const std::shared_ptr<kml::Node>& node)
{
    if (!node->GetVisibility())
    {
        return false;
    }
    if (node->GetChildren().size() > 0)
    {
        return IsVisibleLeafNode(node->GetChildren()[0]);
    }
    else
    {
        return node->GetVisibility();
    }
}

static MStatus WriteGLTF(
    TexturePathManager& texManager,
    std::map<int, std::shared_ptr<kml::Material> >& materials,
    std::vector<std::shared_ptr<kml::Node> >& nodes,
    const MString& dirname, const MDagPath& dagPath)
{
    MStatus status = MS::kSuccess;

    std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
    bool onefile = opts->GetInt("output_onefile") > 0;
    bool recalc_normals = opts->GetInt("recalc_normals") > 0;
    bool make_preload_texture = opts->GetInt("make_preload_texture") > 0;
    bool output_invisible_nodes = opts->GetInt("output_invisible_nodes") > 0;

    std::shared_ptr<kml::Node> root_node = CreateMeshNode(dagPath);
    if (!output_invisible_nodes)
    {
        if (!IsVisibleLeafNode(root_node))
        {
            return status;
        }
    }
    std::shared_ptr<kml::Node> node = GetLeafNode(root_node);

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
                MaterialMapType::iterator it = materials.find(shaderID);
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
                            if (tex->GetUDIMMode())
                            {
                                tex->ClearUDIM_ID();
                                for (int udimID = 1001; udimID < 1100; udimID++)
                                {
                                    std::string orgPath = tex->MakeUDIMFilePath(udimID);
                                    if (IsFileExist(orgPath))
                                    { // find UDIM files
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
                            if (make_preload_texture)
                            {
                                std::string cachePath = GetCacheTexturePath(dstPath);
                                dstTex->SetCacheFilePath(cachePath);
                            }
                            mat->SetTexture(key, dstTex);

                            // if need convert, change image extension.
                            for (size_t t = 0; t < texPath_vec.size(); ++t)
                            {
                                const std::string tmpOrgPath = texPath_vec[t];
                                std::string tempCopiedPath = MakeConvertTexturePath(texManager.GetCopiedPath(tmpOrgPath));
                                if (!texManager.HasOriginalPath(tmpOrgPath))
                                {
                                    tempCopiedPath = std::string(dirname.asChar()) + "/" + GetFileExtName(tempCopiedPath);
                                    texManager.SetPathPair(tmpOrgPath, tempCopiedPath); // register for texture copy
                                }
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
                    MaterialMapType::iterator it = materials.find(shaderID);
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
            MaterialMapType::iterator it = materials.find(shaderID);
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
    if (!recalc_normals)
    {
        if (!kml::CalculateNormalsMesh(node->GetMesh()))
        {
            return MS::kFailure;
        }
    }
    if (!kml::TriangulateMesh(node->GetMesh()))
    {
        return MS::kFailure;
    }
#ifdef REMOVE_NO_AREA_MESH
    // Notice: if you use small polygon in small scale, these will be removed.
    if (!kml::RemoveNoAreaMesh(node->GetMesh()))
    {
        return MS::kFailure;
    }
#endif
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

    std::vector<std::shared_ptr<kml::Node> > tnodes;
    {
        tnodes = kml::SplitNodeByMaterialID(node);
    }

    {
        std::vector<std::shared_ptr<kml::Node> > tnodes2;
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
                tnodes[i]->SetMesh(std::shared_ptr<kml::Mesh>()); //Clear Mesh
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

static void NodeToJson(picojson::object& item, const std::shared_ptr<kml::Node>& node)
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

static void NodesToJson(picojson::object& root, const std::vector<std::shared_ptr<kml::Node> >& nodes)
{
    root["scene"] = picojson::value((double)0);
    picojson::array scenes;
    for (size_t i = 0; i < nodes.size(); i++)
    {
        picojson::object item;
        NodeToJson(item, nodes[i]);
        scenes.push_back(picojson::value(item));
    }

    root["scenes"] = picojson::value(scenes);
}

static std::string GetFullPath(const std::string& path)
{
#ifdef _WIN32
    char buffer[_MAX_PATH + 2] = {};
    GetFullPathNameA(path.c_str(), _MAX_PATH, buffer, NULL);
    return std::string(buffer);
#else
    return path;
#endif
}

static void CopyTextureFiles(const TexturePathManager& texManager)
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

static void GetMeshNodes(std::vector<std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
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

static std::shared_ptr<kml::Bound> ExpandBound(std::shared_ptr<kml::Node>& node)
{
    if (!node->GetBound().get())
    {
        std::vector<std::shared_ptr<kml::Bound> > bounds;
        for (size_t i = 0; i < node->GetChildren().size(); i++)
        {
            auto n = node->GetChildren()[i];
            bounds.push_back(ExpandBound(n));
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

static void GetAllNodes(std::vector<std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
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

static std::vector<std::shared_ptr<kml::Node> > UniqueNodes(const std::vector<std::shared_ptr<kml::Node> >& nodes)
{
    typedef std::map<std::string, std::shared_ptr<kml::Node> > PathMapType;
    PathMapType pathMap;
    for (size_t i = 0; i < nodes.size(); i++)
    {
        std::string path = nodes[i]->GetPath();
        PathMapType::iterator it = pathMap.find(path);
        if (it == pathMap.end())
        {
            pathMap[path] = nodes[i];
        }
        else
        {
            std::shared_ptr<kml::Mesh> amesh = it->second->GetMesh();
            std::shared_ptr<kml::Mesh> bmesh = nodes[i]->GetMesh();
            if (!amesh.get() && bmesh.get())
            {
                pathMap[path] = nodes[i];
            }
        }
    }
    std::vector<std::shared_ptr<kml::Node> > ret;
    for (PathMapType::const_iterator it = pathMap.begin(); it != pathMap.end(); it++)
    {
        ret.push_back(it->second);
    }
    return ret;
}

static std::shared_ptr<kml::Node> FindChild(const std::vector<std::shared_ptr<kml::Node> >& children, const std::string& name)
{
    for (size_t i = 0; i < children.size(); i++)
    {
        if (children[i]->GetName() == name)
        {
            return children[i];
        }
    }
    return std::shared_ptr<kml::Node>();
}

static void SetParent(std::shared_ptr<kml::Node>& parent, const std::vector<std::string>& paths, const std::shared_ptr<kml::Node>& node)
{
    auto cFound = FindChild(parent->GetChildren(), paths.front());
    if (cFound.get())
    {
        if (paths.size() == 1)
        {
            //ERROR
            //MGlobal::displayInfo(MString("?ZZZZZ1:") + MString(cFound->GetName().c_str()));
            //MGlobal::displayInfo(MString("?ZZZZZ2:") + MString(paths.front().c_str()));
        }
        else
        {
            std::vector<std::string> tpaths(std::next(paths.begin()), paths.end());
            SetParent(cFound, tpaths, node);
        }
    }
    else
    {
        if (paths.size() == 1)
        {
            parent->AddChild(node);
        }
        else
        {
            //ERROR
            //MGlobal::displayInfo(MString("?YYYYY????????:") + MString(node->GetPath().c_str()));
            //for (size_t k = 0; k < paths.size(); k++)
            //{
            //    MGlobal::displayInfo(MString("?YYYYY:") + MString(paths[k].c_str()));
            //}
        }
    }
}

static std::shared_ptr<kml::Node> CombineNodes(const std::vector<std::shared_ptr<kml::Node> >& nodes)
{
    std::shared_ptr<kml::Node> node(new kml::Node());
    node->GetTransform()->SetIdentity();
    node->SetVisiblity(true);

    struct NodeStruct
    {
        std::shared_ptr<kml::Node> node;
        std::vector<std::string> paths;
    };
    struct NodeStructSorter
    {
        bool operator()(const NodeStruct& l, const NodeStruct& r) const
        {
            if (l.paths.size() != r.paths.size())
            {
                return l.paths.size() < r.paths.size();
            }
            else
            {
                size_t sz = std::min<size_t>(l.paths.size(), r.paths.size());
                for (size_t i = 0; i < sz; i++)
                {
                    std::string ls = l.paths[i];
                    std::string rs = r.paths[i];
                    if (ls != rs)
                    {
                        return std::lexicographical_compare(ls.begin(), ls.end(), rs.begin(), rs.end());
                    }
                }
            }
            return false;
        }
    };

    std::vector<NodeStruct> sortNodes;
    for (size_t i = 0; i < nodes.size(); i++)
    {
        NodeStruct s;
        auto& node = nodes[i];
        std::string path = node->GetPath();
        std::vector<std::string> paths = SplitPath(path, "|");
        if (paths.front() == "")
        {
            std::vector<std::string> tpaths(std::next(paths.begin()), paths.end());
            paths.swap(tpaths);
        }
        s.node = node;
        s.paths = paths;
        //if (node->GetName() != paths.back())
        //{
        //    MGlobal::displayInfo(MString("?KKKKK1????????:") + MString(node->GetName().c_str()));
        //    MGlobal::displayInfo(MString("?KKKKK2????????:") + MString(paths.back().c_str()));
        //}

        //IMPORTANT!!!
        node->SetName(paths.back());

        sortNodes.push_back(s);
    }
    std::sort(sortNodes.begin(), sortNodes.end(), NodeStructSorter());

    // for (size_t i = 0; i < sortNodes.size(); i++)
    //{
    //    MGlobal::displayInfo(MString("?MMMMM????????:") + MString(sortNodes[i].node->GetPath().c_str()));
    //}

    for (size_t i = 0; i < sortNodes.size(); i++)
    {
        SetParent(node, sortNodes[i].paths, sortNodes[i].node);
    }

    ExpandBound(node);
    return node;
}

static std::string GetExportDirectory(const std::string& fname)
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

static std::string GetFinalPath(const std::string& path)
{
    std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
    bool glb = opts->GetInt("output_glb") > 0;
    bool vrm = opts->GetInt("vrm_export") > 0;
    if (glb || vrm)
    {
        std::string filename = RemoveExt(path);
        if (vrm)
        {
            return filename + ".vrm";
        }
        else
        {
            return filename + ".glb";
        }
    }
    else
    {
        return RemoveExt(path); //directory
    }
}

static MString GetResourceString(const MString& strResource)
{
    MString command = "uiRes";
    command += MString(" \"") + strResource + MString("\"");
    command += ";";
    return MGlobal::executeCommandStringResult(command);
}

static MString ReplaceSpecialCharacter(const MString& str)
{
    const char* c = str.asChar();
    std::string ret;
    for (; *c; c++)
    {
        if (*c == '\n')
        {
            ret += "\\n";
        }
        else if (*c == '\r')
        {
            ret += "\\r";
        }
        else
        {
            ret += *c;
        }
    }
    return MString(ret.c_str());
}

static bool CheckGLTFDirectoryAlreadyExists(const std::string& path)
{
    if (MGlobal::mayaState() == MGlobal::kInteractive)
    {
        std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
        bool glb = opts->GetInt("output_glb") > 0;
        bool vrm = opts->GetInt("vrm_export") > 0;
        if (!(glb || vrm))
        {
            std::string dirPath = RemoveExt(path);
            if (IsFileExist(dirPath))
            {
                /*
                * confirmDialog -title "Confirm" -message "Are you sure?"
                * -button "Yes" -button "No" -defaultButton "Yes"
                * -cancelButton "No" -dismissString "No";
                */
                static const char* kYes = "s_TdialogStrings.rYes";
                static const char* kNo = "s_TdialogStrings.rNo";
                static const char* kCouldNotSaveFileMsg = "s_TfileBrowserDialogStrings.rReplaceFileConfirm";

                //MStatus status;
                static MString strYes = GetResourceString(kYes);
                static MString strNo = GetResourceString(kNo);
                static MString strFmt = GetResourceString(kCouldNotSaveFileMsg);

                std::string filePath = GetFileExtName(dirPath);

                MString strMsg;
                strMsg.format(strFmt, MString(filePath.c_str()));
                strMsg = ReplaceSpecialCharacter(strMsg);

                //MGlobal::displayInfo(strYes);
                //MGlobal::displayInfo(strNo);
                //MGlobal::displayInfo(strFmt);
                //MGlobal::displayInfo(strMsg);

                MString command = "confirmDialog";
                command += " -title \"\"";
                command += MString(" -message \"") + strMsg + MString("\"");
                command += MString(" -button \"") + strYes + MString("\"");
                command += MString(" -button \"") + strNo + MString("\"");
                command += MString(" -defaultButton \"") + strYes + MString("\"");
                command += MString(" -cancelButton \"") + strNo + MString("\"");
                command += MString(" -dismissString \"") + strNo + MString("\"");
                command += ";";
                MString result = MGlobal::executeCommandStringResult(command);
                if (result == strNo)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

MStatus glTFExporter::exportSelected(const MString& fname)
{
    MStatus status = MS::kSuccess;

    if (!CheckGLTFDirectoryAlreadyExists(fname.asChar()))
    {
        return MS::kFailure;
    }

    // Create an iterator for the active selection list
    //
    MSelectionList slist;
    MGlobal::getActiveSelectionList(slist);
    MItSelectionList iter(slist);

    if (iter.isDone())
    {
        fprintf(stderr, "Error: Nothing is selected.\n");
        return MS::kFailure;
    }

    // We will need to interate over a selected node's heirarchy
    // in the case where shapes are grouped, and the group is selected.
    MItDag dagIterator(MItDag::kDepthFirst, MFn::kInvalid, &status);

    if (MS::kSuccess != status)
    {
        fprintf(stderr, "Failure in DAG iterator setup.\n");
        return MS::kFailure;
    }

    std::vector<MDagPath> dagPaths;

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

MStatus glTFExporter::exportAll(const MString& fname)
{
    MStatus status = MS::kSuccess;

    if (!CheckGLTFDirectoryAlreadyExists(fname.asChar()))
    {
        return MS::kFailure;
    }

    MItDag dagIterator(MItDag::kBreadthFirst, MFn::kInvalid, &status);

    if (MS::kSuccess != status)
    {
        fprintf(stderr, "Failure in DAG iterator setup.\n");
        return MS::kFailure;
    }

    std::vector<MDagPath> dagPaths;

    for (; !dagIterator.isDone(); dagIterator.next())
    {
        MDagPath dagPath;
        MObject component = MObject::kNullObj;
        status = dagIterator.getPath(dagPath);

        if (!status)
        {
            fprintf(stderr, "Failure getting DAG path.\n");
            return MS::kFailure;
        }

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

    return exportProcess(fname, dagPaths);
}

static std::vector<std::shared_ptr<kml::Node> > GetJointNodes(const std::vector<std::shared_ptr<kml::Node> >& lnodes)
{
    std::vector<std::shared_ptr<kml::Node> > mesh_nodes;
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
            if (mesh->skin_weight.get())
            {
                auto& skin_weight = mesh->skin_weight;
                for (size_t j = 0; j < skin_weight->joint_paths.size(); j++)
                {
                    joint_paths.push_back(skin_weight->joint_paths[j]);
                }
            }
        }
    }

    std::sort(joint_paths.begin(), joint_paths.end());
    joint_paths.erase(std::unique(joint_paths.begin(), joint_paths.end()), joint_paths.end());

    std::vector<std::shared_ptr<kml::Node> > tnodes;
    for (size_t j = 0; j < joint_paths.size(); j++)
    {
        std::vector<MDagPath> dagPathList = GetDagPathList(joint_paths[j]);
        std::vector<std::shared_ptr<kml::Node> > nodes;
        for (size_t i = 0; i < dagPathList.size(); i++)
        {
            MDagPath path = dagPathList[i];
            std::shared_ptr<kml::Node> n(new kml::Node());
            n->SetName(path.partialPathName().asChar());
            n->SetPath(path.fullPathName().asChar());
            n->SetVisiblity(IsVisible(path));

            SetTRS(path, n);

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

static void GetTransformNodes(std::vector<std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
{
    if (node->GetTransform().get())
    {
        std::string path = node->GetPath();
        if (!path.empty() && path[0] == '|')
        {
            nodes.push_back(node);
        }
    }
    if (node->GetChildren().size() > 0)
    {
        auto& children = node->GetChildren();
        for (size_t i = 0; i < children.size(); i++)
        {
            GetTransformNodes(nodes, children[i]);
        }
    }
}

static void GetMorphNodes(std::vector<std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
{
    if (node->GetTransform().get() && node->GetMesh().get())
    {
        const auto& mesh = node->GetMesh();
        if (mesh->morph_targets.get())
        {
            std::string path = node->GetPath();
            if (!path.empty() && path[0] == '|')
            {
                nodes.push_back(node);
            }
        }
    }
    if (node->GetChildren().size() > 0)
    {
        auto& children = node->GetChildren();
        for (size_t i = 0; i < children.size(); i++)
        {
            GetTransformNodes(nodes, children[i]);
        }
    }
}

static double ToDegree(double v)
{
    return 180.0 * v / M_PI;
}

static std::shared_ptr<kml::Skin> FindSkin(const std::vector<std::shared_ptr<kml::Skin> >& skins, const std::shared_ptr<kml::Node>& node)
{
    for (size_t i = 0; i < skins.size(); i++)
    {
        const auto& skin = skins[i];
        const auto& joints = skin->GetJoints();
        for (size_t j = 0; j < joints.size(); j++)
        {
            if (joints[j].get() == node.get())
            {
                return skin;
            }
        }
    }
    return std::shared_ptr<kml::Skin>();
}

static void GetAnimationsFromTransform(std::vector<std::shared_ptr<kml::Animation> >& animations, const std::shared_ptr<kml::Node>& node)
{
    struct Category
    {
        std::string name;
        std::vector<std::shared_ptr<kml::Node> > nodes;
    };

    std::vector<std::shared_ptr<kml::Node> > nodes;
    GetTransformNodes(nodes, node);
    typedef std::map<void*, Category> CategoryMapType;
    CategoryMapType categoryMap;
    {
        const auto& skins = node->GetSkins();
        for (size_t i = 0; i < nodes.size(); i++)
        {
            const auto& n = nodes[i];
            auto skin = FindSkin(skins, n);
            if (skin.get())
            {
                auto& category = categoryMap[(void*)skin.get()];
                category.nodes.push_back(n);
                category.name = "Skin:" + skin->GetName();
            }
            else
            {
                auto& category = categoryMap[(void*)n.get()];
                category.nodes.push_back(n);
                category.name = "Transform:" + n->GetName();
            }
        }
    }

    for (CategoryMapType::iterator git = categoryMap.begin(); git != categoryMap.end(); git++)
    {
        typedef std::map<std::string, std::vector<std::shared_ptr<kml::Node> > > MapType;
        MapType pathNodeMap;
        std::vector<MDagPath> pathList;
        {
            MSelectionList selectionList;
            const auto& gnodes = git->second.nodes;
            for (size_t i = 0; i < gnodes.size(); i++)
            {
                std::string path = gnodes[i]->GetOriginalPath();
                pathNodeMap[path].push_back(gnodes[i]);
            }
            for (MapType::const_iterator it = pathNodeMap.begin(); it != pathNodeMap.end(); it++)
            {
                std::string path = it->first;
                selectionList.add(MString(path.c_str()));
            }
            MItSelectionList iterator = MItSelectionList(selectionList, MFn::kDagNode);
            while (!iterator.isDone())
            {
                MDagPath dagPath;
                iterator.getDagPath(dagPath);
                pathList.push_back(dagPath);
                iterator.next();
            }
        }

        std::shared_ptr<kml::Animation> animation(new kml::Animation());
        animation->SetName(git->second.name);

        for (size_t idx = 0; idx < pathList.size(); idx++)
        {
            MPlugArray animatedPlugs;
            MAnimUtil::findAnimatedPlugs(pathList[idx], animatedPlugs);
            unsigned int numPlugs = animatedPlugs.length();
            if (numPlugs <= 0)
            {
                continue;
            }

            MFnTransform fnTransform(pathList[idx]);
            MFnIkJoint fnJoint(pathList[idx]);
            MVector tt = fnTransform.getTranslation(MSpace::kTransform);

            MEulerRotation rr;
            fnTransform.getRotation(rr);

            double ss[3] = {1.0, 1.0, 1.0};
            fnTransform.getScale(ss);

            MQuaternion mOR, mJO;

            mOR = fnTransform.rotateOrientation(MSpace::kTransform);

            MStatus joret = fnJoint.getOrientation(mJO);
            if (joret != MS::kSuccess)
            {
                mJO.w = 1.0;
                mJO.x = 0.0;
                mJO.y = 0.0;
                mJO.z = 0.0;
            }

            std::vector<double> translationKeys;
            std::vector<MPlug> translationPlugs;
            std::vector<double> rotationKeys;
            std::vector<MPlug> rotationPlugs;
            std::vector<double> scaleKeys;
            std::vector<MPlug> scalePlugs;

            for (int j = 0; j < numPlugs; ++j)
            {
                MPlug plug = animatedPlugs[j];
                MObjectArray animation;
                if (!MAnimUtil::findAnimation(plug, animation))
                {
                    continue;
                }

                std::string name = plug.name().asChar();
                std::string typeName = name.substr(name.find(".") + 1);

                int keyType = -1;
                if (typeName == "translateX" || typeName == "translateY" || typeName == "translateZ")
                {
                    keyType = 0;
                    translationPlugs.push_back(plug);
                }
                else if (typeName == "rotateX" || typeName == "rotateY" || typeName == "rotateZ")
                {
                    keyType = 1;
                    rotationPlugs.push_back(plug);
                }
                else if (typeName == "scaleX" || typeName == "scaleY" || typeName == "scaleZ")
                {
                    keyType = 2;
                    scalePlugs.push_back(plug);
                }

                const unsigned int numCurves = animation.length();
                for (unsigned int k = 0; k < numCurves; k++)
                {
                    MObject animCurveNode = animation[k];
                    if (!animCurveNode.hasFn(MFn::kAnimCurve))
                    {
                        continue;
                    }

                    MFnAnimCurve animCurve(animCurveNode);
                    unsigned int numKeys = animCurve.numKeyframes();

                    for (unsigned int currKey = 0; currKey < numKeys; currKey++)
                    {
                        MTime keyTime = animCurve.time(currKey);
                        //double value = animCurve.evaluate(keyTime);
                        double second = keyTime.asUnits(MTime::kSeconds);
                        if (keyType == 0)
                        {
                            translationKeys.push_back(second);
                        }
                        else if (keyType == 1)
                        {
                            rotationKeys.push_back(second);
                        }
                        else if (keyType == 2)
                        {
                            scaleKeys.push_back(second);
                        }
                    }
                }
            }

            std::shared_ptr<kml::AnimationInstruction> instruction(new kml::AnimationInstruction());
            instruction->SetTargets(pathNodeMap[pathList[idx].fullPathName().asChar()]);

            if (!translationPlugs.empty())
            {
                std::sort(translationKeys.begin(), translationKeys.end());
                translationKeys.erase(std::unique(translationKeys.begin(), translationKeys.end()), translationKeys.end());

                std::vector<glm::vec3> values(translationKeys.size());
                for (size_t j = 0; j < translationKeys.size(); j++)
                {
                    values[j] = glm::vec3(tt.x, tt.y, tt.z);
                }

                for (int j = 0; j < translationPlugs.size(); j++)
                {
                    MPlug plug = translationPlugs[j];
                    MObjectArray animation;
                    if (!MAnimUtil::findAnimation(plug, animation))
                    {
                        continue;
                    }

                    std::string name = plug.name().asChar();
                    std::string typeName = name.substr(name.find(".") + 1);

                    unsigned int numCurves = animation.length();
                    for (unsigned int c = 0; c < numCurves; c++)
                    {
                        MObject animCurveNode = animation[c];
                        if (!animCurveNode.hasFn(MFn::kAnimCurve))
                            continue;
                        MFnAnimCurve animCurve(animCurveNode);

                        for (size_t k = 0; k < translationKeys.size(); k++)
                        {
                            double second = translationKeys[k];
                            double value = animCurve.evaluate(MTime(second, MTime::kSeconds));

                            if (typeName == "translateX")
                            {
                                values[k].x = value;
                            }
                            else if (typeName == "translateY")
                            {
                                values[k].y = value;
                            }
                            else if (typeName == "translateZ")
                            {
                                values[k].z = value;
                            }
                        }
                    }
                }

                std::shared_ptr<kml::AnimationPath> apath(new kml::AnimationPath());
                apath->SetPathType("translation");

                std::shared_ptr<kml::AnimationCurve> curves[4];
                for (int j = 0; j < 4; j++)
                {
                    curves[j] = std::shared_ptr<kml::AnimationCurve>(new kml::AnimationCurve());
                    curves[j]->SetInterpolationType(kml::AnimationInterporationType::LINEAR);
                }
                for (size_t k = 0; k < translationKeys.size(); k++)
                {
                    curves[0]->GetValues().push_back(translationKeys[k]);
                    curves[1]->GetValues().push_back(values[k].x);
                    curves[2]->GetValues().push_back(values[k].y);
                    curves[3]->GetValues().push_back(values[k].z);
                }
                apath->SetCurve("k", curves[0]);
                apath->SetCurve("x", curves[1]);
                apath->SetCurve("y", curves[2]);
                apath->SetCurve("z", curves[3]);

                instruction->AddPath(apath);
            }

            if (!rotationPlugs.empty())
            {
                std::sort(rotationKeys.begin(), rotationKeys.end());
                rotationKeys.erase(std::unique(rotationKeys.begin(), rotationKeys.end()), rotationKeys.end());

                std::vector<glm::vec3> values(rotationKeys.size());
                for (size_t j = 0; j < rotationKeys.size(); j++)
                {
                    values[j] = glm::vec3(rr.x, rr.y, rr.z);
                }
#ifndef NDEBUG
                std::vector<glm::vec3> angles(rotationKeys.size());
                for (size_t j = 0; j < rotationKeys.size(); j++)
                {
                    angles[j] = glm::vec3(ToDegree(rr.x), ToDegree(rr.y), ToDegree(rr.z));
                }
#endif

                for (int j = 0; j < rotationPlugs.size(); j++)
                {
                    MPlug plug = rotationPlugs[j];
                    MObjectArray animation;
                    if (!MAnimUtil::findAnimation(plug, animation))
                    {
                        continue;
                    }

                    std::string name = plug.name().asChar();
                    std::string typeName = name.substr(name.find(".") + 1);

                    unsigned int numCurves = animation.length();
                    for (unsigned int c = 0; c < numCurves; c++)
                    {
                        MObject animCurveNode = animation[c];
                        if (!animCurveNode.hasFn(MFn::kAnimCurve))
                            continue;
                        MFnAnimCurve animCurve(animCurveNode);

                        for (size_t k = 0; k < rotationKeys.size(); k++)
                        {
                            double second = rotationKeys[k];
                            double value = animCurve.evaluate(MTime(second, MTime::kSeconds));

#ifndef NDEBUG
                            double angle = ToDegree(value);
                            if (typeName == "rotateX")
                            {
                                angles[k].x = angle;
                            }
                            else if (typeName == "rotateY")
                            {
                                angles[k].y = angle;
                            }
                            else if (typeName == "rotateZ")
                            {
                                angles[k].z = angle;
                            }
#endif

                            if (typeName == "rotateX")
                            {
                                values[k].x = value;
                            }
                            else if (typeName == "rotateY")
                            {
                                values[k].y = value;
                            }
                            else if (typeName == "rotateZ")
                            {
                                values[k].z = value;
                            }
                        }
                    }
                }

                std::vector<glm::quat> values2(rotationKeys.size());
#ifndef NDEBUG
                std::vector<double> angle2(rotationKeys.size());
#endif

                for (size_t k = 0; k < values2.size(); k++)
                {
                    double trot[3] = {values[k].x, values[k].y, values[k].z};
                    MTransformationMatrix transform;
                    transform.setRotation(trot, fnTransform.rotationOrder());
                    MQuaternion mR = transform.rotation();
                    MQuaternion q = mOR * mR * mJO;
                    values2[k] = glm::quat(q.w, q.x, q.y, q.z); //wxyz
#ifndef NDEBUG
                    angle2[k] = ToDegree(2.0 * acos(q.w));
#endif
                }

                std::shared_ptr<kml::AnimationPath> apath(new kml::AnimationPath());
                apath->SetPathType("rotation");

                std::shared_ptr<kml::AnimationCurve> curves[5];
                for (int j = 0; j < 5; j++)
                {
                    curves[j] = std::shared_ptr<kml::AnimationCurve>(new kml::AnimationCurve());
                    curves[j]->SetInterpolationType(kml::AnimationInterporationType::LINEAR);
                }
                for (size_t k = 0; k < rotationKeys.size(); k++)
                {
                    curves[0]->GetValues().push_back(rotationKeys[k]);
                    curves[1]->GetValues().push_back(values2[k].x);
                    curves[2]->GetValues().push_back(values2[k].y);
                    curves[3]->GetValues().push_back(values2[k].z);
                    curves[4]->GetValues().push_back(values2[k].w);
                }
                apath->SetCurve("k", curves[0]);
                apath->SetCurve("x", curves[1]);
                apath->SetCurve("y", curves[2]);
                apath->SetCurve("z", curves[3]);
                apath->SetCurve("w", curves[4]);

                instruction->AddPath(apath);
            }

            if (!scalePlugs.empty())
            {
                std::sort(scaleKeys.begin(), scaleKeys.end());
                scaleKeys.erase(std::unique(scaleKeys.begin(), scaleKeys.end()), scaleKeys.end());

                std::vector<glm::vec3> values(scaleKeys.size());
                for (size_t j = 0; j < scaleKeys.size(); j++)
                {
                    values[j] = glm::vec3(ss[0], ss[1], ss[2]);
                }

                for (int j = 0; j < scalePlugs.size(); j++)
                {
                    MPlug plug = scalePlugs[j];
                    MObjectArray animation;
                    if (!MAnimUtil::findAnimation(plug, animation))
                    {
                        continue;
                    }

                    std::string name = plug.name().asChar();
                    std::string typeName = name.substr(name.find(".") + 1);

                    unsigned int numCurves = animation.length();
                    for (unsigned int c = 0; c < numCurves; c++)
                    {
                        MObject animCurveNode = animation[c];
                        if (!animCurveNode.hasFn(MFn::kAnimCurve))
                            continue;
                        MFnAnimCurve animCurve(animCurveNode);

                        for (size_t k = 0; k < scaleKeys.size(); k++)
                        {
                            double second = scaleKeys[k];
                            double value = animCurve.evaluate(MTime(second, MTime::kSeconds));

                            if (typeName == "scaleX")
                            {
                                values[k].x = value;
                            }
                            else if (typeName == "scaleY")
                            {
                                values[k].y = value;
                            }
                            else if (typeName == "scaleZ")
                            {
                                values[k].z = value;
                            }
                        }
                    }
                }

                std::shared_ptr<kml::AnimationPath> apath(new kml::AnimationPath());
                apath->SetPathType("scale");

                std::shared_ptr<kml::AnimationCurve> curves[4];
                for (int j = 0; j < 4; j++)
                {
                    curves[j] = std::shared_ptr<kml::AnimationCurve>(new kml::AnimationCurve());
                    curves[j]->SetInterpolationType(kml::AnimationInterporationType::LINEAR);
                }
                for (size_t k = 0; k < scaleKeys.size(); k++)
                {
                    curves[0]->GetValues().push_back(scaleKeys[k]);
                    curves[1]->GetValues().push_back(values[k].x);
                    curves[2]->GetValues().push_back(values[k].y);
                    curves[3]->GetValues().push_back(values[k].z);
                }
                apath->SetCurve("k", curves[0]);
                apath->SetCurve("x", curves[1]);
                apath->SetCurve("y", curves[2]);
                apath->SetCurve("z", curves[3]);

                instruction->AddPath(apath);
            }

            if (!instruction->GetPaths().empty())
            {
                animation->AddInstruction(instruction);
            }
        }
        //end of pathList

        if (!animation->GetInstructions().empty())
        {
            animations.push_back(animation);
        }
    }
}

static void GetAnimationsFromMorph(std::vector<std::shared_ptr<kml::Animation> >& animations, const std::shared_ptr<kml::Node>& node)
{
    std::vector<std::shared_ptr<kml::Node> > nodes;
    GetMorphNodes(nodes, node);
    typedef std::map<std::string, std::vector<std::shared_ptr<kml::Node> > > MapType;
    MapType pathNodeMap;
    std::vector<MDagPath> pathList;
    {
        MSelectionList selectionList;
        for (size_t i = 0; i < nodes.size(); i++)
        {
            std::string path = nodes[i]->GetOriginalPath();
            pathNodeMap[path].push_back(nodes[i]);
        }
        for (MapType::const_iterator it = pathNodeMap.begin(); it != pathNodeMap.end(); it++)
        {
            std::string path = it->first;
            selectionList.add(MString(path.c_str()));
        }
        MItSelectionList iterator = MItSelectionList(selectionList, MFn::kDagNode);
        while (!iterator.isDone())
        {
            MDagPath dagPath;
            iterator.getDagPath(dagPath);
            pathList.push_back(dagPath);
            iterator.next();
        }
    }

    {
        for (size_t idx = 0; idx < pathList.size(); idx++)
        {
            std::string name = std::string("Morph:") + std::string(pathList[idx].partialPathName().asChar());
            std::shared_ptr<kml::Animation> animation(new kml::Animation());
            animation->SetName(name);

            MDagPath dagPath = pathList[idx];
            dagPath.extendToShape();
            MFnDependencyNode node(dagPath.node());

            MPlugArray mpa;
            MPlug mp = node.findPlug("inMesh");
            mp.connectedTo(mpa, true, false);

            MObject vtxobj = MObject::kNullObj;
            int isz = mpa.length();
            if (isz < 1)
            {
                continue;
            }

            MFnDependencyNode dnode(mpa[0].node());
            if (dnode.typeName() != "blendShape")
            {
                continue;
            }

            MStatus status;
            MFnBlendShapeDeformer fnBlendShapeDeformer(mpa[0].node(), &status);
            if (status != MStatus::kSuccess)
            {
                continue;
            }

            MPlug weightArrayPlug = fnBlendShapeDeformer.findPlug("weight", &status);
            if (status != MStatus::kSuccess)
            {
                continue;
            }

            unsigned int numWeights = weightArrayPlug.evaluateNumElements(&status);
            if (status != MStatus::kSuccess)
            {
                continue;
            }

            std::shared_ptr<kml::AnimationInstruction> instruction(new kml::AnimationInstruction());
            instruction->SetTargets(pathNodeMap[pathList[idx].fullPathName().asChar()]);

            std::vector<float> keys;
            for (int j = 0; j < numWeights; j++)
            {
                MPlug plug = weightArrayPlug.elementByLogicalIndex(j, &status);
                if (status != MStatus::kSuccess)
                {
                    continue;
                }

                MObjectArray animation;
                if (!MAnimUtil::findAnimation(plug, animation))
                {
                    continue;
                }

                //#ifndef NDEBUG
                if (0)
                {
                    std::string name = plug.name().asChar();
                    std::string typeName = name.substr(name.find(".") + 1); //
                    std::string info1 = std::string("plug.name:") + name;
                    std::string info2 = std::string("plug.name:") + typeName;
                    MGlobal::displayInfo(MString(info1.c_str()));
                    MGlobal::displayInfo(MString(info2.c_str()));
                }
                //#endif

                unsigned int numCurves = animation.length();
                for (unsigned int c = 0; c < numCurves; c++)
                {
                    MObject animCurveNode = animation[c];
                    if (!animCurveNode.hasFn(MFn::kAnimCurve))
                        continue;
                    MFnAnimCurve animCurve(animCurveNode);
                    unsigned int numKeys = animCurve.numKeyframes();
                    for (unsigned int currKey = 0; currKey < numKeys; currKey++)
                    {
                        MTime keyTime = animCurve.time(currKey);
                        //double value = animCurve.evaluate(keyTime);
                        double second = keyTime.asUnits(MTime::kSeconds);
                        keys.push_back(second);
                    }
                }
            }

            std::sort(keys.begin(), keys.end());
            keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

            int total_floats = keys.size() * numWeights;
            if (total_floats == 0)
            {
                continue;
            }

            std::vector<float> values(total_floats);
            for (int j = 0; j < numWeights; j++)
            {
                MPlug plug = weightArrayPlug.elementByLogicalIndex(j, &status);
                if (status != MStatus::kSuccess)
                {
                    continue;
                }

                MObjectArray animation;
                if (!MAnimUtil::findAnimation(plug, animation))
                {
                    continue;
                }

                unsigned int numCurves = animation.length();
                for (unsigned int c = 0; c < numCurves; c++)
                {
                    MObject animCurveNode = animation[c];
                    if (!animCurveNode.hasFn(MFn::kAnimCurve))
                        continue;
                    MFnAnimCurve animCurve(animCurveNode);
                    for (unsigned int k = 0; k < keys.size(); k++)
                    {
                        double second = keys[k];
                        double value = animCurve.evaluate(MTime(second, MTime::kSeconds));
                        values[numWeights * k + j] = value;
                    }
                }
            }

            std::shared_ptr<kml::AnimationPath> path(new kml::AnimationPath());
            path->SetPathType("weights");

            std::shared_ptr<kml::AnimationCurve> curves[2];
            for (int j = 0; j < 2; j++)
            {
                curves[j] = std::shared_ptr<kml::AnimationCurve>(new kml::AnimationCurve());
                curves[j]->SetInterpolationType(kml::AnimationInterporationType::LINEAR);
            }
            for (size_t k = 0; k < keys.size(); k++)
            {
                curves[0]->GetValues().push_back(keys[k]);
            }
            for (size_t k = 0; k < values.size(); k++)
            {
                curves[1]->GetValues().push_back(values[k]);
            }
            path->SetCurve("k", curves[0]);
            path->SetCurve("w", curves[1]);

            instruction->AddPath(path);

            if (!instruction->GetPaths().empty())
            {
                animation->AddInstruction(instruction);
            }

            if (!animation->GetInstructions().empty())
            {
                animations.push_back(animation);
            }
        }
    }
}

static std::vector<std::shared_ptr<kml::Animation> > GetAnimations(const std::shared_ptr<kml::Node>& node)
{
    std::vector<std::shared_ptr<kml::Animation> > animations;
    GetAnimationsFromTransform(animations, node);
    GetAnimationsFromMorph(animations, node);
    return animations;
}

static std::vector<std::shared_ptr<kml::Animation> > MergeSingleAnimation(std::vector<std::shared_ptr<kml::Animation> >& animations)
{
    std::vector<std::shared_ptr<kml::Animation> > tanims;
    if(animations.size() > 0)
    {
        std::shared_ptr<kml::Animation> tanim(new kml::Animation());
        tanim->SetName(animations[0]->GetName());
        for(size_t i = 0; i < animations.size(); i++)
        {
            const std::vector<std::shared_ptr<kml::AnimationInstruction> >& insts = animations[i]->GetInstructions();
            for(size_t j = 0; j < insts.size(); j++)
            {
                tanim->AddInstruction(insts[j]);
            }
        }
        tanims.push_back(tanim);
    }
    return tanims;
}

static void GetSkinnedMeshNodes(std::vector<std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
{
    if (node->GetTransform().get() && node->GetMesh().get() && node->GetMesh()->GetSkinWeight().get())
    {
        nodes.push_back(node);
    }
    if (node->GetChildren().size() > 0)
    {
        auto& children = node->GetChildren();
        for (size_t i = 0; i < children.size(); i++)
        {
            GetSkinnedMeshNodes(nodes, children[i]);
        }
    }
}

static void FlattenNodes(std::vector<std::shared_ptr<kml::Node> >& nodes, const std::shared_ptr<kml::Node>& node)
{
    nodes.push_back(node);
    const auto& children = node->GetChildren();
    for (size_t i = 0; i < children.size(); i++)
    {
        FlattenNodes(nodes, children[i]);
    }
}

static std::vector<std::shared_ptr<kml::Skin> > GetSkins(const std::shared_ptr<kml::Node>& node)
{
    std::vector<std::shared_ptr<kml::Skin> > skins;

    std::vector<std::shared_ptr<kml::Node> > jointNodes;
    GetTransformNodes(jointNodes, node);
    typedef std::map<std::string, std::shared_ptr<kml::Node> > JointMapType;
    JointMapType jointMap;
    for (size_t i = 0; i < jointNodes.size(); i++)
    {
        jointMap[jointNodes[i]->GetPath()] = jointNodes[i];
    }

    std::vector<std::shared_ptr<kml::Node> > skinNodes;
    GetSkinnedMeshNodes(skinNodes, node);
    typedef std::map<std::string, std::vector<std::shared_ptr<kml::SkinWeight> > > SkinMapType;
    SkinMapType skinMap;

    typedef std::map<std::string, glm::mat4> MatMapType;
    MatMapType matMap;
    for (size_t i = 0; i < skinNodes.size(); i++)
    {
        const auto& skinWeight = skinNodes[i]->GetMesh()->GetSkinWeight();
        const auto& jointPaths = skinWeight->GetJointPaths();
        const auto& jointMats = skinWeight->GetJointBindMatrices();
        for (size_t j = 0; j < jointPaths.size(); j++)
        {
            skinMap[jointPaths[j]].push_back(skinWeight);
            matMap[jointPaths[j]] = jointMats[j];
        }
    }

    std::vector<std::shared_ptr<kml::Node> > rootJointNodes;
    {
        typedef std::set<std::shared_ptr<kml::Node> > JointSetType;
        JointSetType jointSet;
        for (SkinMapType::iterator it = skinMap.begin(); it != skinMap.end(); it++)
        {
            std::string jointName = it->first;
            JointMapType::const_iterator jit = jointMap.find(jointName);
            if (jit != jointMap.end() && jit->second.get())
            {
                jointSet.insert(jit->second);
            }
        }
        JointSetType jointSet2 = jointSet;
        for (JointSetType::iterator it = jointSet2.begin(); it != jointSet2.end(); it++)
        {
            auto n = *it;
            const auto& children = n->GetChildren();
            for (size_t j = 0; j < children.size(); j++)
            {
                JointSetType::iterator itf = jointSet.find(children[j]);
                if (itf != jointSet.end())
                {
                    jointSet.erase(itf);
                }
            }
        }
        for (JointSetType::iterator it = jointSet.begin(); it != jointSet.end(); it++)
        {
            rootJointNodes.push_back(*it);
        }
    }

    for (size_t i = 0; i < rootJointNodes.size(); i++)
    {
        std::shared_ptr<kml::Skin> skin(new kml::Skin());
        std::vector<std::shared_ptr<kml::Node> > joints;
        FlattenNodes(joints, rootJointNodes[i]);
        std::vector<glm::mat4> mats;
        for (size_t j = 0; j < joints.size(); j++)
        {
            mats.push_back(matMap[joints[j]->GetPath()]);
        }
        {
            typedef std::set<std::shared_ptr<kml::SkinWeight> > WeightSetType;
            WeightSetType weightSet;
            for (size_t j = 0; j < joints.size(); j++)
            {
                const auto& weights = skinMap[joints[j]->GetPath()];
                for (size_t k = 0; k < weights.size(); k++)
                {
                    weightSet.insert(weights[k]);
                }
            }
            for (WeightSetType::iterator it = weightSet.begin(); it != weightSet.end(); it++)
            {
                skin->AddSkinWeight(*it);
            }
        }
        skin->SetJoints(joints);
        skin->SetJointBindMatrices(mats);
        skin->SetName(joints[0]->GetName());

        skins.push_back(skin);
    }

    return skins;
}

static bool FreezeSkinnedMeshTransform(const std::shared_ptr<kml::Node>& node, const glm::mat4& gmat = glm::mat4(1.0f))
{
    auto& mesh = node->GetMesh();
    bool bRet = false;
    glm::mat4 mat = gmat * node->GetTransform()->GetMatrix();
    if (mesh.get() && mesh->skin_weight.get())
    {
        mesh = TransformMesh(mesh, mat);
        bRet |= true;
    }
    auto& children = node->GetChildren();
    if (!children.empty())
    {
        for (size_t i = 0; i < children.size(); i++)
        {
            bRet |= FreezeSkinnedMeshTransform(children[i], mat);
        }
    }
    if (bRet)
    {
        node->GetTransform()->SetIdentity();
    }
    return bRet;
}

static bool FreezeSkinnedJointTransform_(const std::set<std::shared_ptr<kml::Node> >& jointsSet, const std::shared_ptr<kml::Node>& node, const glm::mat4& gmat = glm::mat4(1.0f))
{
    bool bRet = false;
    glm::mat4 mat = gmat * node->GetTransform()->GetMatrix();
    auto& children = node->GetChildren();
    if (!children.empty())
    {
        for (size_t i = 0; i < children.size(); i++)
        {
            bRet |= FreezeSkinnedJointTransform_(jointsSet, children[i], mat);
        }
    }
    //the node is joint
    if (jointsSet.find(node) != jointsSet.end())
    {
        glm::vec3 T(mat[3][0] - gmat[3][0], mat[3][1] - gmat[3][1], mat[3][2] - gmat[3][2]);
        glm::quat R(1, 0, 0, 0);
        glm::vec3 S(1, 1, 1);
        node->GetTransform()->SetIdentity();
        node->GetTransform()->SetTRS(T, R, S);
        bRet |= true;
    }
    else
    {
        if (bRet)
        {
            glm::vec3 T(mat[3][0] - gmat[3][0], mat[3][1] - gmat[3][1], mat[3][2] - gmat[3][2]);
            glm::quat R(1, 0, 0, 0);
            glm::vec3 S(1, 1, 1);
            node->GetTransform()->SetIdentity();
            node->GetTransform()->SetTRS(T, R, S);
        }
    }
    return bRet;
}

static void GetGlobalMatrices(std::vector<glm::mat4>& mats, const std::shared_ptr<kml::Node>& node, const glm::mat4& gmat)
{
    glm::mat4 mat = gmat * node->GetTransform()->GetMatrix();
    mats.push_back(mat);
    auto& children = node->GetChildren();
    if (!children.empty())
    {
        for (size_t i = 0; i < children.size(); i++)
        {
            GetGlobalMatrices(mats, children[i], mat);
        }
    }
}

static bool FreezeSkinnedJointTransform(const std::shared_ptr<kml::Node>& node)
{
    std::set<std::shared_ptr<kml::Node> > jointsSet;
    auto& skins = node->GetSkins();
    for (size_t i = 0; i < skins.size(); i++)
    {
        const auto& joints = skins[i]->GetJoints();
        std::copy(joints.begin(), joints.end(), std::inserter(jointsSet, jointsSet.end()));
    }
    FreezeSkinnedJointTransform_(jointsSet, node, glm::mat4(1.0f));
    for (size_t i = 0; i < skins.size(); i++)
    {
        const auto& joints = skins[i]->GetJoints();
        std::vector<glm::mat4> mats;
        GetGlobalMatrices(mats, joints[0], glm::mat4(1.0f));
        for (size_t j = 0; j < mats.size(); j++)
        {
            mats[j] = glm::inverse(mats[j]);
        }
        skins[i]->SetJointBindMatrices(mats);
    }

    return true;
}

MStatus glTFExporter::exportProcess(const MString& fname, const std::vector<MDagPath>& dagPaths)
{
    MStatus status = MS::kSuccess;

    std::shared_ptr<kml::Options> opts = kml::Options::GetGlobalOptions();
    bool onefile = opts->GetInt("output_onefile") > 0;
    bool glb = opts->GetInt("output_glb") > 0;
    bool vrm = opts->GetInt("vrm_export") > 0;
    bool output_invisible_nodes = opts->GetInt("output_invisible_nodes") > 0;
    bool output_animations = opts->GetInt("output_animations") > 0;
    bool output_single_animation = opts->GetInt("output_single_animation") > 0;
    bool freeze_skinned_mesh_transform = opts->GetInt("freeze_skinned_mesh_transform") > 0;

    std::string generator_name = opts->GetString("generator_name");

    std::string dir_path = GetExportDirectory(fname.asChar());
    MakeDirectory(dir_path);

    typedef std::vector<std::shared_ptr<kml::Node> > NodeVecType;
    TexturePathManager texManager;
    MaterialMapType materials;
    NodeVecType nodes;
    std::shared_ptr<ProgressWindow> progWindow;
    {
        int prog = 100;
        if (dagPaths.size())
        {
            int max_size = prog * dagPaths.size();
            progWindow.reset(new ProgressWindow(generator_name, max_size));
            progWindow->SetProgressStatus("");
            for (int i = 0; i < (int)dagPaths.size(); i++)
            {
                if (progWindow->IsCancelled())
                {
                    return MS::kFailure;
                }
                bool bProcess = true;
                MDagPath dagPath = dagPaths[i];
                progWindow->SetProgressStatus(dagPath.fullPathName().asChar());
                MGlobal::executeCommand("refresh -cv -f;");
                if (!output_invisible_nodes)
                {
                    if (!IsVisible(dagPath))
                    {
                        bProcess = false;
                    }
                }
                if (bProcess)
                {
                    if (dagPath.hasFn(MFn::kMesh))
                    {
                        status = WriteGLTF(texManager, materials, nodes, MString(dir_path.c_str()), dagPath);
                    }
                }
                progWindow->SetProgressStatus("");
                progWindow->SetProgress(prog * (i + 1));
                MGlobal::executeCommand("refresh -cv -f;");
            }
            progWindow->SetProgressStatus("");
        }
    }

    if (nodes.empty())
    {
        return MStatus::kSuccess;
    }

    {
        std::vector<std::shared_ptr<kml::Node> > joint_nodes = GetJointNodes(nodes);
        for (size_t i = 0; i < joint_nodes.size(); i++)
        {
            nodes.push_back(joint_nodes[i]);
        }

        std::vector<std::shared_ptr<kml::Node> > all_nodes;
        for (size_t i = 0; i < nodes.size(); i++)
        {
            GetAllNodes(all_nodes, nodes[i]);
        }
        typedef std::set<std::shared_ptr<kml::Node> > SetType;
        SetType set_nodes;
        for (size_t i = 0; i < all_nodes.size(); i++)
        {
            set_nodes.insert(all_nodes[i]);
        }
        nodes.clear();
        for (SetType::iterator it = set_nodes.begin(); it != set_nodes.end(); it++)
        {
            nodes.push_back(*it);
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
            for (MaterialMapType::iterator it = materials.begin(); it != materials.end(); it++)
            {
                auto& mat = it->second;
                mat->SetInteger("_Index", index); //
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
                auto& mat = node->GetMaterials()[0];
                int index = mat->GetInteger("_Index");
                for (int j = 0; j < mesh->materials.size(); j++)
                {
                    mesh->materials[j] = index;
                }
            }
        }

        {
            for (MaterialMapType::iterator it = materials.begin(); it != materials.end(); it++)
            {
                auto& mat = it->second;
                auto keys = mat->GetTextureKeys();
                for (size_t j = 0; j < keys.size(); j++)
                {
                    auto tex = mat->GetTexture(keys[j]);
                    std::string file_path = dir_path + "/" + tex->GetFilePath();
                    tex->SetFileExists(IsFileExist(file_path));
                }
            }
        }

        {
            std::vector<std::shared_ptr<kml::Node> > tnodes;
            for (size_t i = 0; i < nodes.size(); i++)
            {
                GetAllNodes(tnodes, nodes[i]);
            }
            nodes = UniqueNodes(tnodes);
            for (size_t i = 0; i < nodes.size(); i++)
            {
                nodes[i]->ClearChildren();
            }
        }

        std::shared_ptr<kml::Node> node = CombineNodes(nodes);
        node->SetName(GetFileName(std::string(fname.asChar())));

        {
            for (MaterialMapType::iterator it = materials.begin(); it != materials.end(); it++)
            {
                auto& mat = it->second;
                node->AddMaterial(mat);
            }
        }

        if (freeze_skinned_mesh_transform)
        {
            FreezeSkinnedMeshTransform(node);
        }

        {
            std::vector<std::shared_ptr<kml::Skin> > skins = GetSkins(node);
            for (size_t i = 0; i < skins.size(); i++)
            {
                node->AddSkin(skins[i]);
            }
        }

        if (vrm)
        {
            FreezeSkinnedJointTransform(node);
        }

        // If vrm_export option is enabled, it shouldn't output animations.
        if (output_animations)
        {
            std::vector<std::shared_ptr<kml::Animation> > animations = GetAnimations(node);
            if(output_single_animation)
            {
                animations = MergeSingleAnimation(animations);
            }

            for (size_t i = 0; i < animations.size(); i++)
            {
                node->AddAnimation(animations[i]);
            }
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
        if (vrm)
        {
            extname = ".vrm";
        }
        std::string glb_path = RemoveExt(std::string(fname.asChar())) + extname;

        if (!kml::GLTF2GLB(gltf_path, glb_path))
        {
            status = MStatus::kFailure;
        }
        RemoveFiles(dir_path);
    }

    return status;
}
