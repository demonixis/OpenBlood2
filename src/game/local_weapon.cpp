#include "game/original/BloodShells.h"
#include "game/original/ViewWeapon.h"
namespace OpenBlood2
{
    namespace
    {
        class LocalWeapon final : public WeaponServices
        {
          public:
            LocalWeapon(ServerDE& engine, CBloodServerShell& server, HCLIENT client, bool left)
                : engine_(engine), server_(server), client_(client), player_(server.Player(client)), left_(left),
                  weapon_(player_ ? player_->m_InventoryMgr.CurrentWeapon(left) : nullptr)
            {
            }
            float time() const override
            {
                return engine_.GetTime();
            }
            bool has_player() const override
            {
                return server_.Player(client_) == player_ && player_;
            }
            bool player_alive() const override
            {
                return has_player() && !player_->m_damage.IsDead();
            }
            bool weapon_selected(DBYTE type, bool left) const override
            {
                return has_player() && weapon_ && left == left_ &&
                       player_->m_InventoryMgr.CurrentWeapon(left) == weapon_ && weapon_->data.m_nType == type;
            }
            float ammunition(bool alternate) const override
            {
                return has_player() && weapon_ ? weapon_->CheckAmmo(alternate) : 0;
            }
            FireResult fire(const WeaponFire& value) override
            {
                return server_.OnMessage(client_, value);
            }
            SoundId play_sound(const SoundRequest& value, std::string& error) override
            {
                return engine_.PlayLocalSound(player_ ? player_->m_hObject : nullptr, value, error);
            }

          private:
            ServerDE& engine_;
            CBloodServerShell& server_;
            HCLIENT client_;
            CPlayerObj* player_;
            bool left_;
            CWeapon* weapon_;
        };
    } // namespace
} // namespace OpenBlood2
bool CViewWeapon::Update(ServerDE& engine, CBloodServerShell& server, HCLIENT client,
                         const OpenBlood2::WeaponInput& input, DVector position, DVector direction, double seconds,
                         std::string& error)
{
    if (!model)
        return true;
    OpenBlood2::LocalWeapon services(engine, server, client, left);
    return Update(services, input, position, direction, seconds, error);
}
