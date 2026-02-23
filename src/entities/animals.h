#ifndef ANIMALS_H
#define ANIMALS_H

#include <stdbool.h>

#define MAX_ANIMALS 256
#define ANIMAL_SPEED 60.0f
#define ANIMAL_GRAZE_TIME 2.0f      // Seconds to eat one vegetation level
#define ANIMAL_IDLE_TIME 1.5f       // Seconds to idle before scanning
#define ANIMAL_SCAN_RADIUS 4        // Cells to scan for grass

typedef enum {
    ANIMAL_GRAZER,
    ANIMAL_PREDATOR,
} AnimalType;

typedef enum {
    ANIMAL_IDLE,
    ANIMAL_WALKING,
    ANIMAL_GRAZING,
    ANIMAL_HUNTING,
} AnimalState;

typedef enum {
    BEHAVIOR_SIMPLE_GRAZER,
    BEHAVIOR_STEERING_GRAZER,
    BEHAVIOR_PREDATOR,
} AnimalBehavior;

typedef struct {
    float x, y, z;          // Pixel coords (like movers)
    AnimalType type;
    AnimalState state;
    AnimalBehavior behavior;
    bool active;
    float speed;
    float stateTimer;        // Time in current state
    float grazeTimer;        // Time spent grazing current cell
    float animPhase;         // Animation phase accumulator (for head bob)
    int targetCellX, targetCellY;  // Local movement target
    float velX, velY;        // Velocity for steering behavior
    float wanderAngle;       // Persistent wander state for steering
    int targetAnimalIdx;     // Prey index for predators (-1 if none)
} Animal;

// Globals
extern Animal animals[MAX_ANIMALS];
extern int animalCount;

// Core functions
void SpawnAnimal(AnimalType type, int z, AnimalBehavior behavior);
void AnimalsTick(float dt);
void ClearAnimals(void);
int CountActiveAnimals(void);
void KillAnimal(int animalIdx);  // Deactivates animal + spawns carcass at position

#endif // ANIMALS_H
