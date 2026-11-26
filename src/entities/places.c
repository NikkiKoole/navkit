// places.c - ownership and access (places.h, docs/todo/architecture/ownership-and-access.md)
#include "places.h"
#include "mover.h"
#include <string.h>

OwnerGroup groups[MAX_GROUPS];
PlaceInfo places[MAX_PLACES];
int16_t moverGroups[MAX_MOVERS][MAX_MOVER_GROUPS];
int16_t moverGrant[MAX_MOVERS];

// HomeOfGroup cache: the home place per group, rebuilt when a place or door changes
static int16_t placesHomeOf[MAX_GROUPS];
static bool placesHomesDirty = true;

static void PlacesCopyName(char* dst, const char* src) {
    strncpy(dst, src ? src : "", PLACE_NAME_LEN - 1);
    dst[PLACE_NAME_LEN - 1] = '\0';
}

void ClearPlaces(void) {
    memset(groups, 0, sizeof(groups));
    memset(places, 0, sizeof(places));
    memset(moverGroups, 0, sizeof(moverGroups));
    memset(moverGrant, 0, sizeof(moverGrant));
    groups[0].active = true;                       // group 0: public
    groups[0].kind = GROUP_PUBLIC;
    PlacesCopyName(groups[0].name, "public");
    SparseLayerClear(&placeLayer);
    placesHomesDirty = true;
}

void PlacesResetMover(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return;
    memset(moverGroups[moverIdx], 0, sizeof(moverGroups[moverIdx]));
    moverGrant[moverIdx] = 0;
}

int GroupCreate(GroupKind kind, const char* name) {
    for (int g = 1; g < MAX_GROUPS; g++) {
        if (groups[g].active) continue;
        groups[g].active = true;
        groups[g].kind = (uint8_t)kind;
        PlacesCopyName(groups[g].name, name);
        return g;
    }
    return -1;
}

// How many places deep p sits (a root: 1)
static int PlacesDepth(int p) {
    int d = 0;
    for (; p > 0 && d <= PLACE_MAX_DEPTH; p = places[p].parent) d++;
    return d;
}

int PlaceCreate(int owner, int parent, uint8_t rules, const char* name) {
    if (owner < 0 || owner >= MAX_GROUPS || parent < 0 || parent >= MAX_PLACES) return -1;
    if (!groups[owner].active) return -1;                           // no such group
    if (parent > 0 && (!places[parent].active || PlacesDepth(parent) >= PLACE_MAX_DEPTH)) return -1;   // no such place / too deep
    for (int p = 1; p < MAX_PLACES; p++) {
        if (places[p].active) continue;
        places[p] = (PlaceInfo){ .active = true, .owner = (int16_t)owner, .parent = (int16_t)parent,
                             .rules = rules, .rulesSet = rules, .doorX = -1, .doorY = -1, .doorZ = -1 };
        PlacesCopyName(places[p].name, name);
        placesHomesDirty = true;
        return p;
    }
    return -1;
}

void PlaceFillRect(int place, int x0, int y0, int x1, int y1, int z) {
    if (place < 0 || place >= MAX_PLACES || z < 0 || z >= gridDepth) return;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (x < 0 || y < 0 || x >= gridWidth || y >= gridHeight) continue;
            if (place == 0 && !SparseLayerHasBlock(&placeLayer, x, y)) continue;   // nothing to clear
            PLACE_AT_MUT(x, y, z) = (uint16_t)place;
        }
}

void PlaceSetDoor(int place, int x, int y, int z) {
    if (place <= 0 || place >= MAX_PLACES || !places[place].active) return;
    places[place].doorX = (int16_t)x;
    places[place].doorY = (int16_t)y;
    places[place].doorZ = (int16_t)z;
    placesHomesDirty = true;
}

bool MoverInGroup(int moverIdx, int group) {
    if (group == 0) return true;
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return false;
    for (int k = 0; k < MAX_MOVER_GROUPS; k++)
        if (moverGroups[moverIdx][k] == group) return true;
    return false;
}

bool MoverJoinGroup(int moverIdx, int group) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS || group <= 0 || group >= MAX_GROUPS) return false;
    if (MoverInGroup(moverIdx, group)) return true;
    for (int k = 0; k < MAX_MOVER_GROUPS; k++) {
        if (moverGroups[moverIdx][k] != 0) continue;
        moverGroups[moverIdx][k] = (int16_t)group;
        return true;
    }
    return false;
}

void MoverLeaveGroup(int moverIdx, int group) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return;
    for (int k = 0; k < MAX_MOVER_GROUPS; k++)
        if (moverGroups[moverIdx][k] == group) moverGroups[moverIdx][k] = 0;
}

int MoverHousehold(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return 0;
    for (int k = 0; k < MAX_MOVER_GROUPS; k++) {
        int g = moverGroups[moverIdx][k];
        if (g > 0 && groups[g].active && groups[g].kind == GROUP_HOUSEHOLD) return g;
    }
    return 0;
}

void GrantPlace(int moverIdx, int place) {
    if (moverIdx < 0 || moverIdx >= MAX_MOVERS || place < 0 || place >= MAX_PLACES) return;
    moverGrant[moverIdx] = (int16_t)place;
}

int PlaceAt(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= gridWidth || y >= gridHeight || z >= gridDepth) return 0;
    int p = PLACE_AT(x, y, z);
    return (p > 0 && p < MAX_PLACES && places[p].active) ? p : 0;
}

// Does the mover's grant cover place p (p itself or a place inside the granted one)?
static bool PlacesGrantCovers(int moverIdx, int p) {
    int g = moverGrant[moverIdx];
    if (g <= 0) return false;
    for (int q = p, guard = 0; q > 0 && guard < PLACE_MAX_DEPTH; q = places[q].parent, guard++)
        if (q == g) return true;
    return false;
}

// Walk up from the innermost place to the first that decides this rule
static bool PlacesAllows(int moverIdx, int x, int y, int z, uint8_t rule) {
    int p = PlaceAt(x, y, z);
    for (int guard = 0; p > 0 && guard < PLACE_MAX_DEPTH; p = places[p].parent, guard++) {
        const PlaceInfo* pl = &places[p];
        if (!pl->active || !(pl->rulesSet & rule)) continue;
        if (!(pl->rules & rule)) return true;              // decided here: open to all
        if (moverIdx < 0 || moverIdx >= MAX_MOVERS) return false;
        return MoverInGroup(moverIdx, pl->owner) || PlacesGrantCovers(moverIdx, p);
    }
    return true;                                           // nobody decides: anyone may
}

bool MayEnter(int moverIdx, int x, int y, int z) { return PlacesAllows(moverIdx, x, y, z, PLACE_RULE_ENTER); }
bool MayUse(int moverIdx, int x, int y, int z)   { return PlacesAllows(moverIdx, x, y, z, PLACE_RULE_USE); }
bool MayTake(int moverIdx, int x, int y, int z)  { return PlacesAllows(moverIdx, x, y, z, PLACE_RULE_TAKE); }

int OwnerAt(int x, int y, int z) {
    int p = PlaceAt(x, y, z);
    for (int guard = 0; p > 0 && guard < PLACE_MAX_DEPTH; p = places[p].parent, guard++)
        if (places[p].active && places[p].owner > 0) return places[p].owner;
    return 0;
}

// The first place (lowest id) a group owns with a front door
int HomeOfGroup(int group) {
    if (group <= 0 || group >= MAX_GROUPS) return 0;
    if (placesHomesDirty) {
        memset(placesHomeOf, 0, sizeof(placesHomeOf));
        for (int p = MAX_PLACES - 1; p >= 1; p--) {
            const PlaceInfo* pl = &places[p];
            if (pl->active && pl->owner > 0 && pl->owner < MAX_GROUPS && pl->doorX >= 0) placesHomeOf[pl->owner] = (int16_t)p;
        }
        placesHomesDirty = false;
    }
    return placesHomeOf[group];
}

const char* PlaceName(int place) {
    return (place > 0 && place < MAX_PLACES && places[place].active) ? places[place].name : "";
}

const char* GroupName(int group) {
    if (group == 0) return "public";
    return (group > 0 && group < MAX_GROUPS && groups[group].active) ? groups[group].name : "";
}

void PlacesSave(FILE* f, int moverCount) {
    fwrite(groups, sizeof(OwnerGroup), MAX_GROUPS, f);
    fwrite(places, sizeof(PlaceInfo), MAX_PLACES, f);
    fwrite(moverGroups, sizeof(moverGroups[0]), (size_t)moverCount, f);
}

void PlacesLoad(FILE* f, int moverCount) {
    fread(groups, sizeof(OwnerGroup), MAX_GROUPS, f);
    fread(places, sizeof(PlaceInfo), MAX_PLACES, f);
    memset(moverGroups, 0, sizeof(moverGroups));
    fread(moverGroups, sizeof(moverGroups[0]), (size_t)moverCount, f);
    memset(moverGrant, 0, sizeof(moverGrant));            // grants aren't saved: a load ends visits
    placesHomesDirty = true;
}
