#pragma once
#include "model_fixture.h"
#include <map>
#include <string>
namespace test_resource
{
    // A small generated REZ tree containing only synthetic ABC/skin bytes. It
    // exercises the normal resource path; production has no injected model loader.
    struct Directory
    {
        std::map<std::string, Directory> children;
        std::map<std::string, std::vector<std::uint8_t>> files;
    };
    using test_model::Writer;
    inline void cstr(Writer& w, std::string_view value)
    {
        w.b.insert(w.b.end(), value.begin(), value.end());
        w.u8(0);
    }
    inline std::pair<unsigned, unsigned> write_dir(Writer& w, const Directory& dir)
    {
        Writer records;
        for (const auto& [name, child] : dir.children)
        {
            auto [offset, size] = write_dir(w, child);
            records.u32(1);
            records.u32(offset);
            records.u32(size);
            records.u32(0);
            cstr(records, name);
        }
        for (const auto& [name, data] : dir.files)
        {
            auto offset = static_cast<unsigned>(w.b.size());
            w.b.insert(w.b.end(), data.begin(), data.end());
            auto dot = name.rfind('.');
            auto ext = name.substr(dot + 1);
            unsigned extension = 0;
            for (char value : ext)
                extension = (extension << 8) | static_cast<unsigned char>(value);
            records.u32(0);
            records.u32(offset);
            records.u32(static_cast<unsigned>(data.size()));
            records.u32(0);
            records.u32(1);
            records.u32(extension);
            records.u32(0);
            cstr(records, name.substr(0, dot));
            cstr(records, "");
        }
        auto offset = static_cast<unsigned>(w.b.size());
        w.b.insert(w.b.end(), records.b.begin(), records.b.end());
        return {offset, static_cast<unsigned>(records.b.size())};
    }
} // namespace test_resource
