# Video Format Guide

> How to structure short-form video (TikTok/Reels/Shorts) and devlogs for NavKit.
> Based on research + the "8,000 wishlists in 3 days" breakdown from r/SoloDevelopment.

---

## General Principles: What Makes Short-Form Content Work

Short-form video is a compression game. You're compressing value — entertainment, information, emotion — into the smallest possible time window. Everything that works at scale shares the same DNA, regardless of what you're showing.

**The core loop is: interrupt → hook → deliver → loop.** These are four distinct beats, not synonyms:

- **Interrupt** (~0.5s, frame 1) — The thing that makes the thumb stop scrolling. Purely visual/sensory. A sudden movement, bright flash, unexpected image. The viewer hasn't decided to watch — their lizard brain reacted to something different in the feed. For NavKit: water already mid-crash, fire already burning. Not "fire is about to start" — fire is already happening.
- **Hook** (0.5-3s) — Now that they've paused, you give their conscious brain a reason to stay. Text overlay or a curiosity gap: "Watch what happens when I dig here..." Their brain opens a loop it wants to close. The interrupt bought attention, the hook converts it into *intention to watch*.
- **Deliver** (3-20s) — Pay off the promise. The water floods, the fire spreads, the colony collapses. If the hook promised something interesting, the delivery must match or exceed it. Broken promises = viewer feels cheated = bad retention signal.
- **Loop** (last 1-2s) — The ending visually or tonally connects back to the beginning so the viewer doesn't notice the restart. They watch again = the platform counts another view = algorithmic boost. End on a frame similar to your opening, or cut abruptly before a "what happens next" moment so the restart feels like the answer.

**Interrupt and hook are different things.** Most people conflate them. The interrupt is involuntary (lizard brain), the hook is voluntary (curiosity). A visually striking frame that doesn't raise a question gets a pause but not a watch. A great question over a boring frame never gets seen because nobody stopped scrolling. You need both.

This works for games, for cooking, for woodworking, for anything. The subject doesn't matter — what matters is whether you can compress a moment of genuine interest into a form the algorithm can measure (retention, loop completion, shares). People share things that made them feel something: surprise, satisfaction, amusement, awe. If your clip creates one of those feelings in 15 seconds, it will travel.

**How autoplay changes everything:** TikTok, Reels, and Shorts all autoplay as you scroll. There is no click, no thumbnail, no commit moment. The video is already playing as it slides into view. The user's feed is a continuous stream of videos playing one after another. The viewer isn't choosing to watch — they're choosing *not to scroll past*. Your first frames are already animating while their thumb is still in motion.

This is fundamentally different from YouTube (traditional) or a Steam page, where someone sees a static thumbnail and decides to click. On short-form: there is no thumbnail — the video IS the thumbnail. Frame 1 is your entire pitch. Text overlays are visible immediately, rendered on top of the already-playing video (readable before the viewer has even processed what's happening visually). Sound is on by default (TikTok especially), so audio hits immediately too.

**Practical implication for recording:** Set up the interesting thing, start recording, then trigger it. But when you edit, cut everything before the action starts. Your raw recording might be: 3 seconds of setup → water breaks through. Your final edit should start at the exact frame the water breaks through. If your first 10 frames are a black screen, a logo, or an empty landscape — the viewer has already scrolled past before the interesting part starts. They literally never saw it.

**The uncomfortable truth about virality:** You cannot engineer a single viral video. What you can engineer is a *process* that gives you many shots on goal. The people who go viral aren't luckier — they posted 50 clips and one hit. The Reddit poster who got 8,000 wishlists didn't get lucky with their first upload. They studied patterns, edited deliberately, and kept the channel active. Volume + quality of edit + a genuinely interesting subject = eventual breakout.

**Why "just post gameplay" doesn't work:** Raw gameplay is optimized for the player, not the viewer. The viewer doesn't have context, doesn't care about your UI, and will scroll past anything that takes more than 2 seconds to become interesting. You have to re-edit gameplay into something that works as a standalone piece of entertainment. The footage is raw material — the edit is the product.

**Platform-agnostic truth:** TikTok, Reels, and Shorts all reward the same thing: content that keeps people on the platform. They measure this through retention curves and engagement (likes, shares, comments, saves). If your video keeps people watching and makes them interact, every platform will push it. The specific algorithm details change quarterly, but the underlying incentive never changes: keep people scrolling, keep people watching.

**The long game:** Short-form can spike wishlists or attention, but it doesn't build lasting community on its own. The people who discover you through a viral clip need somewhere to go — a Steam page, a Discord, a YouTube channel with more depth. Short-form is the top of the funnel. It catches strangers. Everything else (devlogs, community, deeper content) converts strangers into followers, and followers into buyers. You need both, but short-form is where cold audiences find you.

---

## The Algorithm: What You're Actually Optimizing For

The algorithm doesn't care about your game. It cares about **retention**. Specifically:

- **3-second retention** — 71% of watch/skip decisions happen here. Videos where >50% of viewers drop in the first 3s almost never escape the initial test batch of 200-500 viewers.
- **Qualified views** (5 seconds) — TikTok now also tracks 5-second holds. Your hook needs to survive both gates.
- **Loop completion** — Videos that loop seamlessly count as multiple views. Short + loopable = algorithmic boost.
- **65/45 rule** — 65% of people who watch 3 seconds will watch 10. 45% will watch 30. Win the first 3 seconds and the rest follows.

**Target: 65%+ retention at the 3-second mark.** Videos hitting this get 4-7x more impressions than those that don't.

---

## The 3-Layer Hook

Every scroll-stopping video has three hooks firing simultaneously in the first 1-2 seconds:

### 1. Visual Hook (what they SEE)
The thing that makes their thumb stop moving. For NavKit:
- Water crashing through a wall
- Fire engulfing a forest
- The cross-section "ant farm" view of a colony
- Snow accumulating, then melting into mud

**Rule: Start mid-action.** Never start with a still frame, a logo, or an empty world. Frame 1 should already have something happening.

### 2. Text Hook (what they READ)
On-screen text that creates a curiosity gap or frames the visual. Examples:
- "I simulated water physics in C"
- "Watch what happens when I dig here..."
- "This is what 50,000 lines of code looks like"
- "My settlers keep drowning and it's my fault"

**Rule: Readable in under 2 seconds.** Short words, large font, high contrast. No more than 8-10 words.

### 3. Audio Hook (what they HEAR)
Music or sound that matches the energy, NOT necessarily a trending sound.
- Ambient/eerie for sim showcases (water, fire)
- Comedic timing beats for fail moments
- Something building/cinematic for colony growth timelapses

**Rule: The music must fit YOUR clip's pacing.** The Reddit poster confirmed: trend-chasing audio doesn't help if it clashes with the edit. Match energy, not trends.

---

## Hook Psychology: Why People Stop Scrolling

Seven proven triggers. Pick 1-2 per video, not all of them:

| Trigger | How it works | NavKit example |
|---------|-------------|----------------|
| **Pattern interrupt** | Something visually unexpected breaks the scroll | Water flooding from an unexpected direction |
| **Curiosity gap** | Incomplete information the brain wants to resolve | "I told my coggies to dig here..." (cut before result) |
| **Bold claim** | Challenges what viewer assumes | "No engine. No garbage collector. Just C." |
| **Before/after** | Dramatic visual contrast | Empty grass → thriving multi-level colony |
| **Relatable frustration** | Shared experience | "My coggy starved 2 tiles from the berry bush" |
| **The impossible result** | Surprising outcome | "8x8 pixels. Full water physics." |
| **Behind-the-scenes** | Process transparency | "Here's how fire spread actually works in my code" |

---

## Video Structures (Templates)

### Template 1: The Chaos Moment (15-30s)
Best for: TikTok/Reels. Lowest effort, highest viral potential.

```
[0-1s]  Mid-action visual (water/fire/something breaking)
[1-3s]  Text overlay: "I told my coggies to dig here..."
[3-10s] The chaos unfolds (flooding, fire spreading, colony collapsing)
[10-15s] Optional: aftermath / comedy beat
[last 2s] Text: game name + "wishlist on Steam"
```

**Loop trick:** End on a frame that visually connects back to the opening. The viewer doesn't notice the restart and watches again = 2x view count.

### Template 2: The Simulation Showcase (15-30s)
Best for: Satisfying loop content. Water, fire, snow, smoke.

```
[0-1s]  Close-up of the simulation starting (water source, fire igniting)
[1-3s]  Text: "Water physics in my colony sim" or "I coded this in C"
[3-20s] Let the simulation play out — satisfying spread/flow
[20-25s] Pull back to show full scale
[last 2s] Game name + CTA
```

**Key: No narration needed.** The simulation IS the content. Music carries the mood.

### Template 3: The Dev Insight (30-60s)
Best for: Technical audience, r/gamedev crowd, YouTube Shorts.

```
[0-2s]  The result first (show the feature working)
[2-5s]  Text: "How I built [X] in C with no engine"
[5-30s] Quick explanation with gameplay footage (text overlays, not voiceover)
[30-45s] Show an edge case or funny emergent behavior
[last 5s] "Following the journey" + game name + CTA
```

### Template 4: The Devlog (2-8 min, YouTube)
Best for: Building a following over time, long-tail discovery.

```
[0-5s]   THE RESULT FIRST — show the finished feature immediately
[5-15s]  Quick context: "I've been building a colony sim in C..."
[15s-5m] The meat: how you built it, decisions, show the code briefly, show it running
[~5m]    Something going wrong or an emergent moment (humor)
[last 30s] What's next + wishlist CTA
```

**Never open with:** "Hey guys, welcome back to my devlog" — YouTube punishes slow starts. Show the coolest thing first, THEN explain.

---

## Reading Between the Lines: The Unspoken Rules

Things the platforms don't officially say but the data confirms:

### Content suppression
The Reddit poster warned about "problematic" words. TikTok/IG silently suppress content containing:
- Violence words in text overlays (death, kill, blood, gun, etc.)
- Even implied violence shown on screen
- **For NavKit:** Don't write "fire kills settlers" → write "fire reached the colony." Don't write "movers died" → write "movers didn't make it." Soft framing, same content.

### The US distribution problem
If you're not US-based, TikTok may not push your content to the largest audience. The Reddit poster's solution was having a US-based person publish from their account. But they emphasized: **distribution without a good edit = nothing.** Fix the edit first, worry about distribution second.

### Posting cadence
- TikTok/Reels: 3-5x per week minimum to stay in the algorithm's good graces
- But quality > quantity. 2 good clips > 5 mediocre ones
- The Reddit poster: "I have to keep posting to keep the channel active"
- Reuse content across platforms but **re-upload natively** (don't crosspost with watermarks)

### The "viral-friendly" filter
The Reddit poster's key insight: **strong conversion only happens when the game itself is "viral-friendly"** — meaning:
1. **Clear hook** — Can someone understand what's happening in 3 seconds?
2. **Instantly readable** — No explanation needed for the visual to be interesting
3. **Satisfying loop** — Something the viewer wants to watch again

Not everything in NavKit passes this filter. The stuff that does:

| Viral-friendly (use for TikTok/Reels) | Not viral-friendly (use for devlogs/YouTube) |
|---|---|
| Water flooding through tunnels | Workshop crafting chains |
| Fire spreading through a forest | Pathfinding algorithm details |
| Snow accumulating → melting → mud | Tool quality system |
| Cross-section colony view | Stockpile filter logic |
| Coggy failing hilariously | Save/load architecture |
| Before/after colony growth | Material system depth |

### The edit matters more than the footage
This was the core message of the Reddit post. They didn't just record and upload — they:
1. Studied a batch of proven-performing game clips
2. Identified patterns (hook placement, cut timing, caption style)
3. Edited their footage to match those patterns

**How to do this yourself:** Spend 30 minutes scrolling TikTok's gaming tags. Save every clip with 100k+ views. Note:
- What happens in the first 2 seconds?
- How many cuts are there?
- Where is the text placed?
- What's the music doing?
- How long is it?

You'll start seeing the same patterns. Then edit your NavKit footage to match.

---

## Technical Specs

| Platform | Aspect ratio | Length sweet spot | Resolution |
|----------|-------------|-------------------|------------|
| TikTok | 9:16 vertical | 21-34 seconds | 1080x1920 |
| Instagram Reels | 9:16 vertical | 15-30 seconds | 1080x1920 |
| YouTube Shorts | 9:16 vertical | 30-60 seconds | 1080x1920 |
| YouTube (devlog) | 16:9 horizontal | 2-8 minutes | 1920x1080 |

- **Don't crosspost with watermarks** — each platform suppresses content watermarked by competitors
- **Upload natively** to each platform separately
- **Captions/subtitles** — add them. Many people scroll with sound off. Auto-caption tools work fine.

---

## NavKit's Strongest Visual Assets (Ranked)

For deciding what to record first:

1. **Cellular automata** — Water, fire, smoke. Inherently mesmerizing, zero explanation needed. This is your bread and butter for short-form.
2. **Cross-section side view** — The "ant farm" colony view is visually distinctive. Most colony sims don't show this. Instant pattern interrupt.
3. **Weather cycle** — Rain → mud → snow → melt. Satisfying transformation loop.
4. **Emergent comedy** — Coggies doing dumb things. Starving near food, walking into fire, pathfinding fails. Relatable + shareable.
5. **Colony timelapse** — Empty world → thriving colony. Before/after hook.
6. **Pathfinding visualization** — 4 algorithms side by side. Dev audience catnip, but niche.

---

## Platform Strategy: Same Game, Different Format

Each platform has a fundamentally different viewing context. The viewer is in a different mode, making a different kind of decision, and your content needs to meet them where they are.

### TikTok / Instagram Reels / YouTube Shorts

**Viewing mode:** Passive. Lean-back. Thumb is scrolling. Content autoplays.
**The decision:** "Do I stop scrolling?" (unconscious, ~0.5s)
**What works:** Pure visual spectacle. No context needed. Interrupt → hook → deliver → loop.
**Hook lives in:** Frame 1 (visual) + text overlay + audio.
**Length:** 15-34 seconds.
**What to post:** Cellular automata (water/fire/smoke), emergent chaos moments, satisfying loops, comedy fails.
**Tone:** Entertainment first. "Look at this cool thing" not "let me explain my game."

These platforms are where **strangers** find you. People who've never heard of colony sims, never played Dwarf Fortress, don't know what a z-level is. The content must work with zero context. If you need to explain it, it's not right for this format.

### Reddit

**Viewing mode:** Intentional. Lean-forward. User is browsing a specific community they chose to join.
**The decision:** "Do I click this post?" (conscious, based on title + thumbnail/preview)
**What works:** The post title IS your hook. Thumbnail matters. Longer content works because the viewer already committed by clicking. Comments drive visibility — a post with 50 engaged comments outranks one with 200 passive upvotes.
**Hook lives in:** The title. Then the first sentence or first 2 seconds of video.
**Length:** GIFs (5-15s) or longer videos (1-3 min) both work. Medium doesn't — too long to be a quick hit, too short to be worth clicking.
**What to post:** Depends entirely on the subreddit.
**Tone:** Matches the community. Technical on r/gamedev, show-don't-tell on r/indiegames, "fellow DF fan" on r/dwarffortress.

Reddit is community-shaped. The same video performs completely differently on r/gamedev vs r/BaseBuildingGames vs r/indiegames. You're not fighting an algorithm — you're earning approval from a specific group of people who care about a specific thing. Tailor the framing (title, description) to each community even if the content is the same clip.

**Reddit-specific formats:**
- **"I added [feature] to my colony sim"** + GIF — works on r/IndieDev, r/indiegames, r/BaseBuildingGames
- **Technical breakdown** + GIF/video — works on r/gamedev, r/proceduralgeneration, r/roguelikedev
- **"DF-inspired colony sim"** + comparison angle — works on r/dwarffortress, r/RimWorld
- **Pure spectacle** + minimal text — works on r/gaming (hard to break into, but huge if it hits)

### YouTube (long-form devlogs)

**Viewing mode:** Intentional. User clicked a thumbnail and committed to watching.
**The decision:** "Is this worth my time?" (conscious, based on thumbnail + title + first 10 seconds)
**What works:** Thumbnail + title sell the click. First 10 seconds sell the watch. Depth and personality keep them. YouTube rewards watch time, so longer content that holds attention beats short content.
**Hook lives in:** Thumbnail (static image, must be readable at small size) + title (curiosity or value proposition). Then the first 10 seconds of video must deliver on the promise.
**Length:** 2-8 minutes for devlogs. Under 2 feels insubstantial. Over 10 needs very strong content to hold.
**What to post:** Feature deep-dives, "how I built X," progress updates, technical breakdowns, colony showcases.
**Tone:** Personal. The viewer chose to be here — they want to know *you* and your process, not just see the game.

YouTube has a **long tail**. A TikTok clip dies in 48 hours. A YouTube devlog titled "How I built water simulation in C" gets found via search for years. This is where you build a lasting library.

### Discord

**Viewing mode:** Social. Conversational. People are hanging out, not consuming content.
**The decision:** Not really a decision — content appears in a feed they're already reading.
**What works:** Casual screenshots, quick GIFs, "look what happened today" moments. Low effort, high frequency. Conversation starters, not polished content.
**What to post:** WIP screenshots, funny bugs, "should I do X or Y?" polls, raw unedited moments.
**Tone:** Like talking to friends. This is where your most engaged fans live.

### One recording session → multiple platforms

Record one interesting moment (30-60 seconds, both vertical and horizontal if possible). Then:

1. **TikTok/Reels/Shorts:** Edit the most intense 15-25 seconds. Text overlay hook. Music. Upload natively to each.
2. **Reddit:** GIF of the highlight (5-15s) with a descriptive title tailored to the subreddit.
3. **Discord:** Raw clip or screenshot, casual caption: "water sim is getting out of hand"
4. **YouTube:** Save multiple clips, compile into a monthly devlog with context and narration.

Same footage, four different products. The editing and framing changes, not the recording.

---

## Where the Audience Actually Is

This is hard to pin down precisely because the audience isn't one group — it's several overlapping groups who care about different aspects of the game. But we can think about it in layers.

### Layer 1: Colony sim core (smallest, highest intent)

**Who:** People who actively play Dwarf Fortress, RimWorld, ONI, Songs of Syx, Going Medieval. They know what z-levels are. They know what a hauling job is. They'll wishlist based on a feature list.
**Where:** r/dwarffortress, r/RimWorld, r/BaseBuildingGames, r/Oxygennotincluded, DF Discord, RimWorld Discord. YouTube channels like Nookrium, Splattercat.
**What reaches them:** "I'm building a DF-inspired colony sim with z-levels, 4 pathfinding algorithms, and water/fire/smoke cellular automata." They hear "DF-inspired" and they're already interested. Show depth, not spectacle.
**Content format:** Reddit posts with technical detail, devlogs, feature comparisons, long-form YouTube.

These people are the easiest to convert but the hardest to scale. There just aren't that many of them. Maybe 50-100k active across all these communities combined. But they talk to each other, they evangelize games they love, and they're the ones who wishlist from a single screenshot.

### Layer 2: Adjacent genre fans (medium size, medium intent)

**Who:** People who play Factorio, Vintage Story, Terraria, Satisfactory, Kenshi, Cataclysm DDA. They don't necessarily play colony sims, but they appreciate deep systems, simulation, crafting, logistics.
**Where:** Those respective subreddits and Discords. r/survivalgames, r/tycoon. YouTube recommendations ("if you liked RimWorld...").
**What reaches them:** Angle it toward what THEY care about. For Factorio fans: transport/logistics. For Vintage Story fans: primitive survival + material depth. For Terraria fans: the cross-section view + digging + building.
**Content format:** Reddit posts with a specific angle for each community. Short-form video can work here if the visual maps to their interests.

These people need a bridge — something that connects NavKit to a game they already love. "What if RimWorld had z-levels" or "colony sim with Factorio-style train logistics." The game is the same, the framing changes.

### Layer 3: General gaming / "that looks cool" (largest, lowest intent)

**Who:** People who don't play colony sims (yet). They're on TikTok, Instagram, YouTube Shorts. They scroll past hundreds of clips a day. They don't know what DF is. They don't care about pathfinding algorithms. But they stop for something that looks satisfying or surprising.
**Where:** TikTok, Instagram Reels, YouTube Shorts, r/gaming, r/oddlysatisfying.
**What reaches them:** Pure visual spectacle. Water flooding a cave. Fire spreading. The ant-farm colony view. No jargon, no feature lists. Just "look at this."
**Content format:** Short-form video, optimized for the interrupt→hook→deliver→loop cycle. This is where the viral format guide applies most directly.

These people are the hardest to convert (most will watch and scroll on) but the volume is enormous. If 0.5% of 300k TikTok viewers wishlist, that's 1,500 wishlists from one clip. The Reddit poster's 8,000 wishlists came from this layer.

### Layer 4: Technical / dev audience (small but amplifying)

**Who:** Developers, programmers, HN readers, people who appreciate "50k lines of C, no engine." They may or may not play the game, but they'll share the post, star the repo, upvote on HN.
**Where:** r/gamedev, r/programming, Hacker News, raylib Discord, r/roguelikedev, r/proceduralgeneration.
**What reaches them:** Technical depth. "How I built water simulation with cellular automata in C." Code snippets, architecture decisions, performance optimization stories.
**Content format:** Long-form Reddit posts, HN Show HN, YouTube deep-dives, blog posts.

These people amplify. A front-page HN post drives thousands of visitors. They might not all wishlist, but the traffic spills into Steam's recommendation algorithm. And some of them ARE gamers who will wishlist AND tell their friends.

### The conversion funnel

```
Layer 3 (TikTok/Reels)     → sees cool clip → maybe follows, maybe wishlists
Layer 2 (adjacent communities) → sees tailored pitch → wishlists if it matches their taste
Layer 1 (colony sim core)   → sees feature depth → wishlists, follows dev, joins Discord
Layer 4 (dev/technical)     → sees technical post → shares widely, amplifies reach
```

You don't choose one layer. You create content that works at each layer, from the same footage, with different framing. The water sim clip becomes a TikTok spectacle (Layer 3), a "look what my DF-inspired sim does" Reddit post (Layer 1), and a "cellular automata water simulation in C" technical post (Layer 4).

### Views ≠ wishlists: the conversion gap

10 million views with zero wishlists is a real outcome. It happens when the clip is entertaining but doesn't signal "this is a game you can play." A water physics clip can look like a tech demo, an art piece, or a screensaver — the viewer enjoys it, scrolls on, and never thinks about playing anything.

**Two things must be true for views to convert:**

1. **The clip signals "this is a game."** Somewhere in the video — a coggy walking through the scene, a cursor placing something, end text that says the name — the viewer needs to understand this is a playable thing, not just a visual. You don't need to be heavy-handed. A single moment of agency (player doing something, a UI element visible, a character reacting) is enough.

2. **There's a path from the video to the wishlist.** TikTok doesn't allow clickable links in captions. The viewer has to: see clip → visit your profile → find link in bio → click through to Steam → wishlist. Each step loses 80-90% of people. Typical conversion is 0.1-1% of views. At 300k views, that's 300-3,000 wishlists — but only if both conditions above are met. At 0% (pure spectacle, no game signal, no link) it's zero.

**Practical steps:**
- Always have your Steam page link in your bio before posting anything
- Include the game name in text overlay (even small, even at the end)
- Show at least one moment of "someone is playing this" — a cursor, a mover, a designation being placed
- Pin a comment with "Wishlist on Steam — link in bio"
- Consider ending clips with a simple card: game name + "Wishlist now"

The Reddit poster's 8,000 wishlists from ~540k combined views is roughly 1.5% conversion — unusually high. They attributed it to the game being "viral-friendly" (instantly readable as a game, not just a visual). The edit + the game readability together drove the conversion. Views without readability is a vanity number.

### Where to start

Layer 1 is the safest first move — they're most likely to care and give useful feedback. Post to r/BaseBuildingGames or r/dwarffortress with a GIF and an honest "building a colony sim" title. See what resonates.

Layer 3 (TikTok) is the highest-upside move but needs more editing effort and more volume (many clips before one hits). Start here once you have a workflow for quickly recording → editing → posting.

Layer 4 (technical) is your wildcard. One well-written HN post can outperform months of slow community building. Save this for when you have a specific technical story worth telling in depth.

---

## Mistakes to Avoid

- **Logos or intros before the interesting part** — You have 1 second. Use it on the hook, not your brand.
- **"Welcome back to my devlog"** — Dead on arrival.
- **Explaining before showing** — Always show first, explain after (or during, via text overlay).
- **Overproduced motion graphics** — Authenticity beats polish for indie devs. Rough is trustworthy.
- **Posting gameplay trailers as short-form** — Trailers are built for a different format. Custom-edit for each platform.
- **Violence/death language in overlays** — Gets silently suppressed. Use soft framing.
- **Reposting TikTok to Reels with watermark** — Both platforms penalize this.

---

## First Video Checklist

For your very first short-form post:

- [ ] Pick your most visual system (water flood recommended — it's the most universally satisfying)
- [ ] Record 30-60 seconds of raw footage at 1080x1920 (vertical)
- [ ] Study 5-10 viral game clips on TikTok. Note their hook/cut/text patterns.
- [ ] Edit to 15-25 seconds: hard cut to action at frame 1, text overlay (≤10 words), fitting music
- [ ] Watch it on mute — does the visual still hook? Watch without video — does the audio still work?
- [ ] Upload natively to TikTok + Instagram Reels
- [ ] Caption: brief, no banned words, include relevant hashtags (#indiedev #gamedev #colonysim)
- [ ] If you have a Steam page: link in bio, mention in comments (not in video)

---

## Sources & Further Reading

- [TikTok Hook Formulas (OpusClip)](https://www.opus.pro/blog/tiktok-hook-formulas) — 5 hook formula breakdowns with retention targets
- [Psychology of Viral Video Openers (Brandefy)](https://brandefy.com/psychology-of-viral-video-openers/) — 7 hook types with psychological mechanics
- [20 Viral Short-Form Video Hooks (Vexub)](https://vexub.com/blog/viral-short-form-video-hooks) — Concrete hook templates
- [TikTok Marketing for Video Games (Cloutboost)](https://www.cloutboost.com/blog/tiktok-marketing-for-video-games-a-complete-guide) — Game-specific TikTok strategies
- [TikTok Algorithm 2026 (Sprout Social)](https://sproutsocial.com/insights/tiktok-algorithm/) — How the algorithm evaluates content
- [How To Market A Game — TikTok guides](https://howtomarketagame.com/2022/02/14/how-to-go-viral-on-tiktok/) — Chris Zukowski's indie game marketing advice
- r/SoloDevelopment "8,000 Wishlists in 3 Days" post by ondrop_games — The Reddit post that prompted this guide
