// Road plan (cars experiment): roads as an editable graph - joining, splitting, crossings,
// removing, the rules check, the network it builds
#include "../vendor/c89spec.h"
#include "../vendor/raylib.h"
#include "../src/entities/road_plan.h"
#include "../src/entities/roads.h"
#include <math.h>
#include <string.h>

static bool test_verbose = false;

static bool Near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

static bool JunctionNear(float x, float y) { return RoadPlanNearestNode(x, y, 0.6f) >= 0; }

describe(road_plan_join) {
    it("one straight road is one road between two junctions") {
        RoadPlanClear();
        expect(RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET) == 1);
        expect(RoadPlanRoads() == 1);
        expect(RoadPlanJunctions() == 2);
    }
    it("two roads crossing get a junction where they cross, both split") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        expect(RoadPlanAdd(100, 50, 100, 150, false, 0, 0, RD_CLS_STREET) == 2);
        expect(RoadPlanRoads() == 4);
        expect(RoadPlanJunctions() == 5);
        expect(JunctionNear(100, 100));
    }
    it("a road ending on a road splits it there (a T)") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(100, 100, 100, 160, false, 0, 0, RD_CLS_STREET);
        expect(RoadPlanRoads() == 3);
        expect(RoadPlanJunctions() == 4);
        expect(JunctionNear(100, 100));
    }
    it("an end near a junction joins it") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(51, 101, 51, 160, false, 0, 0, RD_CLS_STREET);
        expect(RoadPlanRoads() == 2);
        expect(RoadPlanJunctions() == 3);
    }
    it("the same road twice is one road") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        expect(RoadPlanRoads() == 1);
    }
}

describe(road_plan_curves) {
    it("a curved road split by a crossing keeps its shape") {
        RoadPlanClear();
        RoadPlanAdd(0, 0, 100, 0, true, 50, 50, RD_CLS_STREET);    // its middle (t 0.5) at (50, 25)
        RoadPlanAdd(50, -10, 50, 60, false, 0, 0, RD_CLS_STREET);
        expect(JunctionNear(50, 25));
        // the piece from (0,0): its own middle is the whole curve's t = 0.25, (25, 18.75)
        int e = -1;
        for (int i = 0; i < rpEdgeCount; i++)
            if (rpEdges[i].alive && (Near(rpNodes[rpEdges[i].a].x, 0, 0.01f) || Near(rpNodes[rpEdges[i].b].x, 0, 0.01f))) e = i;
        expect(e >= 0);
        float x, y;
        RoadPlanPoint(e, 0.5f, &x, &y);
        expect(Near(x, 25.0f, 0.5f) && Near(y, 18.75f, 0.5f));
    }
    it("the check finds a curve tighter than its class takes") {
        RoadPlanClear();
        RoadPlanAdd(0, 0, 30, 0, true, 15, 60, RD_CLS_HIGHWAY);   // a hairpin for a highway
        char lines[8][128];
        int n = RoadPlanCheck(lines, 8);
        bool radius = false;
        for (int i = 0; i < n; i++) if (strstr(lines[i], "radius")) radius = true;
        expect(radius);
    }
}

describe(road_plan_remove) {
    it("removing the crossing road joins the other one back into one road") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(100, 50, 100, 150, false, 0, 0, RD_CLS_STREET);
        expect(RoadPlanRemoveAt(100, 70, 4));
        expect(RoadPlanRemoveAt(100, 130, 4));
        expect(RoadPlanRoads() == 1);
        expect(RoadPlanJunctions() == 2);
    }
    it("nothing near: nothing removed") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        expect(!RoadPlanRemoveAt(100, 140, 4));
        expect(RoadPlanRoads() == 1);
    }
}

describe(road_plan_check) {
    it("junctions too close, roads too sharp, a dead end") {
        RoadPlanClear();
        RoadPlanAdd(0, 100, 200, 100, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(100, 100, 100, 200, false, 0, 0, RD_CLS_STREET);   // a T at 100
        RoadPlanAdd(110, 100, 110, 0, false, 0, 0, RD_CLS_STREET);     // another 10 cells on
        RoadPlanAdd(0, 100, 60, 80, false, 0, 0, RD_CLS_STREET);       // ~18 degrees off the first at (0,100)
        char lines[16][128];
        int n = RoadPlanCheck(lines, 16);
        bool shortRoad = false, angle = false, dead = false;
        for (int i = 0; i < n; i++) {
            if (strstr(lines[i], "between junctions")) shortRoad = true;
            if (strstr(lines[i], "degrees")) angle = true;
            if (strstr(lines[i], "dead end")) dead = true;
        }
        expect(shortRoad && angle && dead);
    }
    it("a clean crossing has no problems") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(100, 50, 100, 150, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(50, 50, 150, 50, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(50, 150, 150, 150, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(50, 50, 50, 150, false, 0, 0, RD_CLS_STREET);
        RoadPlanAdd(150, 50, 150, 150, false, 0, 0, RD_CLS_STREET);
        char lines[16][128];
        expect(RoadPlanCheck(lines, 16) == 0);
    }
}

describe(road_plan_network) {
    it("builds the network: a junction per junction, a road per road") {
        RoadPlanClear();
        RoadPlanAdd(50, 100, 150, 100, false, 0, 0, RD_CLS_ARTERIAL);
        RoadPlanAdd(100, 50, 100, 150, true, 120, 100, RD_CLS_STREET);
        expect(RoadPlanToNetwork() == RoadPlanRoads());
        expect(rdNodeCount == RoadPlanJunctions());
        int curved = 0, arterial = 0;
        for (int e = 0; e < rdEdgeCount; e++) { if (rdEdges[e].hasCtrl) curved++; if (rdEdges[e].cls == RD_CLS_ARTERIAL) arterial++; }
        expect(curved == 2 && arterial == 2);
    }
    it("a roundabout flag makes a roundabout") {
        RoadPlanClear();
        RoadPlanAdd(20, 100, 180, 100, false, 0, 0, RD_CLS_ARTERIAL);
        RoadPlanAdd(100, 20, 100, 180, false, 0, 0, RD_CLS_ARTERIAL);
        expect(RoadPlanSetRoundabout(100, 100, true));
        RoadPlanToNetwork();
        int ring = 0;
        for (int n = 0; n < rdNodeCount; n++) if (rdNodes[n].ring) ring++;
        expect(ring == 4);
    }
}

int main(int argc, char* argv[]) {
    test_verbose = c89spec_parse_args(argc, argv);
    if (!test_verbose) SetTraceLogLevel(LOG_NONE);
    test(road_plan_join);
    test(road_plan_curves);
    test(road_plan_remove);
    test(road_plan_check);
    test(road_plan_network);
    return summary();
}
