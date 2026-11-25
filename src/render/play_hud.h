// play_hud.h - The play HUD's tab bar: Huisje / Boompje / Beestje (see play_hud.c)
#ifndef PLAY_HUD_H
#define PLAY_HUD_H

#include "../../vendor/raylib.h"

Rectangle PlayHudRect(void);   // screen area it covers (zero when the dev UI is on)
void DrawPlayHudTabs(void);    // draw + handle clicks (inside DrawPlayerHUD)
void DrawTownOverview(void);   // the town in one line, top centre (only when there is a town)

#endif
