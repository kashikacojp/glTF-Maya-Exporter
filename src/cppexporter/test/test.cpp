#include "../exporter.h"

#include <iostream>
#include <ostream>
#include <string>

int main()
{
    std::cout << "cpp exporter test" << std::endl;

    glTFExporterCpp gltf;
    
    gltf.exportFile("out.gltf");
    
    return 0;
}