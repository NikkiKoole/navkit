# Mover Control & Work Radius

> **Status**: Design brainstorm
> **Why**: In survival mode, movers wander across the entire map doing jobs. No way to keep them local or override their behavior.

---

## Problem

- No gather zones → movers haul everything on the map
- With gather zones → movers still walk far for designations, building, auto-eat/drink
- No max work distance — WorkGivers find "nearest" but will send movers anywhere
- Player has no direct control over mover movement

## Ideas

### 1. Work Radius / Leash
- Home point (spawn or player-set) with configurable radius
- Jobs outside radius are ignored or heavily deprioritized
- Radius expands as player explores / builds outposts
- Simple but rigid — might frustrate when you *want* them to go far

### 2. Distance Weighting
- Score jobs by (priority - distance_penalty)
- Nearby work always wins over far work at same priority
- Far work still happens eventually when nothing closer is available
- More organic, less player control

### 3. Draft Mode (RimWorld-style)
- Toggle per mover: drafted = ignore job system, stand still
- Click-to-move when drafted
- Undraft to resume normal autonomous behavior
- Good for urgent manual override, not a solution for default behavior
- Orthogonal to work radius — both are useful

### 4. Work Areas (Dwarf Fortress burrows)
- Named zones that restrict which movers work where
- Assign movers to areas — they only take jobs within their area
- Most flexible but most complex UI

## Notes

- Gather zone already gates hauling (IsItemInGatherZone), but designations/building/eating ignore it
- Could start simple: draft mode (bool per mover, skip in AssignJobs) + distance weighting
- Draft mode is ~20 lines of code, distance weighting is a scoring change in WorkGivers
