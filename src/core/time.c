#include "time.h"
#include "../entities/mover.h"  // For Tick() and TICK_DT
#include <stdlib.h>
#include <stdbool.h>

// Core time variables
float gameSpeed = 1.0f;
double gameTime = 0.0;
float gameDeltaTime = TICK_DT;  // Default to one tick for systems called directly

// World clock
float timeOfDay = 6.0f;        // Start at 6am
int dayNumber = 1;
float dayLength = 60.0f;       // Default: 1 real-minute = 1 day

void InitTime(void) {
    gameSpeed = 1.0f;
    gameTime = 0.0;
    gameDeltaTime = TICK_DT;  // Default to one tick
    timeOfDay = 6.0f;
    dayNumber = 1;
    dayLength = 60.0f;
}

void ResetTime(void) {
    InitTime();
}

bool UpdateTime(float tickDt) {
    // Paused - no time passes
    if (gameSpeed <= 0.0f) {
        gameDeltaTime = 0.0f;
        return false;
    }
    
    // Accumulate game time
    gameDeltaTime = tickDt * gameSpeed;
    gameTime += gameDeltaTime;
    
    // Update world clock
    if (dayLength > 0.0f) {
        timeOfDay += (gameDeltaTime / dayLength) * 24.0f;
        while (timeOfDay >= 24.0f) {
            timeOfDay -= 24.0f;
            dayNumber++;
        }
    }
    
    return true;
}

void RunGameSeconds(float seconds) {
    double targetTime = gameTime + seconds;
    while (gameTime < targetTime) {
        Tick();
    }
}

void ResetTestState(unsigned int seed) {
    srand(seed);
    ResetTime();
    // Note: Individual systems may need their accumulators reset too
    // This will be extended as we convert systems to use game time
}
