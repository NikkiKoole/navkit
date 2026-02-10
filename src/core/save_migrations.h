#ifndef SAVE_MIGRATIONS_H
#define SAVE_MIGRATIONS_H

// Current save version (bump when save format changes)
#define CURRENT_SAVE_VERSION 30

// ============================================================================
// SAVE MIGRATION PATTERN (for future use when backward compatibility needed)
// ============================================================================
// 
// When the save format changes (add/remove fields, change enum sizes, etc.):
// 1. Bump CURRENT_SAVE_VERSION
// 2. Add version constants here (e.g., #define V22_ITEM_TYPE_COUNT 24)
// 3. Define legacy struct typedefs here (e.g., typedef struct { ... } StockpileV22;)
// 4. In LoadWorld(), add migration path: if (version >= 23) { /* new format */ } else { /* V22 migration */ }
// 5. Update inspect.c with same migration logic using same structs from this header
//
// This ensures saveload.c and inspect.c can't drift - they share the same legacy definitions.
//
// Currently in development: only V22 is supported, old saves will error out cleanly.
// ============================================================================

#endif
