#pragma once
#ifndef __GLTF_TRANSRAOR_H__
#define __GLTF_TRANSRAOR_H__

#include <memory>
#include <maya/MPxFileTranslator.h>

class glTFTranslator : public MPxFileTranslator 
{
public:
                    glTFTranslator ();
    virtual         ~glTFTranslator ();
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
	MString         filter()const;
protected:
	std::shared_ptr<MPxFileTranslator> importer;
	std::shared_ptr<MPxFileTranslator> exporter;
};

#endif
