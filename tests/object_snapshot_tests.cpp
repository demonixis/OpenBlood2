#include "game/server_adapter.h"
#include <algorithm>
#include <functional>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    struct Hooks
    {
        std::function<void()> construct;
        std::function<void(HOBJECT)> precreate, destroy;
        bool reject = false;
    };
    Hooks* hooks = nullptr;
    struct Scope
    {
        explicit Scope(Hooks& h)
        {
            hooks = &h;
        }
        ~Scope()
        {
            hooks = nullptr;
        }
    };
    class Probe : public BaseClass
    {
      public:
        Probe()
        {
            if (hooks && hooks->construct)
                hooks->construct();
        }
        ~Probe() override
        {
            if (hooks && hooks->destroy)
                hooks->destroy(m_hObject);
        }

      protected:
        DDWORD EngineMessageFn(DDWORD message, void* data, float value) override
        {
            const auto result = BaseClass::EngineMessageFn(message, data, value);
            if (message == MID_PRECREATE && hooks)
            {
                if (hooks->precreate)
                    hooks->precreate(m_hObject);
                if (hooks->reject)
                    return 0;
            }
            return result;
        }
    };
    HOBJECT ordinary(GameSession& g)
    {
        return g.objects.create(g.objects.find_class("BaseClass"));
    }
    ObjectId id(GameSession& g, HOBJECT h)
    {
        return g.objects.get(h)->id;
    }
    bool membership()
    {
        GameSession g;
        CHECK(g.initialize() && g.objects.snapshot().empty() && !g.objects.record_count());
        auto a = ordinary(g), b = ordinary(g), c = ordinary(g);
        CHECK(a && b && c);
        const auto ai = id(g, a), bi = id(g, b), ci = id(g, c);
        const auto held = g.objects.snapshot();
        CHECK((held == std::vector<ObjectId>{ai, bi, ci}) && g.objects.record_count() == 3);
        g.objects.get(c)->activity = OBJSTATE_INACTIVE;
        g.objects.get(c)->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(g.objects.remove(b) && g.objects.snapshot() == held && g.objects.record_count() == 3);
        CHECK(g.objects.get(b)->life == Life::Pending && g.objects.finish_frame() && !g.objects.get(bi));
        CHECK((g.objects.snapshot() == std::vector<ObjectId>{ai, ci}));
        auto reuse = ordinary(g);
        CHECK(reuse && id(g, reuse).slot == bi.slot && id(g, reuse).generation != bi.generation);
        CHECK((g.objects.snapshot() == std::vector<ObjectId>{ai, id(g, reuse), ci}));
        CHECK((held == std::vector<ObjectId>{ai, bi, ci}));
        auto mutable_copy = g.objects.snapshot();
        mutable_copy.clear();
        CHECK(g.objects.record_count() == 3 && g.objects.snapshot().size() == 3);
        CHECK(g.unload_world() && !g.objects.record_count() && g.objects.snapshot().empty());
        CHECK(!g.objects.get(ai) && !g.objects.get(bi) && !g.objects.get(ci));
        CHECK(ordinary(g) && g.objects.record_count() == 1 && g.objects.snapshot().front().generation != ai.generation);
        return true;
    }
    bool callbacks()
    {
        Hooks h;
        GameSession g;
        Scope scope(h); // Captured stack state cannot outlive this scope on a failed assertion.
        CHECK(g.initialize());
        static ClassInfo type{
            "SnapshotProbe", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); }, {}};
        static ClassInfo null_type{"SnapshotNull", nullptr, []() -> std::unique_ptr<BaseClass> { return nullptr; }, {}};
        CHECK(g.objects.register_class(type) && g.objects.register_class(null_type));
        auto first = ordinary(g);
        const auto first_id = id(g, first);
        ObjectId outer{};
        HOBJECT nested = nullptr, from_destructor = nullptr;
        bool constructor_ok = false, precreate_ok = false, destructor_ok = false;
        h.construct = [&]
        {
            const auto during = g.objects.snapshot();
            constructor_ok = during.size() == 2 && during.front() == first_id && !g.objects.get(during.back())->game;
            outer = during.back();
            nested = ordinary(g);
            constructor_ok = constructor_ok && nested && g.objects.record_count() == 3 && during.size() == 2;
        };
        h.precreate = [&](HOBJECT object)
        {
            precreate_ok =
                id(g, object) == outer && g.objects.snapshot() == std::vector<ObjectId>{first_id, outer, id(g, nested)};
        };
        auto object = g.objects.create(&type);
        CHECK(object && constructor_ok && precreate_ok);
        h.destroy = [&](HOBJECT owner)
        {
            const auto before = g.objects.snapshot();
            destructor_ok = g.objects.get(owner)->life == Life::Destroying &&
                            before == std::vector<ObjectId>{first_id, outer, id(g, nested)};
            from_destructor = ordinary(g);
            destructor_ok = destructor_ok && from_destructor && g.objects.remove(nested) &&
                            g.objects.record_count() == 4 && g.objects.snapshot().size() == 4 && before.size() == 3;
        };
        CHECK(g.objects.remove(object) && g.objects.finish_frame() && destructor_ok);
        CHECK(!g.objects.get(outer) && !g.objects.get(nested) && g.objects.get(from_destructor));
        CHECK((g.objects.snapshot() == std::vector<ObjectId>{first_id, id(g, from_destructor)}));
        h = {};
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        h.reject = true;
        CHECK(!g.objects.create(&type) && g.objects.record_count() == 1);
        const auto rejected = g.objects.snapshot().front();
        CHECK(g.objects.get(rejected)->life == Life::Pending && g.objects.finish_frame() && !g.objects.get(rejected));
        CHECK(!g.objects.create(&null_type) && g.objects.record_count() == 1);
        const auto empty = g.objects.snapshot().front();
        CHECK(g.objects.get(empty)->life == Life::Pending && !g.objects.get(empty)->game);
        CHECK(g.objects.finish_frame() && !g.objects.record_count());
        return true;
    }
    bool churn()
    {
        GameSession g;
        CHECK(g.initialize());
        std::vector<ObjectId> original;
        for (unsigned i = 0; i < 1024; ++i)
        {
            const auto object = ordinary(g);
            CHECK(object);
            original.push_back(id(g, object));
        }
        CHECK(g.objects.snapshot() == original);
        for (auto i : original)
            if (i != original.back())
                CHECK(g.objects.remove(g.objects.get(i)->handle));
        CHECK(g.objects.finish_frame() && g.objects.record_count() == 1);
        for (unsigned i = 0; i < 1024; ++i)
        {
            const auto held = g.objects.snapshot();
            CHECK(held == std::vector<ObjectId>{original.back()});
            const auto fresh = ordinary(g);
            CHECK(fresh && id(g, fresh).slot == original.front().slot && !g.objects.get(original.front()));
            CHECK((g.objects.snapshot() == std::vector<ObjectId>{id(g, fresh), original.back()}));
            CHECK(g.objects.remove(fresh) && g.objects.finish_frame() && g.objects.snapshot() == held);
        }
        CHECK(g.unload_world() && g.objects.snapshot().empty() && !g.objects.record_count());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::map<std::string, bool (*)()> tests{
        {"membership", membership}, {"callbacks", callbacks}, {"churn", churn}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second() ? 0 : 1;
}
