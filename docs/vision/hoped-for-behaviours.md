# Behaviours we hope to see

> Status: idea list (started 2026-11-26, user). Not a to-do list: things we'd love to *see*
> happen, preferably as consequences of systems that exist (or will), not as scripts written
> for them. Each entry: what we'd see, which systems it should fall out of, and what is still
> missing. When one happens in a scenario by itself, note where and keep it.

## Showing an unwelcome guest out (user, 2026-11-26)

**What we'd see:** a guest who is no longer welcome (a quarrel during the visit, or overstaying)
is walked to the door by the host: the host comes up to them, they go towards the front door
together, the guest steps out, the door shuts behind them, maybe a little harder than usual.

**Should fall out of:**
- relations (`relations.c`): liking drops during the visit (a quarrel, a disliked trait, the
  host's patience running out);
- ownership and access (`places.c`): the guest's grant is what makes it welcome; taking the grant
  back is "you're no longer welcome here", and a mover somewhere it may not be walks out (the
  guard in `ownership-and-access.md`);
- visits (`visit.c`): host and guest are already one activity with a leave phase; the host walks
  to the door on leaving and closes it (Tidy hosts always);
- noticing and following (`notice.c`, follow the leader in `chase.c`): walking someone somewhere
  is close to a follow with the host in front.

**Missing:** a reason for the host to take the grant back (liking below a threshold during the
visit, or a stay far past what the host wanted: a per-host "how long is welcome"); the host
leading rather than just walking to the door; a mood or relation mark on both afterwards (the
guest remembers; maybe doesn't come back for a while).

## A door left open lets the cold in (Claude, 2026-11-26)

**What we'd see:** in a cold snap a room gets chilly because someone left the front door open
behind them; a Tidy resident gets up and closes it, others shiver.

**Should fall out of:** doors stay open (`CELL_DOOR_OPEN` lets heat through), temperature,
body temperature, the open/close door chore, traits.

**Missing:** someone deciding by themselves to close a door (today only a mark does it): a
"close the door, it's cold" need-like choice for idle residents near an open door that lets
cold or smoke into their own home.

## Nobody answers, so they wait on the gallery (Claude, 2026-11-26)

**What we'd see:** a friend comes over, nobody is home, so they lean on the railing and wait a
bit, talk to a neighbour passing by, and go home.

**Should fall out of:** visits giving up after `visitWaitHours`, noticing and talks (a passer-by
on the gallery), the galerij's shared gallery as a public place.

**Missing:** waiting is standing still today; a waiting visitor counting as free to talk (it is
in the visit activity, which blocks talks).
