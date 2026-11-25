// cars_draw.c - Drawing for the cars experiment (src/entities/cars.h)
#include "../entities/cars.h"
#include "../entities/roads.h"
#include "../entities/traffic.h"
#include "../entities/slots.h"
#include "../entities/mover.h"
#include "../game_state.h"
#include "../../assets/atlas.h"
#include <math.h>

#define CAB_YELLOW ((Color){250, 200, 20, 255})
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

// Road network (debug): lane lines, faint; junction curves someone holds, green
static void DrawRoadsDebug(void) {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    for (int s = 0; s < rdSegCount; s++) {
        const RdSeg* sg = &rdSegs[s];
        Vector2 b0 = CarScreen(sg->bx0, sg->by0), b1 = CarScreen(sg->bx1, sg->by1);
        if (b1.x < 0 || b1.y < 0 || b0.x > sw || b0.y > sh) continue;
        bool held = false;
        if (sg->kind == RD_CONN) {
            for (int i = 0; i < carCount && !held; i++) if (cars[i].onRoad && cars[i].holdNode == s) held = true;
            if (!held) continue;
        }
        Color col = held ? Fade(GREEN, 0.7f) : Fade(WHITE, 0.18f);
        for (int i = 0; i + 1 < sg->n; i += 2) {
            int k = sg->p0 + i, k2 = i + 2 < sg->n ? k + 2 : sg->p0 + sg->n - 1;
            Vector2 a = CarScreen(rdX[k], rdY[k]), b = CarScreen(rdX[k2], rdY[k2]);
            if ((a.x < 0 && b.x < 0) || (a.y < 0 && b.y < 0) || (a.x > sw && b.x > sw) || (a.y > sh && b.y > sh)) continue;
            DrawLineEx(a, b, held ? 2.0f : 1.0f, col);
        }
    }
}

// Traffic lights: a dot at each stop line of a lit junction (always drawn)
static void DrawTrafficLights(void) {
    for (int s = 0; s < rdSegCount; s++) {
        const RdSeg* sg = &rdSegs[s];
        if (sg->kind != RD_LANE) continue;
        {
            float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
            Vector2 e = CarScreen(rdX[sg->p0 + sg->n - 1], rdY[sg->p0 + sg->n - 1]);
            if (e.x < -20 || e.y < -20 || e.x > sw + 20 || e.y > sh + 20) continue;
        }
        int st = TrafficLightAt(sg->to, s);
        if (st < 0) continue;
        int k = sg->p0 + sg->n - 1;
        {   // by level, as the cars: not above the view, not under a hill or deck
            int lvl = (int)floorf(rdZ[k] + 0.5f);
            if (lvl > currentViewZ) continue;
            if (lvl < currentViewZ && !IsCellVisibleFromAbove((int)(rdX[k] / CELL_SIZE), (int)(rdY[k] / CELL_SIZE), lvl + 1, currentViewZ + 1)) continue;
        }
        float h = atan2f(rdY[k] - rdY[k - 1], rdX[k] - rdX[k - 1]);
        // on the kerb side of the stop line
        Vector2 p = CarScreen(rdX[k] - sinf(h) * 1.6f * CELL_SIZE, rdY[k] + cosf(h) * 1.6f * CELL_SIZE);
        Color c = st == 2 ? (Color){60, 220, 80, 255} : (st == 1 ? (Color){250, 190, 40, 255} : (Color){230, 40, 40, 255});
        float r = fmaxf(2.5f, 0.35f * CELL_SIZE * zoom);
        if (r < 4) { DrawRectangle((int)(p.x - r), (int)(p.y - r), (int)(2 * r), (int)(2 * r), c); continue; }   // far out: a square
        DrawCircleV(p, r + 1, BLACK);
        DrawCircleV(p, r, c);
    }
}

// One atlas tile of a car: u forward / v to the right of travel (px from the car's
// centre, in the car's frame), size lu x lv px, rotated with the car
static void CarTileAt(float cx, float cy, float heading, int sprite, float u, float v, float lu, float lv, Color tint) {
    float ch = cosf(heading), sh = sinf(heading);
    float wx = cx + ch * u - sh * v, wy = cy + sh * u + ch * v;
    Vector2 p = CarScreen(wx, wy);
    Rectangle src = SpriteGetRect(sprite);
    Rectangle dst = {p.x, p.y, lu * zoom, lv * zoom};
    DrawTexturePro(atlas, src, dst, (Vector2){lu * zoom * 0.5f, lv * zoom * 0.5f}, heading * RAD2DEG, tint);
}
#define CarTile(c, sprite, u, v, lu, lv, tint) CarTileAt((c)->x, (c)->y, (c)->heading, sprite, u, v, lu, lv, tint)

// The car from its atlas tiles (worksheet8x8.ase: car-frame-*, car-seat), drawn facing
// right (east) at heading 0: a narrow car (<= 1.25 cells) from the 1x2 frame (back + front
// tile, one seat), a wider one from the 2x2 frame (four tiles, two seats). Frames take the
// car's colour; seats don't.
static void DrawCarSpriteAt(float x, float y, float heading, Color col);
static void DrawCarSprite(const Car* c, Color col) { DrawCarSpriteAt(c->x, c->y, c->heading, col); }
static void DrawCarSpriteAt(float x, float y, float heading, Color col) {
    Car tmp;   // CarTile reads x, y, heading
    tmp.x = x; tmp.y = y; tmp.heading = heading;
    const Car* c = &tmp;
    float L = carLength * CELL_SIZE, W = carWidth * CELL_SIZE;
    float hu = L * 0.25f;   // tile centres a quarter of the length from the middle
    if (carWidth <= 1.25f) {
        CarTile(c, SPRITE_car_frame_1x1_back, -hu, 0, L * 0.5f, W, col);
        CarTile(c, SPRITE_car_frame_1x1_front, hu, 0, L * 0.5f, W, col);
        CarTile(c, SPRITE_car_seat, -hu, 0, L * 0.5f, W, WHITE);
    } else {
        float hv = W * 0.25f;   // left = -v (north at heading 0: the sprite's top row)
        CarTile(c, SPRITE_car_frame_2x2_back_left, -hu, -hv, L * 0.5f, W * 0.5f, col);
        CarTile(c, SPRITE_car_frame_2x2_front_left, hu, -hv, L * 0.5f, W * 0.5f, col);
        CarTile(c, SPRITE_car_frame_2x2_back_right, -hu, hv, L * 0.5f, W * 0.5f, col);
        CarTile(c, SPRITE_car_frame_2x2_front_right, hu, hv, L * 0.5f, W * 0.5f, col);
        CarTile(c, SPRITE_car_seat, -hu, -hv, L * 0.5f, W * 0.5f, WHITE);
        CarTile(c, SPRITE_car_seat, -hu, hv, L * 0.5f, W * 0.5f, WHITE);
    }
}

// The slot traffic: poses only for cars on screen
// A small lamp on a car: (u, v) cells-px along / right of its heading from its centre
static void CarLamp(float x, float y, float h, float u, float v, float size, Color col) {
    float ch = cosf(h), sh = sinf(h);
    Vector2 p = CarScreen(x + ch * u - sh * v, y + sh * u + ch * v);
    float s = size * zoom;
    DrawRectanglePro((Rectangle){p.x, p.y, s, s}, (Vector2){s * 0.5f, s * 0.5f}, h * RAD2DEG, col);
}

static void DrawSlotCars(void) {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float x0 = -offset.x / zoom, y0 = -offset.y / zoom, x1 = (sw - offset.x) / zoom, y1 = (sh - offset.y) / zoom;
    SlotsSetView(x0, y0, x1, y1);   // real cars (carSlotReal) only here
    float L = carLength * CELL_SIZE, W = carWidth * CELL_SIZE, carPx = L * zoom;
    for (int i = 0; i < slotCarCount; i++) {
        if (!SlotCarVisible(i, x0, y0, x1, y1)) continue;
        float x, y, h, st;
        SlotCarDrawPose(i, &x, &y, &h, &st);
        Vector2 sc = CarScreen(x, y);
        if (sc.x < -carPx || sc.y < -carPx || sc.x > sw + carPx || sc.y > sh + carPx) continue;
        Color col = slotCars[i].cab ? CAB_YELLOW : carColors[i % (int)(sizeof(carColors) / sizeof(carColors[0]))];
        // by level, like the movers: above the view hidden; below it darker, hidden under a deck or ground
        int lvl = (int)floorf(SlotCarZ(i) + 0.5f);
        if (lvl > currentViewZ) continue;
        if (lvl < currentViewZ) {
            // (cars are drawn after the world: a deck at the viewed level itself covers them too)
            if (!IsCellVisibleFromAbove((int)(x / CELL_SIZE), (int)(y / CELL_SIZE), lvl + 1, currentViewZ + 1)) continue;
            float k = powf(0.6f, (float)(currentViewZ - lvl));
            col = (Color){(unsigned char)(col.r * k), (unsigned char)(col.g * k), (unsigned char)(col.b * k), col.a};
        }
        bool driven = SlotCarDriven(i);   // a mover is driving it: its head on the seat
        if (carPx < 3) { DrawRectangle((int)sc.x - 1, (int)sc.y - 1, 2, 2, col); continue; }
        if (carPx < 8) {
            Rectangle r = {sc.x, sc.y, (L - 2) * zoom, (W - 4) * zoom};
            DrawRectanglePro(r, (Vector2){r.width * 0.5f, r.height * 0.5f}, h * RAD2DEG, col);
            continue;
        }
        DrawCarSpriteAt(x, y, h, col);
        if (driven) {   // the driver: the mover's head sprite on the left seat
            float ch = cosf(h), sh = sinf(h), u = -L * 0.25f, v = carWidth <= 1.25f ? 0 : -W * 0.25f;
            Vector2 hp = CarScreen(x + ch * u - sh * v, y + sh * u + ch * v);
            float ms = CELL_SIZE * MOVER_SIZE * zoom;
            DrawTexturePro(atlas, SpriteGetRect(SPRITE_head), (Rectangle){hp.x - ms * 0.5f, hp.y - ms * 0.5f, ms, ms}, (Vector2){0, 0}, 0, WHITE);
        }
        // indicators (amber, blinking) and brake lights
        int sig = SlotCarSignals(i);
        float fu = L * 0.5f - 1.5f, ru = -L * 0.5f + 1.5f, sv = W * 0.5f - 1.5f, lamp = fmaxf(2.0f, CELL_SIZE * 0.22f);
        if (sig & SL_SIG_BRAKE) {
            CarLamp(x, y, h, ru, -sv + 1.0f, lamp, (Color){255, 30, 30, 255});
            CarLamp(x, y, h, ru, sv - 1.0f, lamp, (Color){255, 30, 30, 255});
        }
        if ((sig & (SL_SIG_LEFT | SL_SIG_RIGHT)) && fmodf((float)GetTime(), 0.7f) < 0.4f) {
            Color amber = {255, 170, 0, 255};
            if (sig & SL_SIG_LEFT) { CarLamp(x, y, h, fu, -sv, lamp, amber); CarLamp(x, y, h, ru, -sv, lamp, amber); }
            if (sig & SL_SIG_RIGHT) { CarLamp(x, y, h, fu, sv, lamp, amber); CarLamp(x, y, h, ru, sv, lamp, amber); }
        }
    }
}

// Crossings: white stripes along the road (every other cell across it), drawn before the movers
void DrawCarsGround(void) {
    int n = SlotsCrossingCount();
    if (n == 0 || slotCarCount == 0) return;
    int lvl = (int)rdBaseZ;
    if (lvl > currentViewZ) return;
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight(), cs = CELL_SIZE * zoom;
    Color white = lvl < currentViewZ ? (Color){150, 150, 150, 200} : (Color){235, 235, 225, 210};
    for (int i = 0; i < n; i++) {
        int x, y, o = SlotsCrossingCell(i, &x, &y);
        if ((o == 1 ? y : x) % 2) continue;
        Vector2 p = CarScreen(x * CELL_SIZE, y * CELL_SIZE);
        if (p.x < -cs || p.y < -cs || p.x > sw || p.y > sh) continue;
        if (lvl < currentViewZ && !IsCellVisibleFromAbove(x, y, lvl + 1, currentViewZ + 1)) continue;
        if (o == 1) DrawRectangleRec((Rectangle){p.x, p.y + cs * 0.2f, cs, cs * 0.6f}, white);
        else DrawRectangleRec((Rectangle){p.x + cs * 0.2f, p.y, cs * 0.6f, cs}, white);
    }
}

void DrawCars(void) {
    if (carCount == 0 && slotCarCount == 0) return;
    if (carCount == 0) {
        PROFILE_BEGIN(DrawCars);
        if (rdSegCount > 0) DrawTrafficLights();
        if (carDebug && carLength * CELL_SIZE * zoom >= 6) DrawRoadsDebug();
        DrawSlotCars();
        PROFILE_END(DrawCars);
        return;
    }
    PROFILE_BEGIN(DrawCars);
    if (rdSegCount > 0) DrawTrafficLights();
    bool roads = rdSegCount > 0;
    float L = carLength * CELL_SIZE, W = carWidth * CELL_SIZE;
    float carPx = L * zoom;   // a car's length on screen: picks the level of detail
    if (carDebug && !roads) DrawCarClearance(currentViewZ);
    if (carDebug && roads && carPx >= 6) DrawRoadsDebug();
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight(), margin = carPx + 4;
    for (int i = 0; i < carCount; i++) {
        Car* c = &cars[i];
        if (!c->active || c->z != currentViewZ) continue;
        Vector2 sc = CarScreen(c->x, c->y);
        if (sc.x < -margin || sc.y < -margin || sc.x > sw + margin || sc.y > sh + margin) continue;   // off screen
        Color col = c->cab ? CAB_YELLOW : carColors[i % (int)(sizeof(carColors) / sizeof(carColors[0]))];
        // Far out: one square / one rectangle per car (wheels and glass don't show anyway)
        if (carPx < 3) {
            DrawRectangle((int)sc.x - 1, (int)sc.y - 1, 2, 2, col);
            continue;
        }
        if (carPx < 8) {
            CarPart(c, 0, 0, L - 2, W - 4, 0, col);
            continue;
        }
        if (carDebug && !c->onRoad) DrawCarDebug(c, col);
        if (carDebug && c->waitNow > 0.5f && c->whyCar >= 0) {
            Vector2 a = {offset.x + c->x * zoom, offset.y + c->y * zoom};
            Vector2 b = {offset.x + cars[c->whyCar].x * zoom, offset.y + cars[c->whyCar].y * zoom};
            DrawLineEx(a, b, 2.0f, RED);
            if (c->waitNow >= CAR_DEADLOCK_S) DrawCircleLinesV(a, CELL_SIZE * 1.6f * zoom, RED);
        }
        DrawCarSprite(c, col);
        // Reversing lights
        if (c->speed < -1.0f) {
            CarPart(c, -L * 0.5f + 2, -W * 0.3f, 3, 4, 0, WHITE);
            CarPart(c, -L * 0.5f + 2, W * 0.3f, 3, 4, 0, WHITE);
        }
        // Road cars: why it brakes (green free, orange following, red waiting for a
        // junction, yellow slowing for a bend) - like trackgen's traffic debug
        if (carDebug && c->onRoad) {
            static const Color whyCol[] = {{80, 220, 90, 255}, {255, 150, 40, 255}, {235, 50, 50, 255}, {240, 220, 60, 255}};
            Color wc = whyCol[c->why >= 0 && c->why < 4 ? c->why : 0];
            float r = fmaxf(2.0f, 0.3f * CELL_SIZE * zoom);
            DrawRectangle((int)(sc.x - r * 0.5f), (int)(sc.y - r * 0.5f), (int)r, (int)r, wc);   // (a circle is many triangles)
        }
    }
    PROFILE_END(DrawCars);
}

// Camera follows car 0 (setting carFollow), eased like the mover follow
// (slot traffic: follows mover 0, a driver - walking to its car, driving, walking on)
void CarsFollowCamera(float frameTime) {
    float fx, fy;
    if (slotCarCount > 0 && moverCount > 0 && movers[0].active) { fx = movers[0].x; fy = movers[0].y; }
    else if (carCount >= 1 && cars[0].active) { fx = cars[0].x; fy = cars[0].y; }
    else return;
    float t = 6.0f * frameTime;
    if (t > 1.0f) t = 1.0f;
    float targetX = GetScreenWidth() * 0.5f - fx * zoom;
    float targetY = GetScreenHeight() * 0.5f - fy * zoom;
    offset.x += (targetX - offset.x) * t;
    offset.y += (targetY - offset.y) * t;
    if (carCount >= 1 && slotCarCount == 0) currentViewZ = cars[0].z;
}
