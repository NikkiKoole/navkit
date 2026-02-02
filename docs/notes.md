
ok i think your ocntext is getting full?

ok we need to be precise in the designation names , and we also save the cooldown timers per mover if they help reproducing bugs, and maybe we also need a way to check hpa pathfindng in the inspect ? Oh and i definitely do not want to fall back to astar, we need a test for this case have our hpa pathfinder fail it and then fix it and just use our hpa pathfinder in this case too, so it seems hpa cross z is not completely working like astar?

also please run the tests in isolation (chekc makefile) it take forever like this
