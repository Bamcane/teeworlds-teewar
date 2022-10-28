#ifndef GAME_SERVER_DEFINE_H
#define GAME_SERVER_DEFINE_H

enum Roles
{
    ROLE_NULL=0,
    ROLE_SNIPER,
    ROLE_SOLDIER,
    ROLE_ENGINEER,

    NUM_ROLES,
};

enum TowerState
{
    TOWERSTATE_ARMOR=1,
    TOWERSTATE_LASER=2,

    NUM_TOWERSTATES,
};

enum SystemClientID
{
    CLIENTID_RED=-4,
    CLIENTID_BLUE,

    NUM_CLIENTIDS,
};

#endif