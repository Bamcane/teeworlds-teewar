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
    CPlayer *pPlayer = GameServer()->m_apPlayers[From];
    if(pPlayer && pPlayer->GetTeam() == m_Team)
        return;

    if((From == CLIENTID_BLUE && m_Team) || (From == CLIENTID_RED && !m_Team))
        return;

    m_TowerHealth -= Damage;

    GameServer()->SendDamageSound(From);

    if(m_TowerHealth <= 0)
    {
        m_DestoryTick = Server()->TickSpeed();
        GameServer()->SendEmoticon(From, EMOTICON_EYES);
        CCharacter *pChr = GameServer()->GetPlayerChar(From);
        if(pChr)
        {
            pChr->SetEmote(EMOTE_HAPPY, Server()->Tick()+m_DestoryTick);
        }
        GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_PL);
        return;
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

void CTower::UpdateState()
{
    m_TowerState = TOWERSTATE_ARMOR;

    if(m_LaserArmor)
    {
        m_TowerState |= TOWERSTATE_LASER;
    }
}

void CTower::OnDestory()
{
    if(m_TowerHealth > 0 || m_DestoryTick <= 0)
        return;

    m_DestoryTick--;
    if(random_prob(0.4f))
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
}

void CTower::OnNormal()
{
    if(m_TowerHealth <= 0)
        return;

    if(m_LaserArmor && Server()->Tick() % (Server()->TickSpeed() * 20) == 0)
        m_LaserArmor--;
}

void CTower::DoFix(int FixType, CPlayer *pFrom)
{
    switch (FixType)
    {
    case FIXTYPE_UPGRADE:
        {
            pFrom->GetCharacter()->m_Armor--;
            m_GiveArmor++;
            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
            if(m_GiveArmor >= 10)
            {
                m_LaserArmor += 2;
                m_GiveArmor = 0;
                GameServer()->CreateSound(m_Pos, SOUND_RIFLE_FIRE);
                GameServer()->SendChatTarget(pFrom->GetCID(), _("Laser Armor: {int:Num}"), "Num", &m_LaserArmor, NULL);
            }
        } break;
    case FIXTYPE_HAMMERFIX:
        {
            int Health = g_Config.m_WarHammerFixHealth;
            TakeFix(Health, pFrom->GetCID());
        }break;
    case FIXTYPE_ATTACK:
        {
            if(m_LaserArmor)
            {
                m_GiveArmor--;
                if(m_GiveArmor < -5)
                {
                    m_LaserArmor--;
                    m_GiveArmor = 0;
                    GameServer()->SendChatTarget(pFrom->GetCID(), _("Laser Armor: {int:Num}"), "Num", &m_LaserArmor, NULL);
                    for(int i = 0; i < MAX_CLIENTS; i ++)
                    {
                        if(!GameServer()->m_apPlayers[i])
                            continue;

                        if(GameServer()->m_apPlayers[i]->GetTeam() == m_Team && GameServer()->m_apPlayers[i]->GetRole() == ROLE_ENGINEER)
                        {
                            GameServer()->SendBroadcast_VL(_("Laser Armor: {int:Num}"), pFrom->GetCID(), "Num", &m_LaserArmor, NULL);
                        }   
                    }
                }
            }else
            {
                TakeDamage(2, pFrom->GetCID());
            }
        }break;
    }
}

void CTower::Tick()
{
    UpdateState();

    OnDestory();
    
    OnNormal();
}

void CTower::Reset()
{
    m_TowerState = TOWERSTATE_ARMOR;
    m_TowerHealth = g_Config.m_WarTowerHealth;

    m_LaserArmor = 0;
    m_GiveArmor = 0;
    
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

        if(m_TowerState&TOWERSTATE_LASER)
        {
            vec2 To = m_Pos + (GetDir((Degres - 360 / NUM_ARMORS)*pi/180) * Radius);
            CNetObj_Laser *pLaser = (CNetObj_Laser *)Server()->SnapNewItem(NETOBJTYPE_LASER, m_ArmorIDs[i], sizeof(CNetObj_Laser));
            if(!pLaser)
                return;
            
            pLaser->m_X = Pos.x;
            pLaser->m_Y = Pos.y;
            pLaser->m_FromX = To.x;
            pLaser->m_FromY = To.y;
            pLaser->m_StartTick = Server()->Tick();
        }else
        {
            CNetObj_Pickup *pArmor = (CNetObj_Pickup *)Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_ArmorIDs[i], sizeof(CNetObj_Pickup));
            if(!pArmor)
                return;
            
            pArmor->m_X = Pos.x;
            pArmor->m_Y = Pos.y;
            pArmor->m_Type = POWERUP_ARMOR;
            pArmor->m_Subtype = 0;
        }

        Degres -= 360 / NUM_ARMORS;
    }

}
