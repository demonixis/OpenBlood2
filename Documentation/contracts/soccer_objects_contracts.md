# Original soccer objects

The stock Nightmares soccer maps use a solid gravity-driven `SoccerBall` and
two touch-only `SoccerGoal` world models. The selected source is
`ObjectDLL/SoccerObjects.cpp` and the scoring branch in
`ObjectDLL/PlayerObj.cpp::SoccerGoal`. The models, skins, sounds, goal geometry
and replacement spawners come from the installed archives.

The ball transfers player velocity with the original upward impulse and aim
jitter. A checked link records the last toucher and resets the 60-second
lifetime. On each update, the ball rolls about the displacement crossed with
the last ground normal and applies the original speed thresholds, drag and
slope acceleration. The bounded world sweep supplies contact normals for the
bounce. The ball uses the default LithTech mass of 30 and applies drag in its
own update. The original bounce sound checks the zero-initialized timestamp,
which the source never updates.

Goals require a valid last toucher and, when directional, a positive
velocity dot product against the authored direction. The goal claims the
ball before firing the score target, emits the original five bloody
fragments through the typed effect service, and plays its optional sound.
Soccer rules credit the opposing goal value or apply the optional own-goal
deduction with the source team-size multipliers. Local custom inspection
uses the zombie-head art. Soccer rules use the standard ball by default
and can select the zombie head.

On a loss or timeout, the first live goal schedules the replacement. The
first update waits one frame, then a random live goal sends its authored
score trigger to the map's spawner. A scored ball leaves replacement to its
own goal's trigger. During world clear, all goals are pending before ball
destruction, so no replacement can escape teardown.

`soccer.soc_kickthehead1` and `soccer.soc_kickthehead2` run the authoritative
object mechanics on the two stock maps, with a checked local client assigned
to the soccer scoring rules. ENet host and client admission follows the
existing M9 contract.
