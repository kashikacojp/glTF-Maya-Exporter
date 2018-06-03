#pragma once
#ifndef __GLTF_EXPORTER_H__
#define __GLTF_EXPORTER_H__

#include <maya/MStatus.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MDagPath.h>
#include <maya/MFileObject.h>
#include <maya/MDistance.h>
#include <maya/MIntArray.h>
#include <maya/MFnMesh.h>

#include <vector>

class glTFExporter : public MPxFileTranslator {
public:
                    glTFExporter () {};
    virtual         ~glTFExporter () {};
    static void*    creator();

    MStatus         reader ( const MFileObject& file,
                             const MString& optionsString,
                             FileAccessMode mode);

    MStatus         writer ( const MFileObject& file,
                             const MString& optionsString,
                             FileAccessMode mode );
    bool            haveReadMethod () const;
    bool            haveWriteMethod () const;
    MString         defaultExtension () const;
    MFileKind       identifyFile ( const MFileObject& fileName,
                                   const char* buffer,
                                   short size) const;
protected:
    MStatus         exportSelected(const MString& fname);
    MStatus         exportAll     (const MString& fname);
protected:
	MStatus         exportProcess(const MString& fname, const std::vector< MDagPath >& dagPaths);
};

#endif
