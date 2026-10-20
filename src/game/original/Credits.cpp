#include "Credits.h"
#include <charconv>

namespace
{
    // atof prefix semantics for the ">TIME:" argument: leading whitespace, optional sign,
    // the longest decimal/exponent prefix, 0 when nothing parses.
    float source_atof(std::string_view text)
    {
        std::size_t i = 0;
        while (i < text.size() && (text[i] == ' ' || (text[i] >= '\t' && text[i] <= '\r')))
            ++i;
        bool negative = false;
        if (i < text.size() && (text[i] == '+' || text[i] == '-'))
            negative = text[i++] == '-';
        double value = 0;
        const auto* begin = text.data() + i;
        const auto result = std::from_chars(begin, text.data() + text.size(), value, std::chars_format::general);
        if (result.ec != std::errc{} || (begin < text.data() + text.size() && *begin == '-'))
            return 0.0f;
        return static_cast<float>(negative ? -value : value);
    }
} // namespace

// ----------------------------------------------------------------------- //
//	CCredit::Init: each line of the credit block is a separate string.
// ----------------------------------------------------------------------- //

bool CCredit::Init(CCredits& owner, std::string_view text)
{
    m_pOwner = &owner;
    m_nState = CS_START;

    owner.s_bPause = false;

    std::string sString;
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == '\n' || text[i] == '\r')
        {
            AddString(sString);
            sString.clear();
            while (i < text.size() && (text[i] == '\n' || text[i] == '\r'))
                ++i;
        }
        else
        {
            if (sString.size() >= 255)
            {
                owner.error = "Credits line exceeds the original 256-byte line buffer";
                return false;
            }
            sString += text[i++];
        }
    }
    if (!sString.empty())
        AddString(sString);
    return true;
}

void CCredit::Clear()
{
    m_pOwner = nullptr;
    for (auto& string : m_aStrings)
        string.clear();
    m_cStrings = 0;
    m_nState = CS_START;
    m_fTimer = 0.0f;
    m_fHoldTime = -1.0f;
}

const std::string* CCredit::GetString(int iString) const
{
    if (iString >= MAX_CREDIT_STRINGS || iString < 0)
        return nullptr;
    return &m_aStrings[iString];
}

bool CCredit::AddString(std::string_view sString)
{
    if (!m_pOwner)
        return false;
    if (m_cStrings >= MAX_CREDIT_STRINGS)
    {
        ++m_pOwner->dropped_strings; // Includes later >TIME: lines, which the original also never reads.
        return false;
    }

    // Check if this is a special command string...
    if (sString.starts_with(">TIME:") && sString.size() > 6)
    {
        m_fHoldTime = source_atof(sString.substr(6));
        return true;
    }

    m_aStrings[m_cStrings++] = sString;
    return true;
}

void CCredit::Draw(float fColor)
{
    if (m_cStrings <= 0)
        return;
    m_pOwner->drawn = {this, fColor};
}

float CCredit::Delta(float fFrameTime) const
{
    float fDelta = fFrameTime;
    fDelta *= m_pOwner->GetSpeed();
    if (m_pOwner->IsPaused())
        fDelta = 0;
    return fDelta;
}

bool CCredit::Update(float fFrameTime)
{
    if (!m_pOwner)
        return false;

    switch (m_nState)
    {
        case CS_START:
            SetState(CS_FADEIN);
            return false;
        case CS_FADEIN:
            if (UpdateFadeIn(fFrameTime))
                SetState(CS_HOLDIN);
            return false;
        case CS_HOLDIN:
            if (UpdateHoldIn(fFrameTime))
                SetState(CS_FADEOUT);
            return false;
        case CS_FADEOUT:
            if (UpdateFadeOut(fFrameTime))
                SetState(CS_HOLDOUT);
            return false;
        case CS_HOLDOUT:
            if (UpdateHoldOut(fFrameTime))
                SetState(CS_DONE);
            return false;
        case CS_DONE:
        default:
            return true;
    }
}

void CCredit::SetState(int nState)
{
    m_nState = nState;

    switch (nState)
    {
        case CS_FADEIN:
            m_fTimer = 0.5f;
            m_fColor = 0.0f;
            break;
        case CS_HOLDIN:
            m_fTimer = m_pOwner ? m_pOwner->default_hold : 0.0f;
            m_fColor = 1.0f;
            if (m_fHoldTime > 0.0)
                m_fTimer = m_fHoldTime;
            break;
        case CS_FADEOUT:
            m_fTimer = 0.400f;
            m_fColor = 1.0f;
            break;
        case CS_HOLDOUT:
            m_fTimer = m_pOwner ? m_pOwner->default_out : 0.0f;
            m_fColor = 1.0f;
            break;
        default:
            break;
    }

    m_fTimerStart = m_fTimer;
    m_fColorStart = m_fColor;
}

bool CCredit::UpdateFadeIn(float fFrameTime)
{
    const float fDelta = Delta(fFrameTime);
    if (m_fTimer > fDelta)
        m_fTimer -= fDelta;
    else
        m_fTimer = 0.0f;

    m_fColor = 1.0f - (m_fTimer / m_fTimerStart);
    if (m_fColor > 1.0f)
        m_fColor = 1.0f;

    Draw(m_fColor);
    return m_fTimer == 0.0f;
}

bool CCredit::UpdateHoldIn(float fFrameTime)
{
    const float fDelta = Delta(fFrameTime);
    if (m_fTimer > fDelta)
        m_fTimer -= fDelta;
    else
        m_fTimer = 0.0f;

    m_fColor = 1.0f;
    Draw(m_fColor);
    return m_fTimer == 0.0f;
}

bool CCredit::UpdateFadeOut(float fFrameTime)
{
    const float fDelta = Delta(fFrameTime);
    if (m_fTimer > fDelta)
        m_fTimer -= fDelta;
    else
        m_fTimer = 0.0f;

    m_fColor = m_fTimer / m_fTimerStart;
    if (m_fColor < 0.0f)
        m_fColor = 0.0f;
    // Over the world the fade stops at 10% (black would be the transparent key).
    if (!m_pOwner->IsClearingScreen() && m_fColor < 0.1f)
    {
        m_fColor = 0.1f;
        m_fTimer = 0.0f;
    }

    Draw(m_fColor);
    return m_fTimer == 0.0f;
}

bool CCredit::UpdateHoldOut(float fFrameTime)
{
    const float fDelta = Delta(fFrameTime);
    if (m_fTimer > fDelta)
        m_fTimer -= fDelta;
    else
        m_fTimer = 0.0f;
    return m_fTimer == 0.0f;
}

// ----------------------------------------------------------------------- //
//	CCredits
// ----------------------------------------------------------------------- //

bool CCredits::Init(std::span<const std::uint8_t> text, int nMode, bool bClearScreen, unsigned screen_width,
                    std::string* out_error)
{
    Term();

    m_iCredit = 0;
    m_nMode = nMode;
    m_bDone = false;
    m_bClearScreen = bClearScreen;
    s_fSpeed = DEF_CREDITS_SPEED;

    // Original SetStaticInfo: font size from the screen width, mode hold times.
    font_width = 8;
    font_height = 14;
    if (screen_width > 360)
    {
        font_width = 10;
        font_height = 16;
    }
    if (screen_width > 520)
    {
        font_width = 14;
        font_height = 22;
    }
    font_spacing = 4;
    if (nMode == CM_CREDITS)
    {
        default_hold = 4.0f;
        default_out = 2.0f;
    }
    if (nMode == CM_INTRO)
    {
        default_hold = 4.0f;
        default_out = 1.2f;
    }

    const bool ok = AddCredits(text);
    if (!ok)
    {
        // Bounded failure keeps no partial roll: an inited, empty CCredits draws nothing.
        for (auto& credit : m_aCredits)
            credit.Term();
        m_cCredits = 0;
    }
    m_bInited = true;
    if (out_error)
        *out_error = error;
    return ok;
}

void CCredits::Term()
{
    for (auto& credit : m_aCredits)
        credit.Term();
    Clear();
}

void CCredits::Clear()
{
    m_bInited = false;
    m_bDone = true;
    m_bClearScreen = true;
    m_cCredits = 0;
    m_iCredit = 0;
    m_nMode = CM_CREDITS;
    s_fSpeed = DEF_CREDITS_SPEED;
    drawn = {};
    font_name = "Arial";
    updates = dropped_credits = dropped_strings = 0;
    error.clear();
}

const CCredit* CCredits::GetCredit(int iCredit) const
{
    if (iCredit >= MAX_CREDITS || iCredit < 0)
        return nullptr;
    return &m_aCredits[iCredit];
}

CCredit* CCredits::GetCredit(int iCredit)
{
    if (iCredit >= MAX_CREDITS || iCredit < 0)
        return nullptr;
    return &m_aCredits[iCredit];
}

void CCredits::HandleInput(int vkey)
{
    switch (vkey)
    {
        case CREDITS_VK_UP:
            if (!IsIntro())
                IncSpeed();
            break;
        case CREDITS_VK_DOWN:
            if (!IsIntro())
                DecSpeed();
            break;
        case CREDITS_VK_HOME:
            if (!IsIntro())
                s_fSpeed = 1.0;
            break;
        case CREDITS_VK_RETURN:
        case CREDITS_VK_SPACE:
        case CREDITS_VK_NEXT:
            if (IsIntro())
            {
                if (auto* pCredit = GetCredit(m_iCredit); pCredit && pCredit->GetState() == CS_HOLDIN)
                    pCredit->SetState(CS_FADEOUT);
            }
            else
                AdvanceCredit(CS_HOLDIN);
            break;
        case CREDITS_VK_PAUSE:
            if (!IsIntro())
                s_bPause ^= 1;
            break;
        case CREDITS_VK_ESCAPE:
            ExitToMainMenu();
            break;
        default:
            break;
    }
}

void CCredits::Update(float fFrameTime)
{
    if (m_cCredits <= 0)
        return;
    ++updates;
    drawn = {}; // The original clears or redraws the screen every frame.

    auto* pCredit = GetCredit(m_iCredit);
    if (!pCredit)
        return;
    if (pCredit->Update(fFrameTime))
        AdvanceCredit();
}

void CCredits::AdvanceCredit(int nState)
{
    m_iCredit++;
    if (m_iCredit >= m_cCredits)
    {
        m_iCredit = 0;
        if (IsIntro())
            m_bDone = true;
    }
    if (auto* pCredit = GetCredit(m_iCredit))
        pCredit->SetState(nState);
}

// Original AddCredits: blocks separated by "##", leading newlines skipped, ">END" stops.
// The resource is read up to its size or first NUL (LockResource text had a terminator).
bool CCredits::AddCredits(std::span<const std::uint8_t> text)
{
    std::string sCredit;
    std::size_t i = 0;
    while (i < text.size() && text[i])
    {
        if (text[i] == '#' && i + 1 < text.size() && text[i + 1] == '#')
        {
            if (sCredit.starts_with(">END"))
                return true;

            AddCredit(sCredit);
            if (!error.empty())
                return false;
            sCredit.clear();

            i += 2;
            while (i < text.size() && text[i] && (text[i] == '\n' || text[i] == '\r'))
                ++i;
        }
        else
        {
            if (sCredit.size() >= 1023)
            {
                error = "Credits block exceeds the original 1024-byte credit buffer";
                return false;
            }
            sCredit += static_cast<char>(text[i++]);
        }
    }
    return true;
}

void CCredits::AddCredit(std::string_view sText)
{
    auto* pCredit = GetCredit(m_cCredits);
    if (!pCredit)
    {
        ++dropped_credits;
        return;
    }
    if (!pCredit->Init(*this, sText))
        return;
    m_cCredits++;
}
