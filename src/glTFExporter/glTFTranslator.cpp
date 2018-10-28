#ifdef _MSC_VER
#pragma warning(disable:4819)
#endif

#include "glTFTranslator.h"
#include "glTFExporter.h"


//////////////////////////////////////////////////////////////

typedef glTFTranslator::Mode Mode;

glTFTranslator::glTFTranslator (Mode mode)
{
    glTFExporter::Mode modeEx = glTFExporter::Mode::EXPORT_GLTF;
    switch(mode)
    {
        case Mode::EXPORT_GLTF: modeEx = glTFExporter::Mode::EXPORT_GLTF;break;
        case Mode::EXPORT_GLB:  modeEx = glTFExporter::Mode::EXPORT_GLB;break;
        case Mode::EXPORT_VRM:  modeEx = glTFExporter::Mode::EXPORT_VRM;break;
    }
    exporter.reset( new glTFExporter(modeEx) );
}

//////////////////////////////////////////////////////////////

glTFTranslator::~glTFTranslator ()
{
	;//
}

//////////////////////////////////////////////////////////////

void* glTFTranslator::creatorGLTF()
{
    return new glTFTranslator(Mode::EXPORT_GLTF);
}

void* glTFTranslator::creatorGLB()
{
    return new glTFTranslator(Mode::EXPORT_GLB);
}

void* glTFTranslator::creatorVRM()
{
    return new glTFTranslator(Mode::EXPORT_VRM);
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
    if(exporter)
	{
		return exporter->defaultExtension();
	}
    return "gltf";
}
//////////////////////////////////////////////////////////////

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
    if (exporter)
	{
		return exporter->filter();
	}
#ifndef ENABLE_VRM 
	return "*.gltf;*.vrm";
#else
    return "*.vrm";
#endif
}
