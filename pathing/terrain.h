#ifndef TERRAIN_H
#define TERRAIN_H

#include "grid.h"

// Terrain generation functions
void InitGrid(void);
void GenerateSparse(float density);
void GeneratePerlin(void);
void GenerateCity(void);
void GenerateMixed(void);
void GenerateConcentricMaze(void);
void GenerateDungeonRooms(void);  // Feature-based roguelike dungeon
void GenerateCaves(void);         // Cellular automata caves
void GenerateDrunkard(void);      // Drunkard's walk tunnels
void GenerateTunneler(void);      // Tunneler algorithm (rooms + corridors)
void GenerateMixMax(void);        // Tunneler + extra rooms
void GenerateTowers(void);        // 3D towers with bridges
void GenerateGalleryFlat(void);   // 3D gallery flat apartment building
void GenerateCastle(void);        // 3D medieval castle with wall walk

// Perlin noise utilities
void InitPerlin(int seed);
float Perlin(float x, float y);
float OctavePerlin(float x, float y, int octaves, float persistence);

#endif // TERRAIN_H
