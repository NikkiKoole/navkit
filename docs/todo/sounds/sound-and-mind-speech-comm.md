# Sound and Mind - Speech and Communication

Scope
- Goal: agent-to-agent communication that can surprise you.
- World: discrete 3D grid, with needs like food, danger, home, jobs, and social goals.
- Audio: synth engine can render mixed bird-like and vowel/consonant sounds.

Core Idea
- A "language" only appears if it improves outcomes.
- Design tasks with information asymmetry so a sender can help a receiver.
- Reward coordination, not just noise.

Calls vs Songs (for this track)
- Use short call-like utterances for urgent content: food location, danger, task intent.
- Reserve longer song-like phrases for social bonding, identity, or preferences.
- You can mix them, but give different incentives so they specialize.

Signal Design (lightweight, not full speech)
- Define a small "phoneme palette" of 6-12 tokens.
- Tokens can be a blend of birdy chirps, tweets, and vowel/consonant bursts.
- A "sentence" is a fixed-length sequence of tokens in a short time window.
- This keeps the space learnable while still allowing variation and surprise.

Perception Model (avoid raw audio parsing)
- Each agent exposes its emitted token sequence and a few synth parameters.
- Listeners receive a compact input vector:
- Nearest speaker id, distance, last N tokens, and timing.
- This keeps computation cheap while preserving emergent patterns.

Adaptation Without Heavy Evolution
- Option A: pure cultural learning.
- Each agent maintains a small mapping from task context to token sequence.
- Reward mappings that improve outcomes, decay those that fail.
- Option B: hybrid.
- Slow genetic bias (initial preferences) plus fast cultural learning (imitation).
- This gives long-term drift without huge evolutionary complexity.

Player Communication
- The player can broadcast a token sequence (or a tag) as a "tutor signal".
- Agents can treat player messages as high-status tutor input.
- This lets you "teach" a phrase and see it spread or mutate.

Prototype Experiments
- Food Call.
- One agent sees food, sends a short call.
- Receiver must reach the food faster than chance.
- Predator Alarm.
- One agent spots danger and calls.
- Receiver must move away or take shelter.
- Job Intent.
- Sender chooses a task and calls.
- Nearby agents pick compatible tasks (haul, build, guard).

Minimal Data Structures (conceptual)
```c
typedef struct {
    uint8_t tokens[8];
    uint8_t tokenCount;
    float duration;
    float pitchAvg;
    float timbreAvg;
} Utterance;
```

Success Metrics
- Coordination accuracy above baseline.
- Stability: token usage converges for the same context.
- Diversity: different regions develop dialects.
- Surprise: new conventions arise without hard-coding.

Open Questions
- What contexts are worth encoding first (food, danger, task intent, social status)?
- How many tokens before learning collapses?
- Should agents ever intentionally deceive?

References (start set)
- Iterated learning models of language structure.
- Signaling games and naming games in multi-agent systems.
- Emergent communication in referential games.
