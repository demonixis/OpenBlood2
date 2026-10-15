#include "engine/model_store.h"
namespace blood2 {
bool ModelStore::load(std::string_view path,std::shared_ptr<const Model>& model,std::string& error) {
    model.reset(); error.clear();
    const auto* resource=resources_.find(path);
    if (!resource) { error="Model resource not mounted: "+std::string(path); return false; }
    // Key by resolved archive + byte range: remounting an override cannot reuse
    // a model decoded from a different archive entry.
    const auto key=path_text(resource->archive)+":"+std::to_string(resource->offset)+":"+std::to_string(resource->size);
    if (auto it=cache_.find(key); it!=cache_.end()) if (auto loaded=it->second.lock()) { model=std::move(loaded); return true; }
    std::vector<std::uint8_t> bytes;
    if (!resources_.read(path,bytes,error)) return false;
    auto loaded=std::make_shared<Model>();
    if (!read_model(bytes,*loaded,error)) return false;
    model=std::move(loaded); cache_[key]=model; return true;
}
}
