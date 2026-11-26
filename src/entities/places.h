// places.h - ownership and access: who may enter, use and take what, where
// (docs/todo/architecture/ownership-and-access.md)
//
// Owners are groups (a household, a business, the town, one person; group 0 = public). A mover
// belongs to a few groups. A place is a set of cells (a per-cell layer of 16-bit place ids, 0 =
// no place) with an owner, a parent place and rules: which of enter / use / take are for the
// owner's members only. A cell holds its innermost place; a rule a place doesn't set is asked of
// its parent. Things are owned by where they are (food in my flat is mine). A grant lets one
// mover into one place (and the places inside it) for a while: a guest.
//
// Guards (see the doc): ownership never changes walkability (only locks do); callers use the
// queries below and never read the layer or tables; places are stored, not derived from rooms.
// Saved (v122): the layer after the one-way layer, the tables and memberships after the movers'
// social need. Grants are not saved (a load ends visits): a mover somewhere it may no longer be
// just walks out.
#ifndef PLACES_H
#define PLACES_H

#include <stdbool.h>
#include <stdint.h>
#include "../world/grid.h"
#include "mover.h"

#define MAX_GROUPS        512
#define MAX_PLACES        1024
#define MAX_MOVER_GROUPS  4
#define PLACE_NAME_LEN    24

typedef enum {
    GROUP_PUBLIC = 0,     // group 0: everyone
    GROUP_HOUSEHOLD,
    GROUP_BUSINESS,
    GROUP_TOWN,
    GROUP_PERSON,
    GROUP_KIND_COUNT
} GroupKind;

typedef struct {
    bool active;
    uint8_t kind;                    // GroupKind
    char name[PLACE_NAME_LEN];       // "Flat 3", "The bakery"
} OwnerGroup;

// Rules: which actions are for the owner's members (and grant holders) only. A place that
// doesn't set a rule leaves it to its parent; no place sets it = anyone may.
#define PLACE_RULE_ENTER  (1 << 0)
#define PLACE_RULE_USE    (1 << 1)
#define PLACE_RULE_TAKE   (1 << 2)
#define PLACE_RULES_HOME  (PLACE_RULE_ENTER | PLACE_RULE_USE | PLACE_RULE_TAKE)

typedef struct {
    bool active;
    int16_t owner;                   // group id, 0 = public
    int16_t parent;                  // place id, 0 = none
    uint8_t rules;                   // PLACE_RULE_* set here (members only)
    uint8_t rulesSet;                // PLACE_RULE_* this place decides (else: ask the parent)
    char name[PLACE_NAME_LEN];
    int16_t doorX, doorY, doorZ;     // front door (-1: none): where a visitor knocks
} PlaceInfo;

extern OwnerGroup groups[MAX_GROUPS];
extern PlaceInfo places[MAX_PLACES];
extern int16_t moverGroups[MAX_MOVERS][MAX_MOVER_GROUPS];   // 0 = empty slot
extern int16_t moverGrant[MAX_MOVERS];                     // place id the mover is a guest in, 0 = none

extern SparseLayer placeLayer;       // uint16_t place id per cell
#define PLACE_AT(x, y, z)     (*(const uint16_t*)SparseLayerRead(&placeLayer, (x), (y), (z)))
#define PLACE_AT_MUT(x, y, z) (*(uint16_t*)SparseLayerWrite(&placeLayer, (x), (y), (z)))

// Setup
void ClearPlaces(void);                          // new world: no groups, places, members, grants
void PlacesResetMover(int moverIdx);             // a new mover in this slot: no groups, no grant
int  GroupCreate(GroupKind kind, const char* name);                 // id >= 1, -1 = full
// A place owned by `owner` (group id, 0 = public) inside `parent` (0 = none). `rules` are the
// members-only actions; this place decides exactly those (others go to the parent).
int  PlaceCreate(int owner, int parent, uint8_t rules, const char* name);   // id >= 1, -1 = full
void PlaceFillRect(int place, int x0, int y0, int x1, int y1, int z);      // inclusive; 0 clears
void PlaceSetDoor(int place, int x, int y, int z);
bool MoverJoinGroup(int moverIdx, int group);    // false: no free slot (already in: true)
void MoverLeaveGroup(int moverIdx, int group);
bool MoverInGroup(int moverIdx, int group);      // group 0 (public): always
int  MoverHousehold(int moverIdx);               // first household group, 0 = none

// Grants (guests): one per mover, not saved
void GrantPlace(int moverIdx, int place);        // 0 = take it back
static inline int MoverGrant(int moverIdx) { return moverGrant[moverIdx]; }

// The questions (the only way callers look at ownership)
int  PlaceAt(int x, int y, int z);               // innermost place at a cell, 0 = none
bool MayEnter(int moverIdx, int x, int y, int z);
bool MayUse(int moverIdx, int x, int y, int z);
bool MayTake(int moverIdx, int x, int y, int z);
// The group that owns things at a cell (the innermost place with an owner), 0 = public
int  OwnerAt(int x, int y, int z);
// The home place of a household group: the place it owns with the front door, 0 = none
int  HomeOfGroup(int group);
const char* PlaceName(int place);                // "" for 0 / unknown
const char* GroupName(int group);                // "public" for 0

// Save/load (saveload.c): tables and memberships, after the movers' social need
#include <stdio.h>
void PlacesSave(FILE* f, int moverCount);
void PlacesLoad(FILE* f, int moverCount);

#endif // PLACES_H
