#ifndef TIME_H
#define TIME_H

// Game time system
// Separates engine ticks (60Hz fixed) from game time (scaled by gameSpeed)

// Core time variables
extern float gameSpeed;        // Multiplier: 1.0 = real-time, 10.0 = 10x speed, 0 = paused
extern double gameTime;        // Total elapsed game-seconds (double for precision over long sessions)
extern float gameDeltaTime;    // Game-seconds elapsed this tick

// World clock
extern float timeOfDay;        // 0.0-24.0 hours
extern int dayNumber;          // Current day (starts at 1)
extern float dayLength;        // Game-seconds per full day

// Initialize time system
void InitTime(void);

// Reset time state (for tests)
void ResetTime(void);

// Update time - call once per tick
// Returns false if paused (gameSpeed <= 0), true otherwise
bool UpdateTime(float tickDt);

// Helper to run simulation for a given number of game-seconds
// Used by tests - runs ticks until gameTime has advanced by 'seconds'
void RunGameSeconds(float seconds);

// Reset all test state including RNG seed
void ResetTestState(unsigned int seed);

#endif
