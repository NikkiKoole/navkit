#include "../vendor/c89spec.h"
#include "../steering/steering.h"
#include <math.h>

// ============================================================================
// Dock Behavior Tests
// ============================================================================

typedef struct {
    Vector2 target;
    float targetOrientation;
    float slowRadius;
    float maxAngularAccel;
    float slowAngle;
} DockContext;

static SteeringOutput dock_behavior(SteeringAgent* agent, void* ctx) {
    DockContext* d = (DockContext*)ctx;
    return steering_dock(agent, d->target, d->targetOrientation,
                         d->slowRadius, d->maxAngularAccel, d->slowAngle);
}

describe(steering_dock_behavior) {
    it("should arrive at dock position facing the correct direction") {
        // Setup: Agent starts at center, dock is to the right opening left
        // Dock opens LEFT (PI), so agent should approach from left, facing RIGHT (0)
        SteeringAgent agent = {
            .pos = {400, 300},
            .vel = {0, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f,
            .orientation = 0,
            .angularVelocity = 0
        };
        
        DockContext ctx = {
            .target = {600, 300},           // Dock position (to the right)
            .targetOrientation = 0,          // Agent should face right (into dock that opens left)
            .slowRadius = 100.0f,
            .maxAngularAccel = 5.0f,
            .slowAngle = 0.5f
        };
        
        // Simulate for 5 seconds (should be plenty of time to dock)
        float dt = 1.0f / 60.0f;
        for (float t = 0; t < 5.0f; t += dt) {
            SteeringOutput out = dock_behavior(&agent, &ctx);
            steering_apply(&agent, out, dt);
        }
        
        // Check: Agent should be at dock position
        float distToTarget = steering_vec_distance(agent.pos, ctx.target);
        int atPosition = distToTarget < 15.0f;
        
        // Check: Agent should be facing the target orientation (within tolerance)
        float angleDiff = fabsf(steering_wrap_angle(agent.orientation - ctx.targetOrientation));
        int correctOrientation = angleDiff < 0.15f;
        
        // Check: Agent should be nearly stopped
        float speed = steering_vec_length(agent.vel);
        int stopped = speed < 10.0f;
        
        expect(atPosition && correctOrientation && stopped);
    }
    
    it("should face velocity direction while approaching (look where going)") {
        // Setup: Agent starts far from dock
        SteeringAgent agent = {
            .pos = {100, 300},
            .vel = {0, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f,
            .orientation = PI,  // Initially facing left (wrong direction)
            .angularVelocity = 0
        };
        
        DockContext ctx = {
            .target = {600, 300},           // Dock far to the right
            .targetOrientation = 0,          // Final orientation: face right
            .slowRadius = 100.0f,
            .maxAngularAccel = 5.0f,
            .slowAngle = 0.5f
        };
        
        // Simulate just 1 second - agent should be mid-approach
        float dt = 1.0f / 60.0f;
        for (float t = 0; t < 1.0f; t += dt) {
            SteeringOutput out = dock_behavior(&agent, &ctx);
            steering_apply(&agent, out, dt);
        }
        
        // Agent should NOT be at dock yet
        float distToTarget = steering_vec_distance(agent.pos, ctx.target);
        int stillApproaching = distToTarget > 50.0f;
        
        // Agent should be facing roughly the same direction as velocity (look where going)
        // Velocity direction = atan2(vel.y, vel.x)
        // Orientation should match this while moving
        float velAngle = atan2f(agent.vel.y, agent.vel.x);
        float angleDiff = fabsf(steering_wrap_angle(agent.orientation - velAngle));
        int facingVelocity = angleDiff < 0.3f;  // Within ~17 degrees
        
        // Agent should be moving (not stopped)
        float speed = steering_vec_length(agent.vel);
        int moving = speed > 20.0f;
        
        expect(stillApproaching && facingVelocity && moving);
    }
    
    it("should approach dock from correct direction based on dock orientation") {
        // Dock opens DOWN (PI/2), agent starts above
        // Agent should approach from above, moving down, facing down
        SteeringAgent agent = {
            .pos = {400, 100},
            .vel = {0, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f,
            .orientation = 0,  // Initially facing right
            .angularVelocity = 0
        };
        
        // Dock at bottom, opens down (PI/2)
        // Agent should face down (-PI/2) to enter
        DockContext ctx = {
            .target = {400, 400},
            .targetOrientation = PI / 2,  // Face down into dock
            .slowRadius = 100.0f,
            .maxAngularAccel = 5.0f,
            .slowAngle = 0.5f
        };
        
        // Simulate until arrival
        float dt = 1.0f / 60.0f;
        for (float t = 0; t < 5.0f; t += dt) {
            SteeringOutput out = dock_behavior(&agent, &ctx);
            steering_apply(&agent, out, dt);
        }
        
        // Check final orientation is facing down (PI/2)
        float angleDiff = fabsf(steering_wrap_angle(agent.orientation - ctx.targetOrientation));
        int correctOrientation = angleDiff < 0.15f;
        
        // Check at position
        float distToTarget = steering_vec_distance(agent.pos, ctx.target);
        int atPosition = distToTarget < 15.0f;
        
        expect(atPosition && correctOrientation);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    test(steering_dock_behavior);
    
    return summary();
}
