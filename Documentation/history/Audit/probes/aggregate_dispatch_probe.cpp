// Link with the actual adapted cpp_engineobjects_de.cpp, not copied logic.
#include "cpp_engineobjects_de.h"
#include <cstdio>
ServerDE* g_pServerDE = nullptr;
__ClassDefiner* __g_ClassDefinerHead = nullptr;

class SyntheticObject : public BaseClass
{
public:
    unsigned calls = 0;
    DDWORD ObjectMessageFn(HOBJECT sender, DDWORD id, HMESSAGEREAD message) override
    {
        ++calls;
        // Bound the recursion so the audit does not crash the host process.
        if (calls == 8) return 1;
        return BaseClass::ObjectMessageFn(sender, id, message);
    }
};

int main()
{
    SyntheticObject object;
    BaseClass::_ObjectMsgFn(&object, nullptr, 1000, nullptr);
    std::printf("one_object_message_callback_entries=%u (bounded_at_8)\n", object.calls);
    return object.calls == 1 ? 0 : 1;
}
