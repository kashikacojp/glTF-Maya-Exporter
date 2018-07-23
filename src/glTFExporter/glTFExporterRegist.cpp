#ifdef _MSC_VER
#pragma warning(disable:4819)
#endif

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MFnPlugin.h>
#include <maya/MString.h>
#include <maya/MStreamUtils.h>

#include <fstream>
#include <sstream>

#include "glTFTranslator.h"

#define VENDOR_NAME "KASHIKA,Inc."
#define PLUGIN_VERSION "1.0.2"

const char *const gltfOptionScript = "glTFExporterOptions";
const char *const gltfDefaultOptions =
    "recalc_normals=0;"
	"output_onefile=1;"
	"output_glb=0;"
	"make_preload_texture=0;"
	"output_buffer=1;"
	"convert_texture_format=1;"
    ;

static
void PrintTextLn(const std::string& str)
{
#if MAYA_API_VERSION >= 20180000
	MStreamUtils::stdOutStream() << str << std::endl;
#else
	std::cerr << str << std::endl;
#endif
}

static
void ShowLicense()
{
	std::string showText;
	showText += "glTF-Maya-Exporter";
	showText += " ";
	showText += "ver ";
	showText += PLUGIN_VERSION;

	PrintTextLn(showText);
}

MStatus initializePlugin( MObject obj )
{
    MFnPlugin plugin( obj, VENDOR_NAME, PLUGIN_VERSION, "Any");

	ShowLicense();
    // Register the translator with the system
    return plugin.registerFileTranslator( "glTFExporter", "none",
                                          glTFTranslator::creator,
                                          (char *)gltfOptionScript,
                                          (char *)gltfDefaultOptions 
	);                               
}
//////////////////////////////////////////////////////////////

MStatus uninitializePlugin( MObject obj )
{
    MFnPlugin plugin( obj );
    return plugin.deregisterFileTranslator( "glTFExporter" );
}

//////////////////////////////////////////////////////////////

