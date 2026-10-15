#pragma once
#include "engine/objects.h"
#include <array>
#include <unordered_set>

struct StringHandle {};
struct LMessage {};
struct ConsoleVariable { float value; };

namespace blood2 {

class ServerAdapter final : public ServerDE {
public:
    explicit ServerAdapter(Objects& objects);
    ~ServerAdapter() override;
    ServerAdapter(const ServerAdapter&) = delete;
    ServerAdapter& operator=(const ServerAdapter&) = delete;
    bool bind();
    void unbind();
    void set_game_settings(unsigned difficulty, unsigned game_type);
    std::size_t string_count() const { return strings_.size(); }
    std::size_t message_count() const { return messages_.size(); }
    std::size_t list_count() const { return lists_.size(); }

    HMESSAGEWRITE StartHMessageWrite() override;
    DRESULT WriteToMessageDWord(HMESSAGEWRITE, DDWORD) override;
    DRESULT WriteToMessageHString(HMESSAGEWRITE, HSTRING) override;
    DDWORD ReadFromMessageDWord(HMESSAGEREAD) override;
    HSTRING ReadFromMessageHString(HMESSAGEREAD) override;
    void EndHMessageWrite(HMESSAGEWRITE) override;
    void EndHMessageRead(HMESSAGEREAD) override;
    void ResetRead(HMESSAGEREAD) override;
    HMESSAGEWRITE StartMessageToObject(LPBASECLASS, HOBJECT, DDWORD) override;
    DRESULT EndMessage(HMESSAGEWRITE) override;
    DRESULT MoveObject(HOBJECT, DVector*) override;

private:
    struct String { std::string text; unsigned references = 1; };
    struct Message {
        std::vector<std::uint8_t> bytes;
        std::size_t cursor = 0;
        bool bad = false, routed = false, borrowed = false;
        HOBJECT sender = nullptr, receiver = nullptr;
        DDWORD id = 0;
    };
    Objects& objects_;
    bool bound_ = false;
    std::array<ConsoleVariable, 2> variables_{{{1}, {0}}};
    std::vector<std::unique_ptr<StringHandle>> string_tokens_;
    std::unordered_map<HSTRING, String> strings_;
    std::vector<std::unique_ptr<LMessage>> message_tokens_;
    std::unordered_map<HMESSAGEWRITE, Message> messages_;
    std::vector<std::unique_ptr<ObjectList>> list_tokens_;
    std::unordered_set<ObjectList*> lists_;
    static ServerAdapter* active_;
    static ServerAdapter& active();
    HSTRING make_string(const char* text);
    HSTRING copy_string(HSTRING);
    void free_string(HSTRING);
    char* string_data(HSTRING);
    ObjectList* make_list();
    ObjectLink* add_to_list(ObjectList*, HOBJECT);
    void remove_from_list(ObjectList*, HOBJECT);
    void release_list(ObjectList*);
    Message* writable(HMESSAGEWRITE);
    Message* readable(HMESSAGEREAD, std::size_t bytes);
    DRESULT unsupported(const char* service);
};

bool register_original_classes(Objects& objects);
class GameSession {
public:
    Objects objects;
    ServerAdapter server{objects};
    ~GameSession() { objects.clear(); }
    bool initialize() { return server.bind() && register_original_classes(objects); }
    HOBJECT create_example(std::string_view class_name);
};

}
