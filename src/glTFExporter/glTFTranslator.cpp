#ifdef _MSC_VER
#pragma warning(disable:4819)
#endif

#include "glTFTranslator.h"
#include "glTFExporter.h"


//////////////////////////////////////////////////////////////

glTFTranslator::glTFTranslator ()
{
	//importer.reset( new gltfImporter() );
	exporter.reset( new glTFExporter() );
}

//////////////////////////////////////////////////////////////

glTFTranslator::~glTFTranslator ()
{
	;//
}

//////////////////////////////////////////////////////////////

void* glTFTranslator::creator()
{
    return new glTFTranslator();
}

//////////////////////////////////////////////////////////////

MStatus glTFTranslator::reader ( const MFileObject& file,
                                const MString& options,
                                FileAccessMode mode)
{
    if(importer)
	{
		return importer->reader(file, options, mode);
	}
	return MS::kFailure;
}

//////////////////////////////////////////////////////////////

MStatus glTFTranslator::writer ( const MFileObject& file,
                                const MString& options,
                                FileAccessMode mode )

{
	if(exporter)
	{
		return exporter->writer(file, options, mode);
	}
	return MS::kFailure;
}
//////////////////////////////////////////////////////////////

bool glTFTranslator::haveReadMethod () const
{
	if(importer)
	{
		return importer->haveReadMethod();
	}
    return false;
}
//////////////////////////////////////////////////////////////

bool glTFTranslator::haveWriteMethod () const
{
	if(exporter)
	{
		return exporter->haveWriteMethod();
	}
    return false;
}
//////////////////////////////////////////////////////////////

MString glTFTranslator::defaultExtension () const
{
    return "gltf";
}
//////////////////////////////////////////////////////////////

#ifdef _WIN32
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#endif

MPxFileTranslator::MFileKind glTFTranslator::identifyFile (
                                        const MFileObject& fileName,
                                        const char* buffer,
                                        short size) const
{
	if (exporter)
	{
		return exporter->identifyFile(fileName, buffer, size);
	}

	return MPxFileTranslator::kNotMyFileType;
}

MString         glTFTranslator::filter()const
{
#ifndef ENABLE_VRM 
	return "*.gltf;*.glb";
#else
    return "*.vrm";
#endif
}
