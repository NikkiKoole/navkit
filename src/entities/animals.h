#ifndef ANIMALS_H
#define ANIMALS_H

#include <stdbool.h>

#define MAX_ANIMALS 256
#define ANIMAL_SPEED 60.0f
#define ANIMAL_GRAZE_TIME_GH 0.5f   // Game hours to eat one vegetation level
#define ANIMAL_IDLE_TIME_GH 0.25f   // Game hours to idle before scanning
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
    ANIMAL_BEING_HUNTED,
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
    bool markedForHunt;      // Player designation
    int reservedByHunter;    // Mover index, -1 = none
} Animal;

// Globals
extern Animal animals[MAX_ANIMALS];
extern int animalCount;
extern bool animalRespawnEnabled;
extern int animalTargetPopulation;
extern float animalSpawnIntervalGH;   // game hours between respawn attempts (not saved)

// Core functions
void SpawnAnimal(AnimalType type, int z, AnimalBehavior behavior);
void SpawnAnimalAtEdge(AnimalType type, int z, AnimalBehavior behavior);
void AnimalsTick(float dt);
void ClearAnimals(void);
int CountActiveAnimals(void);
void KillAnimal(int animalIdx);  // Deactivates animal + spawns carcass at position
int GetAnimalAtGrid(int x, int y, int z);  // Returns animal index at grid cell, or -1

// Scared (a thrown stone hit it): it runs away from (fromX, fromY) (pixels) for animalScareHours
// game hours (animal speeds scale with the day length, so it runs as far at any day length).
// Transient, not saved. Grazers and predators both run; a predator drops its prey.
extern float animalScareHours;
void ScareAnimal(int animalIdx, float fromX, float fromY);
bool AnimalScared(int animalIdx);

// Dazed (a thrown stone hit it): it stands still for animalDazeHours game hours, then comes to
// and runs from (fromX, fromY) like ScareAnimal. Transient, not saved.
extern float animalDazeHours;
void DazeAnimal(int animalIdx, float fromX, float fromY);
bool AnimalDazed(int animalIdx);

// How fast it moved over the last tick (pixels per game second), for aiming ahead of it
void AnimalVelocity(int animalIdx, float* vx, float* vy);

// Running pace of a fleeing grazer (from a hunter, a predator, a scare), x its walking speed
extern float animalFleeSpeed;

#endif // ANIMALS_H
