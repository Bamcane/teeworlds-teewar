/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_TOWER_H
#define GAME_SERVER_ENTITIES_TOWER_H

#include <game/server/entity.h>

#define NUM_ARMORS 24

class CTower : public CEntity
{
public:
	static const int ms_PhysSize = 128;

	int m_Team;
    int m_TowerHealth;
	int m_TowerState;
	
	int m_LaserArmor;
	int m_GiveArmor;

    int m_DestoryTick;

    int m_ArmorIDs[NUM_ARMORS];

	CTower(CGameWorld *pGameWorld, vec2 Pos, int Team);
    ~CTower();

	void TakeDamage(int Damage, int From);
	void TakeFix(int Health, int From);
	void OnDestory();
	void OnNormal();
	void OnFix();

	void UpdateState();

    virtual void Tick();
	virtual void Reset();
	virtual void Snap(int SnappingClient);
};

#endif
