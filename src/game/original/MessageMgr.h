// Selective original Blood II CMessageMgr (ClientShellDLL/MessageMgr.cpp): the on-screen
// message lines. See Documentation/contracts/original_hud_contracts.md#message-lines.
#pragma once
#include "game/player_messages.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Original ring of kMaxMessages entries, with the shell's SetMaxMessages(6) and the default
// five-second lifetime. Owned text replaces the HSTRING/HSURFACE pair; the application draws
// the returned lines. The CInputLine editor stays the application's MessageInput.
class CMessageMgr
{
  public:
    enum
    {
        kMaxMessages = 20,
        kMaxMessageSize = 128,
        kDefaultMaxMessages = 4,
        kDefaultMessageTime = 5, // seconds
        kFontHeight = 16,        // Init: CreateFont("Arial", 6, 16) and m_nFontHeight = 16.
    };
    void Enable(bool enabled);
    // AddLine: the oldest line expires when the count passes the maximum. An empty string
    // still occupies a line, as CreateSurfaceFromString's padded surface did.
    void AddLine(std::string_view text, float time, const OpenBlood2::ConsoleTextSource& source = {});
    void Clear();
    void SetMaxMessages(int count);
    void SetMessageTime(float seconds)
    {
        m_fMessageTime = seconds;
    }
    void SetCoordinates(int x, int y)
    {
        m_x = x;
        m_y = y;
    }
    struct Line
    {
        std::string text;
        int x = 0, y = 0;
        OpenBlood2::ConsoleTextSource source;
    };
    // Original Draw: expires lines whose time has come (the oldest first, as the source
    // advanced m_nFirstMessage) and returns the remaining lines at their screen positions.
    // `input_y` receives the row of the CInputLine editor below them.
    std::vector<Line> Draw(float time, int* input_y = nullptr);
    int GetMaxMessages() const
    {
        return m_nMaxMessages;
    }
    float GetMessageTime() const
    {
        return m_fMessageTime;
    }
    bool GetState() const
    {
        return m_bEnabled;
    }
    int count() const
    {
        return m_nMessageCount;
    }
    // Current lines, oldest first (inspection and tests; no expiry).
    std::vector<std::string> lines() const;
    std::uint64_t added = 0, expired = 0;

  private:
    struct Message
    {
        float fExpiration = 0;
        std::string text;
        OpenBlood2::ConsoleTextSource source;
    };
    bool m_bEnabled = false;
    int m_x = 1, m_y = 0;
    int m_nMaxMessages = kDefaultMaxMessages;
    float m_fMessageTime = kDefaultMessageTime;
    int m_nMessageCount = 0, m_nFirstMessage = 0, m_nNextMessage = 0;
    std::array<Message, kMaxMessages> m_Messages{};
};
