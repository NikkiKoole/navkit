# Sound and Mind - System Design

Purpose
- Enable agent communication that surprises the player without heavy ML.
- Keep the system learnable, debuggable, and cheap to run.
- Use the existing synth engine as a vocal renderer, not as a learning target.

Non-Goals
- No full speech recognition or raw audio parsing.
- No large-scale neural training in the core game loop.
- No requirement that signals be human-readable at first.

World Constraints (Current Codebase)
- Discrete 3D grid world with z-levels.
- Job system with explicit job types.
- Movers with stuck detection and repath logic.
- Items and stockpiles on grid tiles.
- Synth engine available for vocal output.

Design Pillars
- Meaningful: signals change outcomes and improve coordination.
- Minimal: tiny token palette, small utterances, simple learning rules.
- Local: learning mostly happens via nearby agents.
- Visible: UI and debug overlays explain what is being "said."

Foundational Systems (Needed For Long-Term Depth)
- Needs: hunger, fatigue, safety, comfort, and social contact.
- Emotions: simple valence/arousal or mood states tied to needs.
- Memory: short-term memory for recent events and recent utterances.
- Attention: a small limit on how many signals can be processed.
- Social ties: affinity or familiarity scores between agents.
- Home and territory: a place to return, defend, or prefer.
- Time and rhythm: day/night or shift cycles for predictable patterns.

Minimum Viable Groundwork (If These Do Not Exist Yet)
- Needs can start as 2 to 3 simple meters (hunger, fatigue, safety).
- Emotions can be derived, not simulated (e.g., "stressed" if safety low).
- Memory can be a tiny ring buffer of last N events.
- Social ties can be a float that only changes on cooperation or conflict.

System Overview
1. Concept Layer
2. Signal Layer
3. Listener Layer
4. Learning Layer
5. Cultural Layer
6. Player Channel

Concept Layer
- Concepts are the "meaning atoms" agents can talk about.
- Concepts map to world events and agent contexts.
- Examples: job intent, stuck danger, food location, home needs.

Concept Registry (Data-Driven)
- Each concept has an id, tags, a trigger condition, and a reward rule.
- Concepts can be added without changing the signal system.
- Concepts can be grouped by priority and urgency.

Concept Types
- Action intent: "I am doing X."
- Location: "Here is X."
- State: "I am stuck," "I am tired."
- Need: "I need food," "I need shelter."
- Social: "I choose you," "I accept."
- Emotion: "I am stressed," "I am calm."

Concept Grounding Rules
- Every concept must map to a world trigger.
- Every concept must have a measurable outcome for reward or decay.
- Concepts that are not grounded should be ignored or queued for later.

Adding New Concepts (Chair, Bed, Table, Etc.)
- Add a concept entry for the object or action.
- Bind it to world detection rules.
- Optionally add listener responses.
- Add or reuse tokens for its utterance.
- No new system architecture required.

Signal Layer
- Use a tiny token palette, 6 to 12 tokens.
- Tokens map to synth presets.
- Tokens are not "words." They are just stable signal units.

Utterances
- A short fixed-length sequence of tokens.
- A call is 3 to 6 tokens.
- A song is 8 to 20 tokens.
- Timing can be regular or patterned.

Signal Channels
- Call channel: urgent, short, information dense.
- Song channel: social, identity, courtship.
- Mixed channel: optional, for stylistic blending.

Listener Layer
- Listeners do not parse audio.
- Listeners receive token sequences and metadata.
- Inputs include speaker id, distance, last tokens, and timing.

Listener Gating
- Only respond if the context matches.
- Only respond if idle or if response is high priority.
- Avoid "everyone reacts to everything."

Learning Layer
- Each agent tracks scores for utterance choices per concept.
- Reward when a response improves outcomes.
- Decay when a response fails.
- Pick utterances with a bit of exploration to avoid stagnation.

Cultural Layer
- Agents sometimes copy successful neighbor utterances.
- A small mutation rate creates drift.
- Local imitation creates dialects.

Player Channel
- The player can emit a tutor utterance.
- Tutor signals have higher weight in learning.
- This lets the player seed a phrase and watch it spread or mutate.

Audio Mapping
- Each token maps to a synth preset.
- Presets can be bird-like, vowel-like, or consonant-like.
- Tokens keep a consistent identity so learning is stable.

Initial Concepts (Priority Order)
- Job Intent
- Stuck Danger
- Food Location

Job Intent Concept
- Trigger: mover assigned a job.
- Signal: job type plus optional direction bucket.
- Listener response: idle movers prefer complementary jobs.
- Reward: reduced idle time, fewer job conflicts.

Stuck Danger Concept
- Trigger: mover timeWithoutProgress exceeds threshold.
- Signal: distress call with location hint.
- Listener response: avoid area or assist by reducing congestion.
- Reward: faster recovery from stuck state.

Food Location Concept
- Trigger: food item seen or stockpile surplus.
- Signal: location or resource type.
- Listener response: hungry movers route to location.
- Reward: faster consumption, less starvation risk.

Birdsong and Sex Track
- Song sequences used primarily for mate choice.
- Two-phase learning: memorize a tutor song, then practice.
- Preferences can be simple numeric vectors.
- Cultural drift creates regional styles.

Mixing Birdsong and Speech
- Calls can include chirps, tweets, and vowel bursts.
- Songs can include consonant-like rhythms.
- Different rewards keep the channels meaningful.

Data Structures (Conceptual)
```c
typedef enum {
    CONCEPT_JOB_INTENT,
    CONCEPT_STUCK_DANGER,
    CONCEPT_FOOD_LOCATION,
    CONCEPT_COUNT
} ConceptId;

typedef struct {
    uint8_t tokens[16];
    uint8_t tokenCount;
    float duration;
    float score;
} Utterance;

typedef struct {
    ConceptId id;
    float basePriority;
    float cooldown;
} ConceptSpec;
```

Integration Points (Future)
- Job assignment: emit job intent when assigned.
- Mover update: emit stuck danger when stuck threshold reached.
- Item system: emit food location on discovery.
- Synth: play token presets for utterances.
- UI: show glyphs or text labels for active utterances.
- Needs system: emit need-state calls when thresholds crossed.
- Emotion layer: allow mood to bias call frequency and token choice.

Metrics and Debugging
- Coordination success rate per concept.
- Average time saved per response.
- Dialect spread by region.
- Frequency of utterance usage.

Risks and Guardrails
- Spam: add cooldowns and per-concept rate limits.
- Noise: reward only when outcomes improve.
- Monoculture: keep small mutation and local imitation.

Phased Delivery Plan (For Later)
- Phase 0: minimal needs and emotion scaffolding.
- Phase 0.5: memory buffer and simple social affinity.
- Phase 1: job intent with simple calls.
- Phase 2: stuck danger and distress response.
- Phase 3: food location calls.
- Phase 4: songs and mate choice.
- Phase 5: player tutor signals and regional dialects.

Concept Generator (Keep Concepts From Going Missing)
- Goal: automatically surface new "things" so the concept system stays complete.
- Approach: scan authoritative sources and generate a coverage report.

Authoritative Sources To Scan
- `src/entities/items.h` for ItemType (future chairs/beds/tables as items).
- `src/entities/jobs.h` for JobType (job intent concepts).
- `src/entities/workshops.h` for WorkshopType and recipes (crafting intent concepts).
- `src/world/grid.h` for CellType (terrain/location concepts).
- `src/world/designations.h` for DesignationType (build/dig intent concepts).
- `src/world/material.h` for MaterialType (resource concepts).

Generated Outputs (Concept Coverage)
- `docs/todo/sounds/concepts/coverage.md`
- One row per enum value, with:
- Concept id, source enum, tags, status (mapped or TODO), and notes.

Lint Rule
- Build (or CI) warns if a new enum value has no concept mapping.
- This prevents "missing concepts" when new objects are added.

Human Override
- Some enum values should be ignored or merged.
- Provide a small ignore/alias list in the generator config.

Result
- When you add chair/bed/table, the generator flags the missing concept mapping.
- You choose to map it to a call, a song, or ignore it.
