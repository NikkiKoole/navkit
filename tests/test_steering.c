#include "../vendor/c89spec.h"
#include "../experiments/steering/steering.h"
#include <math.h>

// ============================================================================
// Basic Steering Behavior Tests
// ============================================================================

describe(steering_seek_behavior) {
    it("should accelerate toward target") {
        Boid agent = {
            .pos = {100, 100},
            .vel = {0, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f
        };
        
        Vector2 target = {500, 100};  // Target to the right
        
        SteeringOutput out = steering_seek(&agent, target);
        
        // Should produce acceleration toward target (positive x)
        expect(out.linear.x > 0);
    }
    
    it("should reach target over time") {
        Boid agent = {
            .pos = {100, 100},
            .vel = {0, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f
        };
        
        Vector2 target = {300, 100};
        
        // Simulate for 3 seconds
        float dt = 1.0f / 60.0f;
        for (float t = 0; t < 3.0f; t += dt) {
            SteeringOutput out = steering_seek(&agent, target);
            steering_apply(&agent, out, dt);
        }
        
        // Should be close to target
        float dist = steering_vec_distance(agent.pos, target);
        expect(dist < 50.0f);
    }
}

describe(steering_arrive_behavior) {
    it("should slow down when approaching target") {
        Boid agent = {
            .pos = {100, 100},
            .vel = {0, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f
        };
        
        Vector2 target = {300, 100};
        float slowRadius = 100.0f;
        
        // Simulate for 5 seconds
        float dt = 1.0f / 60.0f;
        for (float t = 0; t < 5.0f; t += dt) {
            SteeringOutput out = steering_arrive(&agent, target, slowRadius);
            steering_apply(&agent, out, dt);
        }
        
        // Should be at target and nearly stopped
        float dist = steering_vec_distance(agent.pos, target);
        float speed = steering_vec_length(agent.vel);
        
        expect(dist < 30.0f && speed < 30.0f);
    }
}

describe(steering_flee_behavior) {
    it("should accelerate away from threat") {
        Boid agent = {
            .pos = {100, 100},
            .vel = {0, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f
        };
        
        Vector2 threat = {200, 100};  // Threat to the right
        
        SteeringOutput out = steering_flee(&agent, threat);
        
        // Should produce acceleration away from threat (negative x)
        expect(out.linear.x < 0);
    }
}

describe(steering_wander_behavior) {
    it("should produce non-zero output") {
        Boid agent = {
            .pos = {400, 300},
            .vel = {50, 0},
            .maxSpeed = 150.0f,
            .maxForce = 300.0f
        };
        
        float wanderAngle = 0.0f;
        
        SteeringOutput out = steering_wander(&agent, 30, 60, 0.5f, &wanderAngle);
        
        // Should produce some steering
        float mag = steering_vec_length(out.linear);
        expect(mag > 0);
    }
}

// ============================================================================
// NOTE: Docking tests removed - steering_dock requires explicit orientation
// control which is not part of the basic Boid (pure Reynolds model).
// Use Vehicle for behaviors that need independent orientation.
// ============================================================================

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    // Check for quiet mode flag
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'q') {
            set_quiet_mode(1);
            break;
        }
    }
    
    test(steering_seek_behavior);
    test(steering_arrive_behavior);
    test(steering_flee_behavior);
    test(steering_wander_behavior);
    
    return summary();
}
