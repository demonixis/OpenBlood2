#include "InventoryMgr.h"
#include <algorithm>
#include <cmath>
namespace {
constexpr float fMaxAmmoCount[13][6] = 
{
// Strength or Magic attrib:
//   1		2		3		4		5		6
	{0.0f,  0.0f,   0.0f,   0.0f,   0.0f,   0.0f},		// None
	{100.0f,200.0f, 300.0f, 400.0f, 500.0f, 600.0f},	// Bullets
	{50.0f,	75.0f,  100.0f, 125.0f, 150.0f, 175.0f},	// Shells
	{20.0f,	40.0f,  60.0f,  80.0f,  100.0f, 120.0f},	// BMG Rounds
	{20.0f,	40.0f,  60.0f,  80.0f,  100.0f, 120.0f},	// Flares
	{25.0f,	50.0f,  75.0f,  100.0f, 150.0f, 200.0f},	// DieBugDie
	{5.0f,	10.0f,	20.0f,	35.0f,	50.0f,	120.0f},	// Rockets
	{25.0f,	25.0f,	50.0f,	75.0f,	100.0f,	125.0f},	// Fuel
	{100.0f,200.0f, 300.0f, 400.0f, 500.0f, 600.0f},	// Batteries
	{100.0f,200.0f, 300.0f, 400.0f, 500.0f, 600.0f},	// Focus
	{10.0f, 10.0f,  10.0f,  10.0f,  10.0f,  10.0f},		// ProximityBomb
	{10.0f, 10.0f,  10.0f,  10.0f,  10.0f,  10.0f},		// RemoteBomb
	{10.0f, 10.0f,  10.0f,  10.0f,  10.0f,  10.0f},		// TimeBomb
};
}
bool CInventoryMgr::Init(HOBJECT object,HCLIENT client) {
    auto* server = BaseClass::GetServerDE(); m_hOwner = object;
    if (!m_hMuzzleFlash) {
        ObjectCreateStruct state; state.m_ObjectType = OT_LIGHT;
        if (auto* flash = server->CreateObject(server->GetClass("BaseClass"),&state)) {
            m_hMuzzleFlash = flash->m_hObject;
            if (server->CreateInterObjectLink(m_hOwner,m_hMuzzleFlash) != DE_OK) return false;
        } else return false;
    }
    if (client) SetClient(client);
    return true;
}
CInventoryMgr::~CInventoryMgr() {
    if (m_hMuzzleFlash) BaseClass::GetServerDE()->RemoveObject(m_hMuzzleFlash);
}
DDWORD CInventoryMgr::EngineMessageFn(LPBASECLASS object,DDWORD message,void* data,DFLOAT fData) {
    if (message == MID_LINKBROKEN && data == m_hMuzzleFlash) {
        // The engine notifies on either endpoint's removal. Release this owned
        // child before clearing the handle, including when the owner is pending.
        BaseClass::GetServerDE()->RemoveObject(m_hMuzzleFlash); m_hMuzzleFlash = nullptr;
    }
    return Aggregate::EngineMessageFn(object,message,data,fData);
}
void CInventoryMgr::SetAmmoCount(DBYTE type,float count) {
    if (type == 0 || type > 12 || m_bInfiniteAmmo || !std::isfinite(count) || count < 0) return;
    m_fAmmo[type] = count;
    ++revision;
}
float CInventoryMgr::MaxAmmo(DBYTE type) const {
    if (type==0 || type>12) return 0;
    auto attribute=type==9 ? m_nAttribMagic : m_nAttribStrength;
    return fMaxAmmoCount[type][std::clamp<int>(attribute,1,6)-1];
}
void CInventoryMgr::SetStrength(DBYTE strength) {
    strength=std::clamp<DBYTE>(strength,1,6);
    if (m_nAttribStrength>strength) for (DBYTE i=1;i<=8;++i) m_fAmmo[i]=std::min(m_fAmmo[i],fMaxAmmoCount[i][strength-1]);
    m_nAttribStrength=strength; ++revision;
}
void CInventoryMgr::SetMagic(DBYTE magic) {
    magic=std::clamp<DBYTE>(magic,1,6);
    if (m_nAttribMagic>magic) m_fAmmo[9]=std::min(m_fAmmo[9],fMaxAmmoCount[9][magic-1]);
    m_nAttribMagic=magic; ++revision;
}
InventoryResult CInventoryMgr::AddAmmo(DBYTE type,float amount) {
    if (type==0 || type>12 || !std::isfinite(amount) || amount<0) return InventoryResult::Invalid;
    if (m_fAmmo[type]>=MaxAmmo(type)) return InventoryResult::Full;
    m_fAmmo[type]=std::min(m_fAmmo[type]+amount,MaxAmmo(type)); ++revision; return InventoryResult::Ok;
}
InventoryResult CInventoryMgr::ObtainWeapon(DBYTE type,int slot) {
    const auto* definition=weapon_definition(type);
    if (!definition) return InventoryResult::NotAvailable;
    if (!m_hOwner || !BaseClass::GetServerDE()->HandleToObject(m_hOwner) || slot < -1 || slot>=10) return InventoryResult::Invalid;
    int empty=-1; bool left=false;
    if (slot>=0) {
        auto* weapon=GetWeapon(static_cast<unsigned>(slot));
        if (!weapon) empty=slot;
        else if (weapon->data.m_nType==type) {
            if (definition->m_dwTwoHandStrengthReq && !GetWeapon(static_cast<unsigned>(slot),true)) { empty=slot; left=true; }
            else return InventoryResult::AlreadyHave;
        }
    }
    if (empty<0) for (unsigned i=0;i<10;++i) {
        auto* weapon=GetWeapon(i);
        if (!weapon && empty<0) empty=static_cast<int>(i);
        else if (weapon && weapon->data.m_nType==type) {
            if (definition->m_dwTwoHandStrengthReq && !GetWeapon(i,true)) { empty=static_cast<int>(i); left=true; break; }
            return InventoryResult::AlreadyHave;
        }
    }
    if (empty<0) return InventoryResult::Full;
    // Original strength gate was commented out; do not introduce a new one.
    std::unique_ptr<CWeapon> weapon;
    if (type==WEAP_BERETTA) weapon=std::make_unique<CBeretta>();
    else weapon=std::make_unique<CMelee>();
    weapon->Init(m_hOwner,this,left);
    (left ? m_LWeapons : m_Weapons)[static_cast<unsigned>(empty)]=std::move(weapon);
    ++revision; return InventoryResult::Ok;
}
InventoryResult CInventoryMgr::ChangeWeaponSlot(DBYTE slot) {
    auto* weapon=GetWeapon(slot); if (!weapon) return InventoryResult::NotAvailable;
    if (CurrentWeapon() && CurrentWeapon()->data.m_nType==weapon->data.m_nType) return InventoryResult::Current;
    m_nCurWeaponSlot=slot; ++revision; return InventoryResult::Ok;
}
FireResult CInventoryMgr::FireCurrentWeapon(const blood2::WeaponFire& request) {
    auto* weapon=CurrentWeapon(request.left);
    if (!weapon || weapon->data.m_nType!=request.weapon) return FireResult::Invalid;
    return weapon->Fire(request);
}

void CInventoryMgr::ClearWeapons() {
    for (auto& weapon:m_Weapons) weapon.reset();
    for (auto& weapon:m_LWeapons) weapon.reset();
    m_nCurWeaponSlot=0; requested_weapons={}; ++revision;
}
