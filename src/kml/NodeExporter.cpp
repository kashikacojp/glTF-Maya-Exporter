#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "NodeExporter.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

#include "Compatibility.h"
#include <glm/mat4x4.hpp>

namespace kml
{
    using namespace std;
    using namespace glm;

    static std::string GetIndentString(int level)
    {
        std::stringstream ss;
        for (int i = 0; i < level; i++)
        {
            ss << "    ";
        }
        return ss.str();
    }

    static void ExportMesh(FILE* fp, const std::string& path, int level, const std::shared_ptr<Mesh>& mesh)
    {
        std::string strIndent = GetIndentString(level);
        fprintf(fp, "%s \"Mesh\" : {\n", strIndent.c_str());

        fprintf(fp, "%s }", strIndent.c_str());
    }

    static void ExportNode(FILE* fp, const std::string& path, int level, const std::shared_ptr<Node>& node)
    {
        std::string strIndent = GetIndentString(level);
        std::string strChildIndent = strIndent + GetIndentString(1);
        fprintf(fp, "%s {\n", strIndent.c_str());

        {
            fprintf(fp, "%s \"Name\" : ", strIndent.c_str());
        }

        if (node->IsLeaf() && node->HasShape())
        {
            const auto& mesh = node->GetMesh();
            assert(mesh.get());
            ExportMesh(fp, path, level + 1, mesh);
            fprintf(fp, "\n");
        }
        else if (!node->IsLeaf())
        {

            fprintf(fp, "%s \"Children\" : [\n", strChildIndent.c_str());
            const auto& children = node->GetChildren();
            for (int i = 0; i < (int)children.size(); i++)
            {
                ExportNode(fp, path, level + 1, children[i]);
                if (i != (int)children.size() - 1)
                {
                    fprintf(fp, ",\n");
                }
            }
            fprintf(fp, "%s]\n", strChildIndent.c_str());
        }
        fprintf(fp, "%s }", strIndent.c_str());
    }

    bool NodeExporter::Export(const std::string& path, const std::shared_ptr<Node>& node)
    {
        FILE* fp = fopen(path.c_str(), "wt");
        if (fp == NULL)
        {
            return false;
        }

        ExportNode(fp, path, 0, node);
        fprintf(fp, "\n");

        fclose(fp);
        return true;
    }
} // namespace kml
