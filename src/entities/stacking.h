#ifndef STACKING_H
#define STACKING_H

#include "items.h"

// Merge incoming item's stack into existing item's stack.
// If the merge would exceed maxStack, only merges what fits and keeps the remainder.
// Returns the number of units actually merged.
int MergeItemIntoStack(int existingIdx, int incomingIdx);

// Split count units off from a stack, creating a new item.
// The original item's stackCount is reduced by count.
// Returns the new item index, or -1 on failure.
// count must be > 0 and < items[itemIdx].stackCount.
int SplitStack(int itemIdx, int count);

#endif
