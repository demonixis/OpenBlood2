// Selective original CMessageMgr (ClientShellDLL/MessageMgr.cpp:220-452): Enable, AddLine,
// Draw, Clear, SetMaxMessages. Fonts/surfaces are the application's; see MessageMgr.h.
#include "game/original/MessageMgr.h"

void CMessageMgr::Enable(bool bEnabled)
{
    if (m_bEnabled && !bEnabled)
        Clear();

    m_bEnabled = bEnabled;
}
void CMessageMgr::AddLine(std::string_view szMsg, float time, const OpenBlood2::ConsoleTextSource& source)
{
    Message* pMsg = &m_Messages[m_nNextMessage];

    // Set next Message pointer
    m_nNextMessage = (m_nNextMessage + 1) % kMaxMessages;

    // don't overflow the maximum number of Messages
    if (++m_nMessageCount > m_nMaxMessages)
    {
        // expire the oldest Message and recalculate the first Message in the buffer
        Message* pOld = &m_Messages[m_nFirstMessage];
        *pOld = {};

        m_nFirstMessage = (m_nFirstMessage + 1) % kMaxMessages;

        // reset the Message count to max
        m_nMessageCount = m_nMaxMessages;
    }

    pMsg->text = std::string(szMsg);
    pMsg->source = source;
    pMsg->fExpiration = time + m_fMessageTime;
    ++added;
}
std::vector<CMessageMgr::Line> CMessageMgr::Draw(float time, int* input_y)
{
    std::vector<Line> out;
    if (input_y)
        *input_y = m_y;
    if (!m_bEnabled)
        return out;

    // early return if there are no Messages in the queue (the editor line is the caller's)
    if (m_nMessageCount == 0)
        return out;

    int nCount = m_nMessageCount; // use temporary values
    int nFirst = m_nFirstMessage; // because the real ones may change
    int y = m_y;

    for (int i = 0; i < nCount; i++)
    {
        Message* pMsg = &m_Messages[(nFirst + i) % kMaxMessages];

        // expire Messages that have overstayed their welcome
        if (time >= pMsg->fExpiration)
        {
            *pMsg = {};
            // expire the oldest Message and recalculate the Message position and counter
            m_nFirstMessage = (m_nFirstMessage + 1) % kMaxMessages;
            m_nMessageCount--;
            ++expired;

            // don't draw the expired Message
            continue;
        }

        // draw the text
        out.push_back({pMsg->text, m_x, y, pMsg->source});
        y += kFontHeight;
    }
    if (input_y)
        *input_y = y;
    return out;
}
void CMessageMgr::Clear()
{
    for (int i = 0; i < m_nMessageCount; i++)
        m_Messages[(m_nFirstMessage + i) % kMaxMessages] = {};

    m_nFirstMessage = m_nNextMessage = m_nMessageCount = 0;
}
void CMessageMgr::SetMaxMessages(int nMaxMessages)
{
    if (nMaxMessages < 1)
        nMaxMessages = 1;
    if (nMaxMessages > kMaxMessages)
        nMaxMessages = kMaxMessages;

    m_nMaxMessages = nMaxMessages;
}
std::vector<std::string> CMessageMgr::lines() const
{
    std::vector<std::string> out;
    for (int i = 0; i < m_nMessageCount; i++)
        out.push_back(m_Messages[(m_nFirstMessage + i) % kMaxMessages].text);
    return out;
}
