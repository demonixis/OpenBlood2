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
        NetworkWorldReceiver receiver;
        CHECK(receiver.begin(18));
        Net::WorldSnapshot first;
        first.world = 18;
        first.sequence = 10;
        first.time = 1;
        Net::ObjectState a, b;
        a.id = {0, 4};
        b.id = {1, 2};
        first.objects = {a, b};
        CHECK(receiver.receive(first, geometry, models, sprites, error));
        CHECK(!receiver.receive(first, geometry, models, sprites, error));
        auto after = first;
        after.sequence = 11;
        after.time = 1.05;
        after.objects[0].pose.position.x = 40;
        Net::WorldPatch patch;
        bool changed = true;
        CHECK(Net::make_world_patch(first, after, patch, changed) && !changed);
        after.objects[0].flags |= FLAG_VISIBLE;
        after.objects[1].id = {2, 1};
        CHECK(Net::make_world_patch(first, after, patch, changed) && changed);
        CHECK(patch.removed.size() == 1 && patch.removed[0] == b.id && patch.values.objects.size() == 2);
        CHECK(receiver.receive(patch, geometry, models, sprites, error));
        CHECK(receiver.membership_sequence() == 11 && !receiver.world().get(b.id));
        CHECK(receiver.world().get(a.id)->pose.position.x == 40 && receiver.world().get(ObjectId{2, 1}));
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        patch.base = 11;
        patch.values.sequence = 12;
        patch.removed.clear();
        patch.values.player = b.id;
        patch.values.objects.clear();
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        patch.values.player = {};
        patch.removed = {b.id};
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        patch.removed.clear();
        patch.values.objects = {b};
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        patch.values.objects[0].id.generation = 1;
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        patch.values.objects[0].id.generation = 3;
        CHECK(receiver.receive(patch, geometry, models, sprites, error));
        CHECK(!receiver.world().get(b.id) && receiver.world().get(ObjectId{1, 3}));
        patch.base = 12;
        patch.values.sequence = 13;
        patch.values.objects[0].id = {4, 1};
        patch.values.objects[0].type = OT_MODEL;
        patch.values.objects[0].has_model = true;
        patch.values.objects[0].filename = "models/absent.abc";
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        CHECK(receiver.membership_sequence() == 12 && !receiver.world().get(ObjectId{4, 1}));
        patch.values.objects[0].filename = "models/test.abc";
        CHECK(receiver.receive(patch, geometry, models, sprites, error));
        const auto before_replacement = receiver.world();
        auto current = Net::snapshot_values(receiver.world().frame());
        current.sequence = receiver.membership_sequence();
        auto replacement = current;
        replacement.sequence = 14;
        replacement.objects[0].id.generation = 5;
        CHECK(Net::make_world_patch(current, replacement, patch, changed) && changed);
        const auto removed = patch.removed;
        patch.removed.clear();
        CHECK(!receiver.receive(patch, geometry, models, sprites, error));
        patch.removed = removed;
        CHECK(receiver.receive(patch, geometry, models, sprites, error));
        CHECK(!receiver.world().get(a.id) && receiver.world().get(ObjectId{0, 5}));
        CHECK(before_replacement.get(a.id) && before_replacement.get(ObjectId{4, 1})->model);
        Net::WorldState state{
            18, 14, 15, receiver.world().frame().time, {Net::motion_values(replacement.objects.back())}};
        state.objects[0].animation.animation = UINT32_MAX;
        CHECK(receiver.receive(state, error) == NetworkWorldReceiver::StateResult::Refused);
        CHECK(receiver.world().get(ObjectId{4, 1})->animation.animation == 0);
        CHECK(receiver.begin(19));
        first.world = 19;
        first.sequence = 1;
        first.objects[0].id.generation = 1;
        CHECK(receiver.receive(first, geometry, models, sprites, error));
        CHECK(!receiver.world().get(ClientObjectId{18, a.id}) && receiver.world().get(ObjectId{0, 1}));
        std::cout << "Independent asset binding, atomic refusal, destruction and retained model lifetime\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
