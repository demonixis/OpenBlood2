#pragma once
// DLink and the three helpers used by SmellHint, selected from the original
// AppHeaders/dlink.h. Unused aliasing casts/macros are deliberately not imported.
struct DLink { DLink* m_pPrev = nullptr; DLink* m_pNext = nullptr; void* m_pData = nullptr; };
inline void dl_TieOff(DLink* pLink) { pLink->m_pNext = pLink->m_pPrev = pLink; }
inline void dl_Insert(DLink* pAfter, DLink* pLink) {
    pLink->m_pPrev = pAfter;
    pLink->m_pNext = pAfter->m_pNext;
    pLink->m_pPrev->m_pNext = pLink->m_pNext->m_pPrev = pLink;
}
inline void dl_Remove(DLink* pLink) {
    pLink->m_pPrev->m_pNext = pLink->m_pNext;
    pLink->m_pNext->m_pPrev = pLink->m_pPrev;
}
