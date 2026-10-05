#include "TownCheck.h"

namespace remake
{

std::vector<TownIssue> CheckMaterials(const BchModel& model, const std::set<std::string>& available)
{
    std::vector<TownIssue> issues;
    std::set<std::string> reported;
    for (const BchMesh& mesh : model.Meshes)
    {
        if (mesh.Triangles.empty() || mesh.Material >= model.Materials.size()) continue; // draws nothing
        const BchMaterial& material = model.Materials[mesh.Material];
        for (const std::string& name : material.Texture)
        {
            if (name.empty() || name == "projection_dummy" || available.count(name)) continue;
            if (!reported.insert(material.Name + "/" + name).second) continue;
            issues.push_back({true, "material " + material.Name + " names texture " + name + ", which the area pack does not hold"});
        }
    }
    return issues;
}

std::vector<TownIssue> CheckBudget(const BchModel& model, size_t fileBytes, const PieceBudget& original)
{
    std::vector<TownIssue> issues;
    size_t vertices = 0;
    for (size_t i = 0; i < model.Meshes.size(); i++)
    {
        const BchMesh& mesh = model.Meshes[i];
        vertices += mesh.Vertices.size();
        if (mesh.Vertices.size() > 65536) issues.push_back({true, "mesh " + std::to_string(i) + " has " + std::to_string(mesh.Vertices.size()) + " vertices, more than 16-bit indices reach"});
    }
    if (original.MaxVertices && vertices > original.MaxVertices)
        issues.push_back({false, std::to_string(vertices) + " vertices, more than the game's largest piece (" + std::to_string(original.MaxVertices) + "): memory use is untested"});
    if (original.MaxFileBytes && fileBytes > original.MaxFileBytes)
        issues.push_back({false, "piece of " + std::to_string(fileBytes) + " bytes, larger than the game's largest (" + std::to_string(original.MaxFileBytes) + ")"});
    return issues;
}

}
