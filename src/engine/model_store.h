#pragma once
#include "engine/model.h"
#include "engine/resources.h"
#include <map>

namespace OpenBlood2
{
    class ModelStore
    {
      public:
        explicit ModelStore(Resources& resources) : resources_(resources) {}
        bool load(std::string_view path, std::shared_ptr<const Model>& model, std::string& error);
        void clear()
        {
            cache_.clear();
        }

      private:
        Resources& resources_;
        std::map<std::string, std::weak_ptr<const Model>> cache_;
    };
} // namespace OpenBlood2
