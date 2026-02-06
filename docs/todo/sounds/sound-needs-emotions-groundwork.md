# Sound and Mind - Needs and Emotions Groundwork

Purpose
- Provide a light, stable foundation for meaningful communication later.
- Keep the system small enough to ship early.
- Allow gradual expansion without rewriting the sound layer.

Minimum Viable Needs (Phase 0)
- Hunger
- Fatigue
- Safety

Optional Early Needs (Phase 0.5)
- Comfort (bed, chair, shelter)
- Social contact (isolation vs group)

Derived Emotions (Not Simulated Directly)
- Calm: safety high, fatigue low
- Stressed: safety low or stuck
- Hungry: hunger low
- Tired: fatigue low
- Curious: all needs stable and idle

Why Derived Emotions
- No extra simulation loops at the start.
- Easy to debug because emotions are just labels on needs.
- Still expressive enough to shape sound behavior.

Memory (Minimal)
- Short event buffer: last N events (job assigned, stuck, heard call, ate).
- Used for:
  - Avoiding repeated calls
  - Tracking whether a call helped
  - Building simple preferences (who helped me last)

Social Ties (Minimal)
- Single affinity score per pair (0 to 1).
- Increase on cooperation, decrease on conflict.
- Used to bias listening or imitation.

Home and Territory (Minimal)
- Each agent has a home tile or region.
- Home can bias comfort and song preferences later.

Time and Rhythm (Optional)
- Day/night or shift clock.
- Used to create predictable timing for songs or calls.

How This Feeds Sound Later (But Not Yet)
- Needs trigger concept calls when thresholds are crossed.
- Emotions bias frequency, timbre, or token choice.
- Social ties bias who gets copied.

Guardrails
- Do not let needs overwhelm job system yet.
- Keep thresholds coarse and slow-changing.
- Avoid cascading failure loops until stability is proven.

Phased Plan
- Phase 0: hunger, fatigue, safety meters (no behaviors yet).
- Phase 0.5: derived emotions + short memory buffer.
- Phase 1: job intent calls (no need triggers).
- Phase 2+: start linking needs to calls.
