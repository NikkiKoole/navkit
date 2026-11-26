// mechanisms.h - levers, pressure plates and links (docs/todo/world/infrastructure.md step 3)
//
// A source (a lever or a pressure plate) is on or off. A link joins a source to a target
// cell; when the source changes, every link from it writes a number to its target:
// on = 1, off = 0 (flipped by the link's invert). Nothing runs per tick but the plates'
// weight check (only while plates exist) and doors waiting for their doorway to clear.
//
// Targets speak numbers (the signal model of docs/vision/far future /gates-and-processors.md):
//   door  write: 1 = unlock and open, 0 = lock (waits until nobody stands in the doorway)
//         read:  0 locked, 1 closed, 2 open
//   valve write: 1 = open, 0 = shut      read: 0 shut, 1 open
// What a target was told (write) and what it is (read) may differ: a door told to lock stays
// open while someone stands in it.
#ifndef MECHANISMS_H
#define MECHANISMS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    MECH_NONE = 0,
    MECH_LEVER,     // a mover pulls it (or the console); stays where it is put
    MECH_PLATE,     // on while the weight on its cell is at least plateWeight
} MechKind;

typedef struct {
    int16_t x, y, z;
    uint8_t kind;       // MechKind; MECH_NONE = free slot
    uint8_t on;
} Mechanism;

typedef struct {
    int16_t src;        // index into mechanisms[]; -1 = free slot
    int16_t x, y, z;    // target cell
    uint8_t invert;
    uint8_t pad;
} MechLink;

#define MAX_MECHANISMS 256
#define MAX_MECH_LINKS 1024

extern Mechanism mechanisms[MAX_MECHANISMS];
extern MechLink mechLinks[MAX_MECH_LINKS];
extern int plateWeight;        // setting: weight that holds a plate down (a mover or animal = 1, an item = its stack)
extern bool linksOverlay;      // setting: draw every link as a line from source to target

void ClearMechanisms(void);
int  PlaceMechanism(int x, int y, int z, MechKind kind);   // index, or -1 (taken / full / off map)
bool RemoveMechanism(int x, int y, int z);                  // and its links
int  MechanismAt(int x, int y, int z);                       // index or -1
int  MechanismCount(void);
const char* MechKindName(int kind);

// Links. A target must be a door or a pipe valve when linked; it is looked up again on each write.
int  LinkMechanism(int src, int tx, int ty, int tz, bool invert);  // index or -1 (already linked: that one)
bool UnlinkMechanism(int src, int tx, int ty, int tz);
int  MechLinkCount(int src);                                   // links from src (-1 = all)

// Flip a lever, or set a source; fires its links when the state changes.
void SetMechanismOn(int idx, bool on);
void PullLever(int idx);

// Target numbers (see top)
bool MechIsTarget(int x, int y, int z);
void MechWrite(int x, int y, int z, int value);
int  MechRead(int x, int y, int z);            // -1 = nothing there
bool MechLockWaiting(int x, int y, int z);     // a door told to lock, waiting for its doorway

// Link mode (the inspector's "Link" button): clicking a door or valve links or unlinks it
extern int linkingMechanismIdx;                // -1 = not linking
bool ToggleMechanismLink(int src, int tx, int ty, int tz);   // true = now linked, false = unlinked (or no target)
bool MechLinked(int src, int tx, int ty, int tz);
void FlipMechanismLinkInvert(int linkIdx);
const char* MechTargetName(int x, int y, int z);   // "door", "valve", NULL
const char* MechTargetStateName(int x, int y, int z);   // "locked", "closed", "open", "shut", NULL

void MechanismsTick(void);                     // plates + waiting doors

// Pulling a lever by order: a designation on the lever's cell, a mover walks up and pulls it
bool DesignatePullLever(int x, int y, int z);
void CompletePullLeverDesignation(int x, int y, int z);

void MechanismsSave(FILE* f);
void MechanismsLoad(FILE* f);

#endif
