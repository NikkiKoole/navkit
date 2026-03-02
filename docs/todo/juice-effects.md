# Juice Effects

> Status: partial

> Small feedback effects that make interactions feel satisfying. No gameplay impact — pure feel.

## What Already Exists

- **Screen shake system**: `TriggerScreenShake(intensity, duration)` in game_state.h + debug S/M/L buttons in dev panel
- **Tree felling shake**: 4.0/0.3s full tree, 1.0/0.15s young tree (`designations.c:CompleteChopDesignation`)
- **Lightning flash**: full-screen white overlay with fast decay (`DrawLightningFlash` in rendering.c, max 180 alpha)
- **Lightning strikes**: timer-based during thunderstorms, targets exposed flammable cells, can start fires (`UpdateLightning` in weather.c)
- **Cloud shadows**: multi-scale noise scrolling with wind, darkening varies by weather type (rendering.c)

## Ideas

### Screen Shake

- **Thunder**: shake on lightning strike (~3.0/0.3s) — lightning flash exists but no shake yet, easy add in `TryLightningStrike`
- **Mining complete**: small shake when wall breaks (~1.5/0.15s)
- **Large construction complete**: subtle shake when a big wall finishes (~1.0/0.1s)

### Sprite Effects

- **Item drop bounce**: squash & stretch tween when items land on the ground
- **Job completion pop**: brief scale-up flash on mover sprite when finishing work
- **Needs critical pulse**: throb/pulse mover sprite when starving or exhausted — makes urgency visible without UI
- **Construction snap**: scale pop on newly placed wall/floor (quick overshoot then settle)

### Screen Effects

- **Fire ignition**: subtle orange tint flash when something catches fire nearby
- **Sunrise/sunset**: gentle warm/cool color wash at dawn/dusk transitions

### Particles (future, needs particle system)

- **Mining dust**: small dust cloud at cell when wall breaks
- **Construction dust**: puff when building completes
- **Chopping chips**: wood chip particles while chopping
- **Footstep dust**: tiny puffs when movers walk on dirt/sand
- **Rain splashes**: small splash sprites on exposed ground during rain
- **Fire sparks**: occasional spark particles rising from fires

## Implementation Notes

- Most non-particle effects are 5-10 lines each
- Thunder shake is the easiest win — add `TriggerScreenShake(3.0f, 0.3f)` next to existing `TriggerLightningFlash()` in `TryLightningStrike()`
- Sprite tweens need a per-entity `tweenTimer` float (or a small global tween pool)
- Screen flash can be a fullscreen rect with alpha fade, similar to existing lightning flash / cloud shadow overlay
- Particle system is a bigger investment — defer until several particle effects are wanted
- Keep all juice subtle — the game has a calm colony-sim tone, not an action game
