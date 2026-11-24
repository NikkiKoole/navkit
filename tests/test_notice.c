// test_notice.c - movers notice each other (src/entities/notice.c)
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
static bool test_verbose = false;
#include "../src/game_state.h"
#include "../src/world/grid.h"
#include "../src/entities/mover.h"
#include "../src/entities/notice.h"
#include "../src/core/time.h"
#include "test_helpers.h"
#include <string.h>

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

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(notice_basics);
    return summary();
}
