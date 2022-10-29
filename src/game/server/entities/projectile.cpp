/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/generated/protocol.h>
#include <game/server/gamecontext.h>
#include "projectile.h"

CProjectile::CProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, bool Explosive, float Force, int SoundImpact, int Weapon)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Type = Type;
	m_Pos = Pos;
	m_Direction = Dir;
	m_StartLifeSpan = Span;
	m_LifeSpan = Span;
	m_Owner = Owner;
	m_Team = GameServer()->m_apPlayers[Owner]->GetTeam();
	m_Force = Force;
	m_Damage = Damage;
	m_SoundImpact = SoundImpact;
	m_Weapon = Weapon;
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

	switch(m_Type)
	{
		case WEAPON_GRENADE:
			Curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			Speed = GameServer()->Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			Curvature = GameServer()->Tuning()->m_ShotgunCurvature;
			Speed = GameServer()->Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			Curvature = GameServer()->Tuning()->m_GunCurvature;
			Speed = GameServer()->Tuning()->m_GunSpeed;
			break;
	}

	return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CProjectile::Tick()
{
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	int Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, &CurPos, 0);
	CCharacter *OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *TargetChr = GameServer()->m_World.IntersectCharacter(PrevPos, CurPos, 6.0f, CurPos, OwnerChar);

	CTower *TargetTower = GameServer()->m_World.IntersectTower(PrevPos, CurPos, 6.0f, CurPos, m_Team);

	m_LifeSpan--;

	if(TargetChr || TargetTower || Collide || m_LifeSpan < 0 || GameLayerClipped(CurPos))
	{
		if(TargetTower && TargetTower->m_TowerState&TOWERSTATE_LASER)
		{
			DoBounce();
			TargetTower->m_LaserArmor--;
		}
		else 
		{
			if(m_LifeSpan >= 0 || m_Weapon == WEAPON_GRENADE)
				GameServer()->CreateSound(CurPos, m_SoundImpact);

			if(m_Explosive)
			GameServer()->CreateExplosion(CurPos, m_Owner, m_Weapon, false);

			else if(TargetChr)
				TargetChr->TakeDamage(m_Direction * max(0.001f, m_Force), m_Damage, m_Owner, m_Weapon);

			else if(TargetTower)
				TargetTower->TakeDamage(m_Damage, m_Owner);

			GameServer()->m_World.DestroyEntity(this);
		}
	}
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::DoBounce()
{
	float Pt = (Server()->Tick()-m_StartTick-1)/(float)Server()->TickSpeed();
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	vec2 CollisionPos;
	CollisionPos.x = PrevPos.x;
	CollisionPos.y = CurPos.y;
	int CollideY = GameServer()->Collision()->IntersectLine(PrevPos, CollisionPos, NULL, NULL);
	CollisionPos.x = CurPos.x;
	CollisionPos.y = PrevPos.y;
	int CollideX = GameServer()->Collision()->IntersectLine(PrevPos, CollisionPos, NULL, NULL);
	
	m_Pos = PrevPos;
	vec2 vel;
	float Curvature, Speed;
	switch (m_Type)
	{
		case WEAPON_GUN: Curvature = GameServer()->Tuning()->m_GunCurvature; Speed = GameServer()->Tuning()->m_GunSpeed; break;
		case WEAPON_SHOTGUN: Curvature = GameServer()->Tuning()->m_ShotgunCurvature; Speed = GameServer()->Tuning()->m_ShotgunSpeed; break;
		case WEAPON_GRENADE: Curvature = GameServer()->Tuning()->m_GrenadeCurvature; Speed = GameServer()->Tuning()->m_GrenadeSpeed; break;
	}
	vel.x = m_Direction.x;
	vel.y = m_Direction.y + 2*Curvature/10000*Ct*Speed;

	if (CollideX && !CollideY)
	{
		m_Direction.x = -vel.x;
		m_Direction.y = vel.y;
	}
	else if (!CollideX && CollideY)
	{
		m_Direction.x = vel.x;
		m_Direction.y = -vel.y;
	}
	else
	{
		m_Direction.x = -vel.x;
		m_Direction.y = -vel.y;
	}
	m_LifeSpan = m_StartLifeSpan/2;
	m_StartLifeSpan /= 2;
	
	m_Direction.x *= (100 - 50) / 100.0;
	m_Direction.y *= (100 - 50) / 100.0;
	m_StartTick = Server()->Tick();

	m_Team = !m_Team;
	m_Owner = m_Team ? CLIENTID_BLUE : CLIENTID_RED;
}

void CProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x*100.0f);
	pProj->m_VelY = (int)(m_Direction.y*100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

void CProjectile::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if(pProj)
		FillInfo(pProj);
}
