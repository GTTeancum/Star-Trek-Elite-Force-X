#ifndef STEFX_PLAYER_PROFILE_H
#define STEFX_PLAYER_PROFILE_H

// Diagnostic only: inclusive actor work is partitioned by the existing phase
// markers. A scope closes the last interval even on an early return.
static unsigned int s_playerCycles[256], s_playerVisits[256];
static unsigned int s_playerEntered, s_playerCompleted;
static bool s_playerProfiling;
class STEFX_PlayerProfile;
static STEFX_PlayerProfile *s_playerProfile;
class STEFX_PlayerProfile {
    STEFX_PlayerProfile *parent;
    unsigned __int64 stamp;
    unsigned int stage;
    bool active;
public:
    STEFX_PlayerProfile() : parent(s_playerProfile), stage(0), active(s_playerProfiling) {
        if (active) {
            if (parent) parent->Close();
            s_playerProfile=this; ++s_playerEntered;
            stamp=STEFX_XboxReadTsc();
        }
    }
    void Close() {
        s_playerCycles[stage]+=STEFX_XboxElapsedCycles(stamp);
        ++s_playerVisits[stage];
    }
    void Mark(unsigned int next) {
        Close(); stage=next & 255u; stamp=STEFX_XboxReadTsc();
    }
    ~STEFX_PlayerProfile() {
        if (active) {
            Close(); ++s_playerCompleted; s_playerProfile=parent;
            if (parent) parent->stamp=STEFX_XboxReadTsc();
        }
    }
};
static void STEFX_ProfilePlayerPhase(unsigned int stage) {
    if (s_playerProfile) s_playerProfile->Mark(stage);
}
static void STEFX_ResetPlayerProfile(bool active) {
    s_playerProfiling=active;
    s_playerEntered=s_playerCompleted=0;
    for (int i=0;i<256;++i) s_playerCycles[i]=s_playerVisits[i]=0;
}
#endif
