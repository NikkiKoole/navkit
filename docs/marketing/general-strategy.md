# Marketing Strategy

> For a solo indie dev building a colony sim. Sustainable, not exhausting.

---

## Core Principles

### 1. Show the work, not the product

You're not selling a finished game — you're inviting people into a journey. Devlogs, process GIFs, "look what happened today" posts. The colony sim audience specifically loves watching games get built. DF has been "in development" for 20 years and people followed every update.

### 2. One piece of content, repurposed everywhere

Don't try to be active on 10 platforms. Make one thing — say a 15-second GIF of water flooding a cave — and post it to Twitter, Reddit, Discord, TikTok. Same content, different captions. This is sustainable for a solo dev, "create unique content for every platform" is not.

### 3. Frequency beats polish

A rough screenshot every week beats a polished trailer every 6 months. People follow consistency. Screenshot Saturday on Twitter is perfect for this — low effort, high visibility, builds habit.

### 4. Let systems sell the game

The biggest marketing asset is that the simulation does interesting things on its own. Fire spreads, water floods, movers make dumb decisions, things go wrong in funny ways. Record those moments. "I didn't plan this, it just happened" is the most compelling content for sim games.

### 5. Community before launch

A Discord server where 50 people care about your game is worth more than 5000 Twitter followers. Start small, let people give feedback, make them feel like they shaped the game. Those early fans become evangelists.

### 6. The Steam page is your funnel

Everything points to the Steam page. Every post ends with "wishlist on Steam." That's the only metric that matters pre-launch.

### 7. Don't market — document

Reframe it mentally. You're not "doing marketing," you're documenting what you're building. That's more authentic, more sustainable, and more interesting to watch. The marketing is a side effect of sharing your work.

---

## Weekly Routine (Sustainable)

This should take ~1 hour per week, not more.

| Day | Action | Time |
|-----|--------|------|
| Any day | Capture a GIF/screenshot of whatever you worked on this week | 5 min |
| Saturday | Post to Twitter (#screenshotsaturday #indiedev #gamedev) | 10 min |
| Saturday | Cross-post to 1-2 relevant subreddits | 10 min |
| Saturday | Share in Discord servers you're already in | 5 min |
| Monthly | Write a short devlog (what changed, what's next, one fun anecdote) | 30 min |

That's it. Don't let marketing eat into dev time.

---

## Content Ideas (Ranked by Effort)

### Low effort, high impact
- GIF of a simulation emergent moment (fire spreading, water flooding, coggy doing something dumb)
- Before/after screenshots ("colony at hour 1 vs hour 10")
- Side-by-side pathfinding algorithm comparison
- Screenshot of a bug that looks funny

### Medium effort
- Short devlog post ("I added weather this week, here's what rain does to mud")
- Time-lapse of a colony growing
- Technical breakdown post for r/gamedev or HN ("how I built water simulation in C")

### Higher effort (monthly/quarterly)
- YouTube devlog (2-5 min, voice or text overlay on gameplay footage)
- Trailer update for Steam page
- Blog post deep-dive on a system

---

## Milestones That Unlock Marketing

| Milestone | What it unlocks |
|-----------|----------------|
| Steam page live | Can funnel all content to wishlists |
| Discord server created | Have a place to send interested people |
| First devlog posted | Establishes the "building in public" pattern |
| 100 wishlists | Steam starts showing your game in recommendations |
| Playable demo | Can participate in Steam Next Fest |
| 500 wishlists | Media/YouTubers start taking you seriously |

---

## Source Code Visibility

The GitHub repo is currently public. Before selling the game commercially:

**Go private before the Steam page goes up.** Reasons:
- Selling a game while the full source is freely available undermines the value proposition. People expect to pay for something they can't just clone.
- A public repo makes it trivial to build pirate copies or clones.
- Doesn't stop you from sharing code snippets in devlogs or technical posts — you just control what's visible.
- You can still open-source specific components later if you want (pathfinding library, etc.).

**Before making it private:**
- Make sure all your local clones are up to date
- Download any issues/PRs you want to keep (there probably aren't any from external contributors)
- Consider archiving interesting commit history you might want to reference

**What to put in its place:**
- A public repo with just a README: game name, description, link to Steam page, link to Discord
- Or nothing — not every game needs a public repo

**Alternative: keep a public "engine" repo, private "game" repo.** If the decoupled-simulation architecture happens, the simulation/pathfinding layer could stay open while the game-specific code (content, assets, scenarios) is private. This gives you technical credibility (devs can see your pathfinding) without giving away the product. But this is more work and not necessary right now.

---

## Tools

The bottleneck isn't posting efficiency, it's having something to post. Keep tools minimal.

### Worth using now
- **Buffer** (free tier) — Write one post, schedule it to Twitter + BlueSky + Mastodon simultaneously. The "one piece of content everywhere" principle automated.
- **Kap** (free, Mac) — GIF recording. Capture 10-15 second clips of gameplay moments. Lightweight, no fuss.
- **ShareX** (free, Windows) or **OBS** (free, cross-platform) — Screen capture alternatives, OBS also does video for YouTube devlogs.
- **Steam's built-in tools** — Visibility rounds, event announcements, update posts. Free, reaches existing wishlisters.

### Maybe later (once there's an audience)
- **Mailchimp** (free tier) — Devlog newsletter. Worth it after 100+ interested people.
- **Linktr.ee or carrd.co** — One URL that points to Steam, Discord, Twitter. Put in every bio.

### Not worth it
- **Building your own tool** — You'll spend weeks on it and post twice. The developer brain says "I could automate this" but the real problem is never the posting pipeline.
- **Expensive social media suites** — You're one person, not a marketing team.
- **Analytics dashboards** — Not enough data to analyze yet. Steam's built-in analytics are sufficient.
- **AI posting / auto-generators** — Community audiences smell automated content instantly and hate it. Authenticity is your advantage over studios.

---

## Step-by-Step Plan

Roughly in order. Don't do the next step until the previous one is done (or at least started). The whole pre-launch setup (steps 1-5) can be done in a single weekend if you push through it.

### Phase 0: Foundation (do this week)

**Step 1: Make the GitHub repo private**
- Confirm all local clones are up to date
- Flip the repo to private on GitHub
- Optionally create a new public repo with just a README + links

**Step 2: Pick a working title**
- "Workers Need Burgers" is the current favorite. Use it. You can change it later.

**Step 3: Register on Steamworks + create the Coming Soon page**
- Pay the $100 fee
- Fill in the minimum: name, short description, 5 dev screenshots, a simple capsule image
- Submit for review (~3-5 business days)
- This is the single highest-leverage thing you can do

**Step 4: Create a Discord server**
- Doesn't need to be fancy. A few channels: #announcements, #general, #screenshots, #suggestions
- This is where your early community lives

**Step 5: Set up social accounts**
- Twitter/X with game name, link to Steam page in bio
- Optionally BlueSky / Mastodon
- Install Buffer, connect accounts

### Phase 1: Build the Habit (weeks 1-4)

**Step 6: First Screenshot Saturday post**
- Pick the most visually interesting thing in the game right now
- Post to Twitter with #screenshotsaturday #indiedev #gamedev
- Cross-post to r/IndieDev or r/IndieDev
- Link Steam page in every post
- Do this every Saturday. Consistency matters more than quality.

**Step 7: Join 2-3 Discord communities**
- DF, RimWorld, raylib, Indie Game Developers — wherever feels natural
- Don't spam your game. Participate genuinely, share when relevant.

**Step 8: First devlog post**
- Short (300-500 words): what the game is, what you're working on, one interesting technical detail
- Post to Reddit (r/gamedev or r/indiegames), cross-post link to Twitter/Discord
- Establishes the "building in public" pattern

### Phase 2: Grow (months 1-3)

**Step 9: Weekly Screenshot Saturday** (keep going, don't stop)

**Step 10: Monthly devlog**
- What changed, what's next, one fun story or emergent moment
- Alternate between Reddit communities (r/BaseBuildingGames one month, r/dwarffortress the next)

**Step 11: One technical deep-dive**
- Pick your best system (pathfinding, water sim, fire spread)
- Write it up for r/gamedev or Hacker News (Show HN)
- This is where "50k lines of C, no engine" gets attention from the dev crowd
- Can drive a big spike in wishlists if it hits front page

**Step 12: First YouTube devlog** (optional but high value)
- 2-5 min, gameplay footage with text overlay or voiceover
- Doesn't need to be polished — rough is authentic
- YouTube content has a long tail (people find it months later via search)

### Phase 3: Milestone Targets (months 3-6)

**Step 13: Hit 100 wishlists**
- Steam starts recommending your game to similar audiences
- If you've been doing weekly posts, this should happen naturally

**Step 14: Build a playable demo**
- Scope it tight: one scenario, 15-30 minutes of gameplay
- This unlocks Steam Next Fest (huge visibility event, free)

**Step 15: Apply for Steam Next Fest**
- These run a few times per year
- Massive wishlist boost if your demo is solid
- Plan the demo build around the next available fest date

**Step 16: Reach out to YouTubers**
- Nookrium, Splattercat, GamingByGaslight — they cover indie colony sims
- Send a short email: what the game is, a Steam link, a GIF, offer a key
- Don't bother until you have 500+ wishlists and a playable demo — they get hundreds of pitches

### What NOT to do

- Don't spend more than 1 hour per week on marketing
- Don't build marketing tools
- Don't wait until the game is "ready" to start posting
- Don't post the same thing to the same community more than once a month
- Don't compare your numbers to studios with marketing budgets
- Don't stop developing to "focus on marketing" — the development IS the content
