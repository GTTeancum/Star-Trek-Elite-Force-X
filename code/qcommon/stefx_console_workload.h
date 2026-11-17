// Explicit console qualification only; RAM publication, no input or game edits.
// Included by each actual game module after its own types and globals exist.
#ifdef __cplusplus
extern "C" {
#endif
extern volatile unsigned int g_SPXBConsoleWorkload[80];
extern volatile unsigned int g_SPXBConsoleFire[4];
extern volatile unsigned int g_SPXBConsoleNpcFire;
extern volatile unsigned int g_SPXBConsoleQualification;
#ifdef __cplusplus
}
#endif

static void G_STEFX_PublishConsoleWorkload(void) {
    static int enabled = -1, nextTime, previousTime;
    int i, slot, count, connected=0, playing=0, bots=0, npcs=0, engaged=0;
    unsigned int serial;
    if (enabled < 0) {
#if defined(STEFX_SP_HOSTED_MP)
        enabled = trap_Cvar_VariableIntegerValue("stefx_console_qualification") == 1;
#else
        enabled = gi.Cvar_VariableIntegerValue("stefx_console_qualification") == 1;
#endif
        g_SPXBConsoleQualification = enabled;
    }
    if (!enabled) return;
    if (level.time < previousTime) nextTime = 0;
    previousTime = level.time;
    if (level.time < nextTime) return;
    nextTime = level.time + 100;
    serial = (g_SPXBConsoleWorkload[0] + 1u) | 1u;
    g_SPXBConsoleWorkload[0] = serial;
    for (i=1; i<80; ++i) g_SPXBConsoleWorkload[i] = 0;
    g_SPXBConsoleWorkload[1] = 1; // schema
    g_SPXBConsoleWorkload[2] = level.time;
#if defined(STEFX_SP_HOSTED_MP)
    count = 4;
    g_SPXBConsoleWorkload[3] = 2;
    for (i=0; i<level.maxclients; ++i) {
        gentity_t *ent = &g_entities[i];
        if (!ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED) continue;
        ++connected;
        if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) ++playing;
        if (ent->r.svFlags & SVF_BOT) ++bots;
    }
#else
    count = 2;
    g_SPXBConsoleWorkload[3] = 1;
    for (i=1; i<globals.num_entities; ++i) {
        gentity_t *ent = &g_entities[i];
        if (ent->inuse && ent->NPC && ent->health > 0) {
            ++npcs;
            if (ent->enemy && ent->enemy->inuse && ent->enemy->health > 0) ++engaged;
        }
    }
#endif
    for (slot=0; slot<count; ++slot) {
        int number = slot, base = 12 + slot*16, ammo = -9999;
        gentity_t *ent;
        playerState_t *ps;
        usercmd_t *cmd;
#if !defined(STEFX_SP_HOSTED_MP)
        if (slot) number = s_stefxSplitP2EntNum;
#endif
        g_SPXBConsoleWorkload[base] = number;
        if (number < 0 || number >= MAX_GENTITIES) continue;
        ent = &g_entities[number];
        if (!ent->inuse || !ent->client) continue;
        ps = &ent->client->ps;
#if defined(STEFX_SP_HOSTED_MP)
        cmd = &ent->client->pers.cmd;
        if (ps->weapon >= 0 && ps->weapon < MAX_WEAPONS) ammo = ps->ammo[ps->weapon];
        g_SPXBConsoleWorkload[base+12] = ent->client->pers.connected;
#else
        cmd = &ent->client->usercmd;
        if (ps->weapon >= 0 && ps->weapon < WP_NUM_WEAPONS) {
            int index = weaponData[ps->weapon].ammoIndex;
            if (index >= 0 && index < MAX_AMMO) ammo = ps->ammo[index];
        }
        ++connected; ++playing;
        g_SPXBConsoleWorkload[base+12] = 1;
#endif
        g_SPXBConsoleWorkload[base+1] = ps->stats[STAT_HEALTH];
        for (i=0; i<3; ++i) {
            unsigned int bits;
            memcpy(&bits, &ps->origin[i], 4);
            g_SPXBConsoleWorkload[base+2+i] = bits;
        }
        g_SPXBConsoleWorkload[base+5] = ps->weapon;
        g_SPXBConsoleWorkload[base+6] = ammo;
        g_SPXBConsoleWorkload[base+7] = g_SPXBConsoleFire[slot];
        g_SPXBConsoleWorkload[base+8] = cmd->buttons;
        g_SPXBConsoleWorkload[base+9] = (int)cmd->forwardmove;
        g_SPXBConsoleWorkload[base+10] = (int)cmd->rightmove;
        g_SPXBConsoleWorkload[base+11] = ent->enemy ? ent->enemy->s.number : ~0u;
        g_SPXBConsoleWorkload[base+13] = ent->inuse;
        g_SPXBConsoleWorkload[base+14] = ps->eventSequence;
        g_SPXBConsoleWorkload[base+15] = ps->pm_type;
    }
    g_SPXBConsoleWorkload[4] = count;
    g_SPXBConsoleWorkload[5] = connected;
    g_SPXBConsoleWorkload[6] = playing;
    g_SPXBConsoleWorkload[7] = bots;
    g_SPXBConsoleWorkload[8] = npcs;
    g_SPXBConsoleWorkload[9] = engaged;
    g_SPXBConsoleWorkload[11] = g_SPXBConsoleNpcFire;
    g_SPXBConsoleWorkload[0] = serial+1;
}
