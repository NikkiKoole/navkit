// cars_draw.c - Drawing for the cars experiment (src/entities/cars.h)
#include "../entities/cars.h"
#include "../entities/mover.h"
#include "../game_state.h"
#include <math.h>

static const Color carColors[] = {
    {200, 60, 50, 255}, {60, 110, 200, 255}, {230, 190, 60, 255}, {70, 160, 90, 255},
    {180, 90, 180, 255}, {230, 130, 50, 255},
};

static Vector2 CarScreen(float wx, float wy) {
    return (Vector2){offset.x + wx * zoom, offset.y + wy * zoom};
}

// Rectangle of size (l x w) pixels, centred at local (u,v) of the car, rotated with it
static void CarPart(const Car* c, float u, float v, float l, float w, float extraRot, Color col) {
    float ch = cosf(c->heading), sh = sinf(c->heading);
    Vector2 p = CarScreen(c->x + u * ch - v * sh, c->y + u * sh + v * ch);
    Rectangle r = {p.x, p.y, l * zoom, w * zoom};
    DrawRectanglePro(r, (Vector2){l * zoom * 0.5f, w * zoom * 0.5f}, (c->heading + extraRot) * RAD2DEG, col);
}

static void DrawCarDebug(const Car* c, Color col) {
    // Route: forward stretches in the car's colour, reverse in white, gear changes ringed
    if (c->state == DRIVE_FOLLOW) {
        for (int i = (c->pathIdx > 0 ? c->pathIdx : 0); i + 1 < c->pathLen; i++) {
            Color lc = c->pathDir[i] < 0 ? Fade(WHITE, 0.8f) : Fade(col, 0.8f);
            DrawLineEx(CarScreen(c->pathX[i], c->pathY[i]), CarScreen(c->pathX[i + 1], c->pathY[i + 1]), 2.0f, lc);
            if (i > 0 && c->pathDir[i] != c->pathDir[i - 1])
                DrawCircleLinesV(CarScreen(c->pathX[i], c->pathY[i]), 4.0f, YELLOW);
        }
        Vector2 g = CarScreen(c->pathX[c->pathLen - 1], c->pathY[c->pathLen - 1]);
        DrawCircleLinesV(g, 0.4f * CELL_SIZE * zoom, col);
        DrawCircleV(CarScreen(c->lookX, c->lookY), 3.0f, YELLOW);
    }
    // Stops (named by the scenario)
    for (int s = 0; s < c->stopCount; s++) {
        Vector2 p = CarScreen((c->stopX[s] + 0.5f) * CELL_SIZE, (c->stopY[s] + 0.5f) * CELL_SIZE);
        DrawText(TextFormat("%c", c->stopName[s]), (int)p.x - 4, (int)p.y - 6, 16, s == c->stopIdx ? YELLOW : WHITE);
    }
    Vector2 p = CarScreen(c->x, c->y - carLength * CELL_SIZE * 0.5f - 10);
    DrawText(DriveStateName(c->state), (int)p.x - 16, (int)p.y - 10, 12, WHITE);
}

// Where a car centre can be (planner nodes): green dot, bright where there's room
// to turn (the planner prefers those)
static void DrawCarClearance(int z) {
    int S = CarNodeSize();
    float h = S * 0.5f;
    int x0 = (int)((-offset.x) / zoom / CELL_SIZE) - 1, y0 = (int)((-offset.y) / zoom / CELL_SIZE) - 1;
    int x1 = x0 + (int)(GetScreenWidth() / zoom / CELL_SIZE) + 3, y1 = y0 + (int)(GetScreenHeight() / zoom / CELL_SIZE) + 3;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > gridWidth) x1 = gridWidth;
    if (y1 > gridHeight) y1 = gridHeight;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int cl = CarClearance(x, y, z);
            if (cl < S) continue;
            bool roomy = x >= 1 && y >= 1 && CarClearance(x - 1, y - 1, z) >= S + 2;
            Vector2 p = CarScreen((x + h) * CELL_SIZE, (y + h) * CELL_SIZE);
            DrawCircleV(p, roomy ? 2.5f : 1.5f, roomy ? Fade(GREEN, 0.8f) : Fade(LIME, 0.45f));
        }
}

void DrawCars(void) {
    if (carCount == 0) return;
    if (carDebug) DrawCarClearance(currentViewZ);
    float L = carLength * CELL_SIZE, W = carWidth * CELL_SIZE;
    float wb = L * 0.6f;
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active || c->z != currentViewZ) continue;
        Color col = carColors[i % (int)(sizeof(carColors) / sizeof(carColors[0]))];
        if (carDebug) DrawCarDebug(c, col);
        if (carDebug && c->waitNow > 0.5f && c->whyCar >= 0) {
            Vector2 a = {offset.x + c->x * zoom, offset.y + c->y * zoom};
            Vector2 b = {offset.x + cars[c->whyCar].x * zoom, offset.y + cars[c->whyCar].y * zoom};
            DrawLineEx(a, b, 2.0f, RED);
            if (c->waitNow >= CAR_DEADLOCK_S) DrawCircleLinesV(a, CELL_SIZE * 1.6f * zoom, RED);
        }
        // Wheels (front ones turned by the steering angle)
        float wl = L * 0.2f, ww = W * 0.14f, side = W * 0.5f - ww * 0.3f;
        Color tyre = {25, 25, 25, 255};
        CarPart(c, -wb * 0.5f, -side, wl, ww, 0, tyre);
        CarPart(c, -wb * 0.5f, side, wl, ww, 0, tyre);
        CarPart(c, wb * 0.5f, -side, wl, ww, c->steer, tyre);
        CarPart(c, wb * 0.5f, side, wl, ww, c->steer, tyre);
        // Body, roof, windscreen
        CarPart(c, 0, 0, L - 2, W - 4, 0, col);
        CarPart(c, -L * 0.05f, 0, L * 0.45f, W * 0.62f, 0, ColorBrightness(col, -0.25f));
        CarPart(c, L * 0.2f, 0, L * 0.08f, W * 0.6f, 0, (Color){170, 210, 230, 255});
        // Reversing lights
        if (c->speed < -1.0f) {
            CarPart(c, -L * 0.5f + 2, -W * 0.3f, 3, 4, 0, WHITE);
            CarPart(c, -L * 0.5f + 2, W * 0.3f, 3, 4, 0, WHITE);
        }
    }
}

// Camera follows car 0 (setting carFollow), eased like the mover follow
void CarsFollowCamera(float frameTime) {
    if (carCount < 1 || !cars[0].active) return;
    float t = 6.0f * frameTime;
    if (t > 1.0f) t = 1.0f;
    float targetX = GetScreenWidth() * 0.5f - cars[0].x * zoom;
    float targetY = GetScreenHeight() * 0.5f - cars[0].y * zoom;
    offset.x += (targetX - offset.x) * t;
    offset.y += (targetY - offset.y) * t;
    currentViewZ = cars[0].z;
}
