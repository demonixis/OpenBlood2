#include "../resource_fixture.h"
#include "client/network_world.h"
#include "client/object_scene.h"
#include <fstream>
#include <iostream>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << "\n";                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* root)
    {
        std::string error;
        test_resource::Directory dir;
        dir.children["models"].files["test.abc"] = test_model::abc().b;
        test_resource::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        const auto [offset, size] = test_resource::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        CHECK(!ec);
        const auto path = std::filesystem::path(root) / "synthetic.rez";
        {
            std::ofstream f(path, std::ios::binary);
            f.write(reinterpret_cast<const char*>(w.b.data()), std::streamsize(w.b.size()));
            CHECK(f.good());
        }
        Resources resources;
        CHECK(resources.mount(path, error));
        ModelStore models(resources);
        SpriteStore sprites(resources);
        World geometry;
        geometry.models.resize(2);
        geometry.models[1].name = "Door";
        ClientWorld replica;
        CHECK(replica.begin(17));
        Net::WorldSnapshot snapshot;
        snapshot.world = 17;
        snapshot.sequence = 1;
        Net::ObjectState object;
        object.id = {0, 1};
        object.type = OT_MODEL;
        object.has_model = true;
        object.filename = "models/test.abc";
        object.flags = FLAG_VISIBLE;
        object.animation.time = 20;
        snapshot.objects.push_back(object);
        object = {};
        object.id = {1, 7};
        object.type = OT_WORLDMODEL;
        object.filename = "Door";
        object.world_model_bound = true;
        object.world_model_index = 1;
        snapshot.objects.push_back(object);
        CHECK(receive_network_world(snapshot, geometry, models, sprites, replica, error));
        CHECK(replica.get(ObjectId{0, 1})->model && replica.get(ObjectId{0, 1})->animation.time == 20);
        const auto retained = replica;
        auto refuse = [&](Net::WorldSnapshot s)
        {
            return !receive_network_world(s, geometry, models, sprites, replica, error) &&
                   replica.frame().sequence == 1 && replica.get(ObjectId{0, 1})->model;
        };
        CHECK(refuse(snapshot));
        snapshot.sequence = 2;
        auto bad = snapshot;
        bad.objects[0].filename = "models/absent.abc";
        CHECK(refuse(bad));
        bad = snapshot;
        bad.objects[0].animation.animation = UINT32_MAX;
        CHECK(refuse(bad));
        bad = snapshot;
        bad.objects[0].hidden_nodes = {1};
        CHECK(refuse(bad));
        bad = snapshot;
        bad.objects[1].world_model_index = 2;
        CHECK(refuse(bad));
        bad = snapshot;
        bad.objects[1].filename = "wrong";
        CHECK(refuse(bad));
        snapshot.objects.erase(snapshot.objects.begin());
        CHECK(receive_network_world(snapshot, geometry, models, sprites, replica, error));
        CHECK(!replica.get(ObjectId{0, 1}) && retained.get(ObjectId{0, 1})->model);
        models.clear();
        sprites.clear();
        ModelView view;
        CHECK(append_client_models(retained, true, view, resources, error));
        CHECK(view.entries.size() == 1 && retained.frame().sequence == 1);
        std::cout << "Independent asset binding, atomic refusal, destruction and retained model lifetime\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
