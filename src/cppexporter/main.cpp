#define DLL_BUILD
#include "exporter.h"

#include <iostream>

class glTFExporterCpp::Impl{
private:
    int test;
public:
    Impl();
    ~Impl();
    bool exportFile(const char* gltf_filepath);

};

glTFExporterCpp::Impl::Impl()
{
    test = 0;
}
glTFExporterCpp::Impl::~Impl()
{
}

bool glTFExporterCpp::Impl::exportFile(const char* gltf_filepath)
{
    std::cout << "DEBUG exportFile: " << gltf_filepath << std::endl;
    return false;
}

// ------------------------------------------------

glTFExporterCpp::glTFExporterCpp()
{
    m_imp = new Impl();
} 
glTFExporterCpp::~glTFExporterCpp()
{
    delete m_imp;
}

bool glTFExporterCpp::exportFile(const char* gltf_filepath) 
{
    return m_imp->exportFile(gltf_filepath);
}

#ifdef _WIN32
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
}
#endif