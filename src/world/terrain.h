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
void GenerateLabyrinth3D(void);   // 3D layered maze with offset passages
void GenerateSpiral3D(void);      // 3D concentric rings with spiral traversal
void GenerateCouncilEstate(void); // UK council estate with tower blocks and low-rise
void GenerateHills(void);         // Natural hills/mountains using Perlin heightmap
void GenerateHillsSoils(void);    // Hills with soil bands (clay/gravel/sand/peat)
void GenerateHillsSoilsWater(void); // Hills + soils + rivers/lakes
void GenerateCraftingTest(void);  // Crafting system test scenario

// Perlin noise utilities
void InitPerlin(int seed);
float Perlin(float x, float y);
float OctavePerlin(float x, float y, int octaves, float persistence);

#endif // TERRAIN_H
