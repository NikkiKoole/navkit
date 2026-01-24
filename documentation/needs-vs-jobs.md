# Needs System vs Job System

Design notes on separating individual-driven needs from colony-driven jobs.

---

## The Core Distinction

| Aspect | Job System | Needs System |
|--------|------------|--------------|
| Driver | "Colony needs X done" | "This mover needs Y" |
| Assignment | Any capable mover | Only that specific mover |
| Task exists | Independent of who does it | Intrinsically personal |
| Examples | Mining, hauling, building, crafting | Eating, sleeping, recreation |

---

## Why They Must Be Separate

When "eat apple" becomes a job:
- Who's the employer? The mover themselves?
- The apple gets reserved... for the hungry person?
- Another mover might "steal" the job assignment?
- The hungry mover competes with colony work priorities?

This feels backwards because **needs aren't work requests** - they're **interrupts**.

---

## How Other Games Handle This

**RimWorld:**
- Needs checked separately from job assignment
- When need is critical (starving, exhausted), pawn **drops current job**
- Personal tasks bypass WorkGiver system entirely
- Direct behavior: path to food -> pick up -> eat

**Dwarf Fortress:**
- "Break" activities interrupt labor
- Dwarves autonomously seek food/drink/sleep when thresholds hit
- Not "jobs" in the labor sense

---

## Proposed Architecture

```
┌─────────────────────────────────────┐
│           Mover Update              │
├─────────────────────────────────────┤
│  1. Check critical needs            │
│     - Starving? -> interrupt, eat   │
│     - Exhausted? -> interrupt, sleep│
│                                     │
│  2. If no critical need:            │
│     - Continue current job          │
│     - Or get new job from WorkGivers│
└─────────────────────────────────────┘
```

### Needs System
- Tracks hunger, energy, mood per mover
- When threshold crossed -> triggers personal action
- Personal actions are **not jobs** - hardcoded behaviors
- Mover walks to fridge, gets food, eats (direct state machine)
- No reservation competition with other movers

### Job System
- Tracks colony work that needs doing
- Assigns work to available movers
- Movers can be interrupted by needs

---

## Communication Between Systems

They talk at one point: **availability**.

```c
bool MoverAvailableForWork(Mover* m) {
    // Not available if handling critical need
    if (m->needState != NEED_STATE_NONE) return false;
    
    // Not available if already has job
    if (m->currentJobId >= 0) return false;
    
    return true;
}
```

Optional: avoid assigning long jobs to nearly-starving movers:

```c
// Don't assign long jobs to nearly-starving movers
if (IsJobLong(job) && m->hunger > 0.8f) {
    continue; // Skip this mover
}
```

---

## Personal Action State Machine (Sketch)

```c
typedef enum {
    NEED_STATE_NONE,        // No active need being handled
    NEED_STATE_SEEKING,     // Walking to need satisfier (fridge, bed)
    NEED_STATE_CONSUMING,   // Eating, sleeping, etc.
} NeedState;

typedef enum {
    NEED_HUNGER,
    NEED_ENERGY,
    NEED_MOOD,
    // etc.
} NeedType;

// On Mover struct
NeedState needState;
NeedType activeNeed;
int needTarget;             // Item or furniture being used
float needProgress;         // For timed actions (eating, sleeping)
```

---

## Job Interruption on Critical Need

```c
void CheckCriticalNeeds(Mover* m) {
    if (m->hunger > HUNGER_CRITICAL && m->needState == NEED_STATE_NONE) {
        // Interrupt current job
        if (m->currentJobId >= 0) {
            CancelJob(m->currentJobId);  // Releases reservations
            m->currentJobId = -1;
        }
        
        // Start personal action
        m->needState = NEED_STATE_SEEKING;
        m->activeNeed = NEED_HUNGER;
        m->needTarget = FindNearestFood(m);
    }
}
```

---

## Priority: Colony Jobs First

For navkit, focus on the colony-driven job system first:
- Mining, hauling, building, crafting
- WorkGivers, Job Pool, Job Drivers

Needs system can be added later as a layer on top:
- Doesn't require changing job architecture
- Just adds interruption logic and personal action state machine

---

## Open Questions (For Later)

- Do movers "own" food items while eating? Or just consume on pickup?
- Can other movers steal food a mover is walking toward?
- How does mood affect job performance? (speed modifier?)
- Recreation: is it just "idle time near fun object" or structured activities?
- Social needs: conversations as paired personal actions?

---

## References

- RimWorld Wiki: Needs (https://rimworldwiki.com/wiki/Needs)
- Dwarf Fortress Wiki: Thoughts (https://dwarffortresswiki.org/index.php/Thought)
