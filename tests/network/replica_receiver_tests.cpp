#include "../model_fixture.h"
#include "client/object_scene.h"
#include "client/session.h"
#include <iostream>
#include <limits>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run()
    {
        // This executable does not link blood2_game: presentation needs no original server.
        ClientSession client;
        const auto epoch = client.world.frame().world;
        ClientWorldFrame frame;
        frame.world = epoch;
        frame.sequence = 1;
        frame.time = .125;
        ClientObject object;
        object.id = {2, 7};
        object.type = OT_MODEL;
        object.flags = FLAG_VISIBLE;
        auto model = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *model, error));
        object.model = model;
        object.filename = "synthetic.abc";
        object.animation.time = 25;
        object.pose.position = {1, 2, 3};
        frame.objects.push_back(object);
        CHECK(client.receive(std::move(frame), error));
        const auto reference = client.world.reference(object.id);
        CHECK(client.world.get(reference) && !client.world.get(ObjectId{2, 6}));
        Resources resources;
        ModelView view;
        for (int i = 0; i < 3; ++i)
        {
            view.begin_frame();
            CHECK(append_client_models(client.world, true, view, resources, error));
            CHECK(view.entries.size() == 1 && view.world_draws.size() == 1);
            CHECK(client.world.get(reference)->animation.time == 25 && client.world.frame().time == .125);
        }
        const auto accepted = client.world.frame();
        auto reject = [&](ClientWorldFrame next)
        {
            return !client.receive(std::move(next), error) && !error.empty() && client.world.frame().sequence == 1 &&
                   client.world.get(reference);
        };
        CHECK(reject(accepted)); // Duplicate.
        auto next = accepted;
        next.sequence = 2;
        next.world += 1;
        CHECK(reject(next));
        next = accepted;
        next.sequence = 2;
        next.time = std::numeric_limits<double>::quiet_NaN();
        CHECK(reject(next));
        next.time = 0;
        CHECK(reject(next));
        next = accepted;
        next.sequence = 2;
        next.objects.push_back(object);
        CHECK(reject(next)); // Duplicate/unordered slots cannot replace the accepted frame.
        next.objects[1].id = {1, 1};
        CHECK(reject(next));
        next.objects.resize(ClientWorld::object_limit + 1);
        CHECK(reject(std::move(next)));
        next = accepted;
        next.sequence = 2;
        next.objects[0].id.generation = 8;
        CHECK(client.receive(std::move(next), error));
        CHECK(!client.world.get(reference) && client.world.get(ObjectId{2, 8}));
        CHECK(!client.world.begin(epoch) && client.world.frame().sequence == 2);
        client.reset_local_world();
        CHECK(client.world.frame().objects.empty() && !client.world.get(reference));
        next = accepted;
        next.world = client.world.frame().world;
        CHECK(client.receive(std::move(next), error));
        CHECK(client.world.get(object.id) && !client.world.get(reference));
        CHECK(!client.receive(ClientWorldFrame(accepted), error));
        ClientSession other;
        CHECK(other.world.frame().world != client.world.frame().world);
        return true;
    }
} // namespace
int main()
{
    return run() ? 0 : 1;
}
