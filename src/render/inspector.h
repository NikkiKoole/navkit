// inspector.h - Selection outline and the fixed inspector panel (see inspector.c)
#ifndef INSPECTOR_H
#define INSPECTOR_H

#include "../../vendor/raylib.h"

Rectangle InspectorPanelRect(void);   // zero size when nothing is selected
void DrawSelectionHighlight(void);    // outline in the world (call before the UI)
void DrawInspectorPanel(void);        // the panel (call after DrawUI)

#endif
