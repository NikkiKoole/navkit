#ifndef INSPECT_H
#define INSPECT_H

// Inspect a save file and print info to stdout
// Usage: ./bin/path --inspect [filename] [options]
// Returns 0 on success, 1 on error
int InspectSaveFile(int argc, char** argv);

#endif
