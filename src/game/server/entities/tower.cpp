/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>

#include "tower.h"

CTower::CTower(CGameWorld *pGameWorld, vec2 Pos, int Team)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOWER)
{
	m_Team = Team;
    m_Pos = Pos;
    m_LastWarningTick = 0;

    Reset();

	GameWorld()->InsertEntity(this);

    for(int i = 0; i<NUM_ARMORS; i++)
	{
		m_ArmorIDs[i] = Server()->SnapNewID();
	}
}

CTower::~CTower()
{
    for(int i = 0; i<NUM_ARMORS; i++)
	{
		Server()->SnapFreeID(m_ArmorIDs[i]);
	}
}

void CTower::TakeDamage(int Damage, int From)
{
    m_TowerHealth -= Damage;

    if(m_TowerHealth <= 0)
    {
        m_DestoryTick = Server()->TickSpeed();
        GameServer()->SendEmoticon(From, EMOTICON_EYES);
        CCharacter *pChr = GameServer()->GetPlayerChar(From);
        if(pChr)
        {
            pChr->SetEmote(EMOTE_HAPPY, Server()->Tick()+m_DestoryTick);
        }
        return;
    }

    int Health = m_TowerHealth * 100 / g_Config.m_WarTowerHealth;
    if(Health < 5 && m_LastWarningTick + Server()->TickSpeed()/2 <= Server()->Tick())
    {
        if(m_Team)
        {
            GameServer()->SendChatTarget(-1, 
                _("| Warning:Blue tower is only {int:Health}% health!"), "Health", &Health);
        }else 
        {
            GameServer()->SendChatTarget(-1, 
                _("| Warning:Red tower is only {int:Health}% health!"), "Health", &Health);
        }
        m_LastWarningTick = Server()->Tick();
    }
    return;
}

void CTower::TakeFix(int Health, int From)
{
    m_TowerHealth = min(g_Config.m_WarTowerHealth, m_TowerHealth + Health);
    GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP);
    CCharacter *pChr = GameServer()->GetPlayerChar(From);
    if(pChr)
        pChr->m_LastFixTick = Server()->Tick();
    return;
}

void CTower::Tick()
{
    if(m_TowerHealth <= 0 && m_DestoryTick > 0)
    {
        m_DestoryTick--;
        if(random_prob(0.1f))
        {
            float RandomRadius = random_float()*(m_ProximityRadius-4.0f);
			float RandomAngle = 2.0f * pi * random_float();
			vec2 BoomPos = m_Pos + vec2(RandomRadius * cos(RandomAngle), RandomRadius * sin(RandomAngle));

            GameServer()->CreateExplosion(BoomPos, -1, WEAPON_GRENADE, true);
            GameServer()->CreateSound(BoomPos, SOUND_GRENADE_EXPLODE);
        }

        if(random_prob(0.1f))
        {
            float RandomRadius = random_float()*(m_ProximityRadius-4.0f);
			float RandomAngle = 2.0f * pi * random_float();
			vec2 SmokePos = m_Pos + vec2(RandomRadius * cos(RandomAngle), RandomRadius * sin(RandomAngle));

            GameServer()->CreatePlayerSpawn(SmokePos);
        }
        m_ProximityRadius -= ms_PhysSize / (float)Server()->TickSpeed();
    }else if(m_TowerHealth)
    {
        CCharacter *apEnts[MAX_CLIENTS];
        int Num = GameServer()->m_World.FindEntities(m_Pos, m_ProximityRadius + 28.0f, (CEntity**)apEnts,
                                                    MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

        for (int i = 0; i < Num; ++i)
        {
            CCharacter *pTarget = apEnts[i];
            CNetObj_PlayerInput Input = pTarget->GetInput();

            if(Input.m_Fire&1 && (pTarget->m_LastFixTick + g_Config.m_WarHammerFixTimer <= Server()->Tick())
                && pTarget->GetActiveWeapon() == WEAPON_HAMMER && 
                    pTarget->GetPlayer()->GetTeam() == m_Team)
                TakeFix(g_Config.m_WarHammerFixHealth, pTarget->GetPlayer()->GetCID());
        }
    }
}

void CTower::Reset()
{
    m_TowerHealth = g_Config.m_WarTowerHealth;
	m_ProximityRadius = ms_PhysSize;
    m_DestoryTick = -1;
}

void CTower::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || (m_TowerHealth <= 0 && m_DestoryTick == 0))
		return;

	CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG, m_Team, sizeof(CNetObj_Flag));
	if(!pFlag)
		return;

	pFlag->m_X = (int)m_Pos.x;
	pFlag->m_Y = (int)m_Pos.y;
	pFlag->m_Team = m_Team;

    float Radius = m_ProximityRadius;
    int Degres = 0;

    for(int i = 0; i < NUM_ARMORS; i ++)
    {
		vec2 Pos = m_Pos + (GetDir(Degres*pi/180) * Radius);

        CNetObj_Pickup *pArmor = (CNetObj_Pickup *)Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ArmorIDs[i], sizeof(CNetObj_Pickup));
        if(!pArmor)
            return;
        
        pArmor->m_X = Pos.x;
        pArmor->m_Y = Pos.y;
        pArmor->m_Type = POWERUP_ARMOR;
        pArmor->m_Subtype = 0;

        Degres -= 360 / NUM_ARMORS;
    }

}
