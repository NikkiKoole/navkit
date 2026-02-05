# Sound and Mind - Birdsong and Sexual Selection

Scope
- Goal: evolving or culturally learned song used for mate choice.
- World: discrete 3D grid with agents, home sites, danger, and resources.
- Audio: synth can render bird-like chirps and vowel/consonant blends.

Bird Communication vs Song
- Many birds use short calls for alarms, food, or contact.
- Songs are often longer and tied to mating and territory.
- In the sim, treat calls as urgent communication and songs as courtship.
- You can allow overlap, but different rewards keep them distinct.

Two-Phase Learning (simple version)
- Memorize a tutor song (from nearby adults).
- Practice and match over time (sensorimotor learning).
- This can be implemented without heavy neural nets.

Sexual Selection Loop
- Males (or suitors) emit songs.
- Females (or choosers) evaluate songs with a preference model.
- Mating success drives which songs persist.

Preference Models (simple to complex)
- Simple: prefer certain pitch ranges or rhythm density.
- Moderate: prefer repeating motifs or call-and-response structure.
- Advanced: each chooser has its own evolving preference vector.

Cultural Transmission
- Learners copy local tutors with slight variation.
- Maintain diversity by biasing learners to add small changes.
- This creates local dialects and drift over generations.

Calls and Songs Together
- Keep a short call channel for survival communication.
- Keep a longer song channel for mating and identity.
- Mixing birdy chirps with vowel/consonant bursts can create a unique style.

Prototype Experiments
- Song Preference Test.
- Males generate 2-4 second sequences.
- Females select a mate based on preference score.
- Population Drift Test.
- Split population by distance.
- Expect regional styles to emerge.

Minimal Data Structures (conceptual)
```c
typedef struct {
    float rhythm[16];
    float pitch[16];
    float timbre[16];
    uint8_t length;
} Song;
```

Success Metrics
- Mating success correlates with song traits.
- Regional styles appear and persist.
- Song diversity remains stable over time.

Open Questions
- Should song be the only mating signal or one of several?
- How strong should female choice be compared to resource success?
- How much tutor bias is needed to maintain stability?

References (start set)
- Birdsong learning and social tutoring.
- Sexual selection and song evolution.
- Cultural transmission of song and dialects.
