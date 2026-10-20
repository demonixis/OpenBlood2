#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// Original ClientShellDLL/Credits.cpp CCredit/CCredits text roll. Strings are owned
// copies of the cres.dll TEXT resource instead of engine HSTRINGs; the frame time is an
// explicit argument and the static font/timer state lives in the CCredits owner. Drawing
// records the last drawn credit for the frontend; see menu_contracts.md#credits-entry.

#define MAX_CREDIT_STRINGS 12

#define MAX_CREDITS 256

#define ADJ_CREDITS_SPEED 0.2f
#define MAX_CREDITS_SPEED 5.0f
#define MIN_CREDITS_SPEED 0.4f
#define DEF_CREDITS_SPEED 1.0f

#define CS_START 0
#define CS_FADEIN 1
#define CS_HOLDIN 2
#define CS_FADEOUT 3
#define CS_HOLDOUT 4
#define CS_DONE 5

#define CM_CREDITS 0
#define CM_INTRO 1

// Windows virtual-key codes received by the original CCredits::HandleInput.
inline constexpr int CREDITS_VK_RETURN = 0x0d, CREDITS_VK_PAUSE = 0x13, CREDITS_VK_ESCAPE = 0x1b,
                     CREDITS_VK_SPACE = 0x20, CREDITS_VK_NEXT = 0x22, CREDITS_VK_HOME = 0x24, CREDITS_VK_UP = 0x26,
                     CREDITS_VK_DOWN = 0x28;

class CCredits;

class CCredit
{
  public:
    bool Init(CCredits& owner, std::string_view text);
    void Term()
    {
        Clear();
    }
    void Clear();

    int GetNumStrings() const
    {
        return m_cStrings;
    }
    const std::string* GetString(int iString) const;
    int GetState() const
    {
        return m_nState;
    }
    float GetHoldTime() const
    {
        return m_fHoldTime;
    }
    float GetTimer() const
    {
        return m_fTimer;
    }

    void SetState(int nState);

    // Returns true to advance to the next credit (original DTRUE).
    bool Update(float fFrameTime);
    bool UpdateFadeIn(float fFrameTime);
    bool UpdateHoldIn(float fFrameTime);
    bool UpdateFadeOut(float fFrameTime);
    bool UpdateHoldOut(float fFrameTime);

  private:
    bool AddString(std::string_view sString);
    void Draw(float fColor);
    float Delta(float fFrameTime) const;

    CCredits* m_pOwner = nullptr;
    std::array<std::string, MAX_CREDIT_STRINGS> m_aStrings;
    int m_cStrings = 0;
    int m_nState = CS_START;
    float m_fTimer = 0.0f;
    float m_fColor = 0.0f;
    float m_fTimerStart = 0.0f;
    float m_fColorStart = 0.0f;
    float m_fHoldTime = -1.0f;
};

class CCredits
{
  public:
    // Original Init(pClientDE, pShell, nMode, bClearScreen) with the TEXT resource bytes
    // and the screen width that selects the font size. The roll is inited even when the
    // text cannot be used (zero credits, like the original); false reports that error.
    bool Init(std::span<const std::uint8_t> text, int nMode = CM_CREDITS, bool bClearScreen = true,
              unsigned screen_width = 640, std::string* error = nullptr);
    void Term();
    void Clear();

    bool IsInited() const
    {
        return m_bInited;
    }
    bool IsDone() const
    {
        return m_bDone;
    }
    bool IsIntro() const
    {
        return m_nMode == CM_INTRO;
    }
    bool IsClearingScreen() const
    {
        return m_bClearScreen;
    }

    const CCredit* GetCredit(int iCredit) const;
    CCredit* GetCredit(int iCredit);
    int GetNumCredits() const
    {
        return m_cCredits;
    }
    int GetCurrentCredit() const
    {
        return m_iCredit;
    }
    int GetMode() const
    {
        return m_nMode;
    }

    void HandleInput(int vkey);
    void ExitToMainMenu()
    {
        m_bDone = true;
    }

    void Update(float fFrameTime);

    void IncSpeed()
    {
        if (s_fSpeed < MAX_CREDITS_SPEED)
            s_fSpeed += ADJ_CREDITS_SPEED;
    }
    void DecSpeed()
    {
        if (s_fSpeed > MIN_CREDITS_SPEED)
            s_fSpeed -= ADJ_CREDITS_SPEED;
    }
    float GetSpeed() const
    {
        return s_fSpeed;
    }
    bool IsPaused() const
    {
        return s_bPause;
    }

    // Original SetStaticInfo font request: IDS_CREDITSFONT face (Arial fallback), CreateFont
    // width/height and line spacing. The frontend substitutes its own font (see contracts).
    std::string font_name = "Arial";
    int font_width = 14, font_height = 22, font_spacing = 4;
    float default_hold = 2.0f, default_out = 0.6f;

    // Last CCredit::Draw of the most recent Update: the frontend renders only this.
    struct Drawn
    {
        const CCredit* credit = nullptr;
        float color = 0; // Red intensity of SetupColor1(m_fColor, 0, 0).
    } drawn;
    std::uint64_t updates = 0, dropped_credits = 0, dropped_strings = 0;
    std::string error;

  private:
    friend class CCredit;
    bool AddCredits(std::span<const std::uint8_t> text);
    void AddCredit(std::string_view sText);
    void AdvanceCredit(int nState = CS_START);

    int m_nMode = CM_CREDITS;
    bool m_bDone = true;
    bool m_bInited = false;
    bool m_bClearScreen = true;
    std::array<CCredit, MAX_CREDITS> m_aCredits;
    int m_cCredits = 0;
    int m_iCredit = 0;

    float s_fSpeed = DEF_CREDITS_SPEED;
    bool s_bPause = false;
};

namespace OpenBlood2
{
    // Original CCredit::Draw/DrawString vertical arithmetic: center y of string `index`
    // of `count`, then the top edge for a string of height `string_height`.
    inline int credit_line_center(int y, int count, int index, int font_height, int font_spacing)
    {
        const int line = font_height + font_spacing;
        const int yTop = y - (count * line) / 2;
        return yTop + line / 2 + index * line;
    }
} // namespace OpenBlood2
