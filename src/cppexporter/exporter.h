#ifdef DLL_BUILD
    #if _WIN32
        #define DllExport   __declspec( dllexport ) 
    #else
        #define DllExport
    #endif
#else
    #define DllExport
#endif

class DllExport glTFExporterCpp
{
public:
    glTFExporterCpp();
    ~glTFExporterCpp();

    bool exportFile(const char* gltf_filepath);
private:
    class Impl;
    Impl* m_imp;
};