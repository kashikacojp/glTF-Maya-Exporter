#pragma once
#ifndef __GLTF_TRANSRAOR_H__
#define __GLTF_TRANSRAOR_H__

#include <maya/MPxFileTranslator.h>
#include <memory>

class glTFTranslator : public MPxFileTranslator
{
public:
    enum Mode
    {
        EXPORT_GLTF = 0,
        EXPORT_GLB,
        EXPORT_VRM
    };

public:
    glTFTranslator(Mode mode = EXPORT_GLTF);
    virtual ~glTFTranslator();

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
    std::shared_ptr<MPxFileTranslator> importer;
    std::shared_ptr<MPxFileTranslator> exporter;
};

#endif
