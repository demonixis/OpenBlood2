#include "CharacterNodes.h"
namespace OpenBlood2
{
    namespace
    {
        bool collect(HOBJECT object, int node, std::vector<std::string>& names, std::string& error)
        {
            auto* server = BaseClass::GetServerDE();
            if (!server || !server->IsObjectAlive(object) || node < 0 || node >= int(NUM_ALL_NODES))
            {
                error = "Invalid character node owner/index";
                return false;
            }
            // Source ignores a missing base but still visits consecutive _extraN names.
            const std::string base = szNodes[node];
            for (unsigned extra = 0; extra <= 257; ++extra)
            {
                auto name = base + (extra ? "_extra" + std::to_string(extra) : "");
                DBOOL hidden = DFALSE;
                const auto result = server->GetModelNodeHideStatus(object, name.c_str(), &hidden);
                if (result == DE_OK)
                    names.push_back(std::move(name));
                else if (result != DE_NOTFOUND)
                {
                    error = "Character node visibility unavailable: " + name;
                    return false;
                }
                else if (extra)
                    return true;
            }
            error = "Character extra-node limit exceeded";
            return false;
        }
        bool apply(HOBJECT object, const std::vector<std::string>& names, bool hidden, std::string& error)
        {
            for (const auto& name : names)
                if (BaseClass::GetServerDE()->SetModelNodeHideStatus(object, name.c_str(), hidden) != DE_OK)
                {
                    error = "Character node visibility assignment failed: " + name;
                    return false;
                }
            return true;
        }
    } // namespace
    bool hide_character_node(HOBJECT object, int node, bool hidden, std::string& error)
    {
        error.clear();
        std::vector<std::string> names;
        return collect(object, node, names, error) && apply(object, names, hidden, error);
    }
    bool reset_character_nodes(HOBJECT object, std::string& error)
    {
        error.clear();
        std::vector<std::string> names;
        // Preflight all lookups before mutation; missing standard nodes are normal.
        for (unsigned i = 0; i < NUM_ALL_NODES; ++i)
            if (!collect(object, i, names, error))
                return false;
        return apply(object, names, false, error);
    }
} // namespace OpenBlood2
