// test_notice.c - movers notice each other (src/entities/notice.c)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
static bool test_verbose = false;
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/notice.h"
#include "../src/entities/talk.h"
#include "../src/core/time.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>
#include <math.h>

#define W 24
#define H 12

static void SetupWorld(void) {
    InitTestGrid(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            GRID(x, y, 0) = CELL_WALL;
            SetWallMaterial(x, y, 0, MAT_DIRT);
            GRID(x, y, 1) = CELL_AIR;
            GRID(x, y, 2) = CELL_AIR;
            SET_FLOOR(x, y, 1);
            SET_FLOOR(x, y, 2);
        }
    ClearMovers();
    InitMoverSpatialGrid(W * CELL_SIZE, H * CELL_SIZE);
    noticeEnabled = true;
    noticeRadius = 6.0f;
    noticeEveryTicks = 1;
    gameTime = 0.0;
}

static int AddMover(int cx, int cy, int cz) {
    int idx = moverCount++;
    Point goal = {cx, cy, cz};
    InitMover(&movers[idx], cx * CELL_SIZE + CELL_SIZE * 0.5f, cy * CELL_SIZE + CELL_SIZE * 0.5f,
              (float)cz, goal, 100.0f);
    return idx;
}

static bool Notices(int i, int j) {
    for (int k = 0; k < moverNotice[i].count; k++) if (moverNotice[i].e[k].mover == j) return true;
    return false;
}

static void Look(void) {
    BuildMoverSpatialGrid();
    NoticeTick();
}

// Give mover i a one-waypoint path toward cell (gx, gy)
static void WalkToward(int i, int gx, int gy) {
    moverPaths[i][0] = (Point){gx, gy, (int)movers[i].z};
    movers[i].pathLength = 1;
    movers[i].pathIndex = 0;
}

static void Pass(int i, float hx, float hy, float* sx, float* sy, bool* any) {
    float scale;
    *any = NoticePassSteer(i, hx, hy, sx, sy, &scale);
}

describe(notice_basics) {
    it("two movers in view notice each other") {
        SetupWorld();
        int a = AddMover(5, 5, 1), b = AddMover(8, 5, 1);
        Look();
        expect(Notices(a, b));
        expect(Notices(b, a));
        expect(NoticeTop(a) == b);
    }

    it("a wall between them hides them") {
        SetupWorld();
        for (int y = 0; y < H; y++) GRID(7, y, 1) = CELL_WALL;
        int a = AddMover(5, 5, 1), b = AddMover(9, 5, 1);
        Look();
        expect(!Notices(a, b));
        expect(NoticeTop(a) == -1);
    }

    it("another level is not noticed (v1)") {
        SetupWorld();
        int a = AddMover(5, 5, 1), b = AddMover(6, 5, 2);
        Look();
        expect(!Notices(a, b));
    }

    it("beyond the radius is not noticed") {
        SetupWorld();
        int a = AddMover(2, 5, 1), b = AddMover(12, 5, 1);
        Look();
        expect(!Notices(a, b));
    }

    it("keeps at most NOTICE_MAX, the closest first") {
        SetupWorld();
        int a = AddMover(10, 5, 1);
        int near = AddMover(11, 5, 1);
        for (int k = 0; k < 6; k++) AddMover(13 + (k % 3), 4 + (k / 3) * 2, 1);
        Look();
        expect(moverNotice[a].count == NOTICE_MAX);
        expect(NoticeTop(a) == near);
    }

    it("attention sticks: an equal newcomer does not take the top spot") {
        SetupWorld();
        int a = AddMover(10, 5, 1), b = AddMover(13, 5, 1);
        Look();
        expect(NoticeTop(a) == b);
        int c = AddMover(7, 5, 1);   // same distance on the other side
        Look();
        expect(NoticeTop(a) == b);
        expect(Notices(a, c));
    }

    it("keeps the time it first noticed someone") {
        SetupWorld();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        Look();
        gameTime = 10.0;
        Look();
        expect(moverNotice[a].e[0].mover == b);
        expect(moverNotice[a].e[0].since == 0.0);
    }

    it("noticing off: nobody notices") {
        SetupWorld();
        noticeEnabled = false;
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        (void)b;
        Look();
        expect(moverNotice[a].count == 0);
        expect(NoticeTop(a) == -1);
        noticeEnabled = true;
    }

    it("staggered: each mover looks once per noticeEveryTicks") {
        SetupWorld();
        noticeEveryTicks = 4;
        for (int k = 0; k < 8; k++) AddMover(4 + k, 5, 1);
        BuildMoverSpatialGrid();
        NoticeTick();
        int looked = 0;
        for (int i = 0; i < moverCount; i++) if (moverNotice[i].count > 0) looked++;
        expect(looked == 2);
        for (int t = 0; t < 3; t++) NoticeTick();
        looked = 0;
        for (int i = 0; i < moverCount; i++) if (moverNotice[i].count > 0) looked++;
        expect(looked == 8);
    }
}

describe(polite_passing) {
    it("head-on and dead ahead: both drift to their own right, opposite sides") {
        SetupWorld();
        politePassing = true;
        int a = AddMover(4, 5, 1), b = AddMover(7, 5, 1);   // 3 cells apart, inside passRadius
        WalkToward(a, 20, 5);   // east
        WalkToward(b, 0, 5);    // west
        Look();
        float ax, ay, bx, by; bool ga, gb;
        Pass(a, 1, 0, &ax, &ay, &ga);
        Pass(b, -1, 0, &bx, &by, &gb);
        expect(ga && gb);
        expect(ay > 0.0f);      // east-walker's right is south (y down)
        expect(by < 0.0f);      // west-walker's right is north
    }

    it("already offset: each moves away from the other's side") {
        SetupWorld();
        int a = AddMover(4, 5, 1), b = AddMover(7, 5, 1);
        movers[b].y -= CELL_SIZE * 0.5f;   // b a bit north = on a's left
        WalkToward(a, 20, 5);
        WalkToward(b, 0, 4);
        Look();
        float ax, ay, bx, by; bool ga, gb;
        Pass(a, 1, 0, &ax, &ay, &ga);
        Pass(b, -1, 0, &bx, &by, &gb);
        expect(ga && gb);
        expect(ay > 0.0f);      // a goes south, away from b
        expect(by < 0.0f);      // b goes north, away from a
    }

    it("same direction, behind, standing or far to the side: no giving way") {
        SetupWorld();
        int a = AddMover(4, 5, 1), same = AddMover(7, 5, 1), stand = AddMover(6, 6, 1),
            behind = AddMover(2, 5, 1), wide = AddMover(7, 8, 1);
        WalkToward(a, 20, 5);
        WalkToward(same, 20, 5);
        WalkToward(behind, 0, 5);
        WalkToward(wide, 0, 8);
        (void)stand;
        Look();
        float ax, ay; bool g;
        Pass(a, 1, 0, &ax, &ay, &g);
        expect(!g);
    }

    it("off: nothing") {
        SetupWorld();
        int a = AddMover(4, 5, 1), b = AddMover(7, 5, 1);
        WalkToward(a, 20, 5);
        WalkToward(b, 0, 5);
        Look();
        politePassing = false;
        float ax, ay; bool g;
        Pass(a, 1, 0, &ax, &ay, &g);
        expect(!g);
        politePassing = true;
    }
}

static void TalkSetup(void) {
    SetupWorld();
    ClearTalks();
    dayLength = 60.0f;
    talkEnabled = true;
    talkChance = 1.0f;
    talkStartRadius = 3.0f;
    talkMinSeconds = 5.0f;
    talkMaxSeconds = 5.0f;
    talkCooldown = 30.0f;
}

static void RunTalk(float seconds) {
    int ticks = (int)(seconds * TICK_RATE);
    for (int t = 0; t < ticks; t++) {
        TalkTick(TICK_DT);
        for (int i = 0; i < moverCount; i++) TalkMoverTick(i, TICK_DT);
    }
}

static float Dist(int a, int b) {
    float dx = movers[a].x - movers[b].x, dy = movers[a].y - movers[b].y;
    return sqrtf(dx * dx + dy * dy);
}

describe(stop_to_talk) {
    it("two idle movers close by start talking, stop walking and take spots 1.5 cells apart") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        WalkToward(a, 20, 5);
        Look();
        RunTalk(0.5f);
        expect(MoverTalk(a) >= 0 && MoverTalk(a) == MoverTalk(b));
        expect(movers[a].pathIndex < 0 && movers[a].pathLength == 0);
        expect(TalkPartner(a) == b && TalkPartner(b) == a);
        RunTalk(2.0f);
        expect(fabsf(Dist(a, b) - 1.5f * CELL_SIZE) < 2.0f);
        expect(MoverIsSpeaking(a) != MoverIsSpeaking(b));   // one speaks, one listens
    }

    it("ends after its time, then a cooldown keeps them from starting again at once") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        Look();
        RunTalk(0.5f);
        expect(MoverTalk(a) >= 0);
        RunTalk(5.0f);
        expect(MoverTalk(a) < 0 && MoverTalk(b) < 0);
        Look();
        RunTalk(2.0f);
        expect(MoverTalk(a) < 0);
        expect(talkStartedCount == 1);
    }

    it("a job ends it for that mover, and a talk of one ends") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        Look();
        RunTalk(0.5f);
        expect(MoverTalk(a) >= 0);
        movers[b].currentJobId = 0;
        RunTalk(0.1f);
        expect(MoverTalk(b) < 0);
        expect(MoverTalk(a) < 0);
        movers[b].currentJobId = -1;
    }

    it("a third joins: the three spread out round the middle, ~1.5 cells apart") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1);
        TalkStart(a, b);
        int c = AddMover(6, 7, 1);
        Look();
        RunTalk(0.5f);
        int t = MoverTalk(a);
        expect(t >= 0 && MoverTalk(c) == t);
        expect(talks[t].count == 3);
        RunTalk(3.0f);
        expect(fabsf(Dist(a, b) - 1.5f * CELL_SIZE) < 3.0f);
        expect(fabsf(Dist(b, c) - 1.5f * CELL_SIZE) < 3.0f);
        expect(fabsf(Dist(a, c) - 1.5f * CELL_SIZE) < 3.0f);
    }

    it("one leaves a group of three: the other two close up to a pair") {
        TalkSetup();
        talkMinSeconds = talkMaxSeconds = 60.0f;   // long enough to outlast the test
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1), c = AddMover(6, 7, 1);
        TalkStart(a, b);
        TalkJoin(MoverTalk(a), c);
        RunTalk(3.0f);
        movers[c].currentJobId = 0;
        RunTalk(3.0f);
        expect(MoverTalk(c) < 0);
        expect(MoverTalk(a) >= 0 && talks[MoverTalk(a)].count == 2);
        expect(fabsf(Dist(a, b) - 1.5f * CELL_SIZE) < 3.0f);
        movers[c].currentJobId = -1;
    }

    it("a walker heading at a conversation steers round it; walking away or off: nothing") {
        TalkSetup();
        talkGiveRoom = true;
        int a = AddMover(10, 4, 1), b = AddMover(10, 6, 1), w = AddMover(7, 5, 1);   // 3 cells off
        TalkStart(a, b);
        float sx, sy;
        expect(TalkGiveRoomSteer(w, 1, 0, &sx, &sy));
        expect(sy > 0.0f);                              // dead ahead: goes round on its right (south)
        expect(!TalkGiveRoomSteer(w, -1, 0, &sx, &sy)); // walking away
        expect(!TalkGiveRoomSteer(a, 0, 1, &sx, &sy));  // a member doesn't steer round its own talk
        talkGiveRoom = false;
        expect(!TalkGiveRoomSteer(w, 1, 0, &sx, &sy));
        talkGiveRoom = true;
    }

    it("busy, too far or switched off: no talk") {
        TalkSetup();
        int a = AddMover(5, 5, 1), b = AddMover(7, 5, 1), far = AddMover(5, 10, 1);
        movers[b].currentJobId = 0;
        (void)far;
        Look();
        RunTalk(1.0f);
        expect(MoverTalk(a) < 0);
        movers[b].currentJobId = -1;
        talkEnabled = false;
        Look();
        RunTalk(1.0f);
        expect(MoverTalk(a) < 0);
        talkEnabled = true;
    }
}

describe(gatherings) {
    it("an idle mover in range comes to watch: a spot on the first ring, on its own side, facing in") {
        TalkSetup();
        watchChance = 1.0f;
        watchRadius = 6.0f;
        watchMinSeconds = watchMaxSeconds = 20.0f;
        int p = AddMover(12, 6, 1), w = AddMover(7, 6, 1);   // watcher 5 cells west
        expect(GatheringStart(p, 100.0f) >= 0);
        expect(MoverIsPerforming(p) && MoverIsSpeaking(p));
        RunTalk(0.5f);
        expect(MoverIsWatching(w) && TalkPartner(w) == p);
        RunTalk(4.0f);
        expect(fabsf(Dist(w, p) - 1.5f * CELL_SIZE) < 3.0f);
        expect(movers[w].x < movers[p].x);                    // stayed on the west side
        float lx, ly;
        expect(TalkLookAt(w, &lx, &ly) && fabsf(lx - movers[p].x) < 0.5f);
    }

    it("a wall between: does not come; out of range: does not come") {
        TalkSetup();
        watchChance = 1.0f;
        watchRadius = 4.0f;
        int p = AddMover(12, 6, 1), hidden = AddMover(9, 6, 1), far = AddMover(12, 1, 1);   // 3 and 5 cells
        for (int y = 0; y < H; y++) GRID(10, y, 1) = CELL_WALL;
        GatheringStart(p, 100.0f);
        RunTalk(1.0f);
        expect(!MoverIsWatching(hidden));
        expect(!MoverIsWatching(far));
        watchRadius = 6.0f;
    }

    it("onlookers leave after their time; the performer keeps performing") {
        TalkSetup();
        watchChance = 1.0f;
        watchMinSeconds = watchMaxSeconds = 2.0f;
        talkCooldown = 60.0f;
        int p = AddMover(12, 6, 1), w = AddMover(9, 6, 1);
        GatheringStart(p, 100.0f);
        RunTalk(0.5f);
        expect(MoverIsWatching(w));
        RunTalk(2.5f);
        expect(!MoverIsWatching(w));
        expect(MoverIsPerforming(p));
    }

    it("a full first ring: the next one goes to the second ring") {
        TalkSetup();
        watchChance = 1.0f;
        watchMinSeconds = watchMaxSeconds = 60.0f;
        int p = AddMover(12, 6, 1);
        int t = GatheringStart(p, 100.0f);
        for (int k = 0; k < 10; k++) AddMover(10 + (k % 5), 3 + (k / 5) * 6, 1);   // all within 4 cells
        Look();
        RunTalk(2.0f);
        int ring0 = 0, ring1 = 0;
        for (int k = 0; k < talks[t].count; k++) {
            if (talks[t].spot[k] / 64 == 0) ring0++;
            else if (talks[t].spot[k] / 64 == 1) ring1++;
        }
        expect(talks[t].count == 10);
        expect(ring0 == 9 && ring1 == 1);   // 2*pi*1.5 / 1.0 = 9 spots on the first ring
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(notice_basics);
    test(polite_passing);
    test(stop_to_talk);
    test(gatherings);
    return summary();
}
