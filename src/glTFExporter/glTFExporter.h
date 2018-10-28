#pragma once
#ifndef __GLTF_EXPORTER_H__
#define __GLTF_EXPORTER_H__

#include <maya/MDagPath.h>
#include <maya/MDistance.h>
#include <maya/MFileObject.h>
#include <maya/MFnMesh.h>
#include <maya/MIntArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MStatus.h>

#include <vector>

class glTFExporter : public MPxFileTranslator
{
public:
    enum Mode
    {
        EXPORT_GLTF = 0,
        EXPORT_GLB,
        EXPORT_VRM
    };

public:
    glTFExporter(Mode mode = EXPORT_GLTF);
    virtual ~glTFExporter(){};

    static void* creatorGLTF();
    static void* creatorGLB();
    static void* creatorVRM();

    MStatus reader(const MFileObject& file,
                   const MString& optionsString,
                   FileAccessMode mode);

    MStatus writer(const MFileObject& file,
                   const MString& optionsString,
                   FileAccessMode mode);
    bool haveReadMethod() const;
    bool haveWriteMethod() const;
    MString defaultExtension() const;
    MFileKind identifyFile(const MFileObject& fileName,
                           const char* buffer,
                           short size) const;
    MString filter() const;

protected:
    MStatus exportSelected(const MString& fname);
    MStatus exportAll(const MString& fname);

protected:
    MStatus exportProcess(const MString& fname, const std::vector<MDagPath>& dagPaths);

protected:
    Mode mode_;
};

#endif
