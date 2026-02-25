# 04e: Animal Respawn

> Split from 04-cooking-and-hunting.md on 2026-02-22.
> **Deps**: None (animals already exist)
> **Opens**: Sustainable hunting, farming pressure from over-hunting

---

## Goal

Animals respawn so hunting is sustainable. Without this, the map's animal population is a one-time resource. Simple edge-spawn on a timer, capped at a target population.

---

## Design

### Edge Spawning

New animals wander in from map edges at a slow rate:

- **Check interval**: Every 2-4 game-hours
- **Spawn condition**: `CountActiveAnimals() < targetPopulation`
- **Spawn location**: Random walkable cell on map edge (x=0, x=max, y=0, y=max)
- **Spawn type**: Weighted random — mostly grazers, rare predator (80/20)
- **Target population**: Configurable, default = initial spawn count

### Population Pressure

- If population drops below 50% of target: spawn rate doubles
- If population is at target: no spawning
- Over-hunting keeps population low → fewer carcasses → food pressure → need farming (06)

This is the "hunting doesn't scale" design intent from the original doc. Animal respawn is slow enough that a colony can't live on meat alone forever.

---

## Implementation

Single function, called from sim tick:

```
AnimalRespawnTick(float dt):
  Accumulate timer
  If timer >= spawnInterval:
    If CountActiveAnimals() < targetPopulation:
      Pick random edge cell
      If walkable: SpawnAnimal(type, z, behavior)
    Reset timer
```

Add to `AnimalsTick()` or call separately from sim_manager.

### Configuration

```c
int animalTargetPopulation;      // Default: initial count at map gen
float animalSpawnIntervalGH;     // Default: 3.0 game-hours
```

Store in balance.h or as globals in animals.c.

---

## Design Notes

- **No breeding**: Animals don't reproduce on-map. Edge spawning is simpler and avoids needing baby animals, mating behavior, etc.
- **No migration patterns**: Animals just appear at edges. Seasonal migration could come much later.
- **Predator cap**: Max 1-2 predators at a time. Too many predators kill all grazers.
- **Not saved**: Spawn timer is runtime-only. On load, timer resets to 0 (fine — just delays next spawn slightly).
- **Map edge only**: Prevents animals popping into existence where the player can see. They walk in naturally from off-screen.
