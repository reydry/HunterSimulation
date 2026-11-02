# HunterSimulation

From what I understood, the task was to build a small simulation where bots play hide and seek. The seeker NPC has to move randomly around the game map, while the hiding NPCs have to move to safe zones depending on whether the seeker has noticed their spot. The project was developed on UE 5.8.4, and a demo level is included. All you need to do is generate the .sln from the .uproject. To avoid generating cover points constantly, I made a manager that runs an EQS once at startup, finds points near cover, and works with those. The manager also updates point availability every half second and picks a safe spot only when the enemy has noticed the current spot or is within a radius from which it could potentially see it. I also made a custom StateTree task that tells the character to go to the corresponding point. While solving the problem of navigating along potentially safe routes, I decided that writing a custom route-building system was too large and time-consuming for a test assignment, so I used a ready-made set of safe points and lead the character through them.

Due to lack of time, all the debug functionality was written with the help of AI.

Known issues:

1. The character does not always reach the point (it may stop somewhere nearby).
2. It may not always find the optimal path.
3. Safe zones are not generated near all walls (need to reconfigure EQS)

# DEBUG
ai.debug.routes (0-2)

ai.debug.routes 1
  Green lines: path segments hidden from all hunters.
  Red lines: segments visible to at least one hunter. Above them is the label EXPOSED (seen by X).
  Spheres at segment ends: path nodes.
  Magenta line NO NAV PATH: no navmesh path was found, so a straight line to the target is drawn.
  Summary line at the target: label | EXPOSED/CONCEALED | exposed N/M segs, exposed/total length in uu.

ai.debug.routes 2
  DIRECT: the direct route to the final point. Above the final point is text showing whether it was accepted or rejected (and by whom it was noticed).
  ALT #N: routes through alternative candidate points, numbered in ascending order of detour.
  Thick light-blue sphere and line: the chosen candidate, i.e. the first one in the list with the minimum detour whose route is hidden.
  Orange: the candidate was rejected because its route is visible to hunters.
  Gray: the candidate was not evaluated by the planner because another one had already been accepted earlier. It is shown for reference, to indicate what it would have been.
  FALLBACK: no candidate is hidden, so the pawn goes directly to the final point despite the exposure.

ai.debug.spots (0-2)

ai.debug.spots 1
  Above every point within 1500 uu of any victim, a text label with the reason is displayed.
  Green: the text starts with SAFE, the point is safe.
  Red: the point is unsafe, and the text gives the reason (a hunter is in view or nearby).
  Labels update every frame and live for 0.5 s.

ai.debug.spots 2
  Triggers at the moment a victim chooses a point. Shows up to 6 of the nearest safe points, with the label format Name: #N d=distance ...:
  
  Blue, CHOSEN: the chosen point.
  Orange, skipped: taken by X: the point is already occupied by another victim.
  Light blue, closer, but not picked: closer than the chosen one and not occupied, but was not chosen. Check the verdict for the reason (it is stored in TakenSpotReasons).
  Gray, farther than chosen by N: farther than the chosen one by N uu.
  Red, nearer but UNSAFE - reason: up to 3 unsafe points that are closer than the chosen one. They explain why the victim did not go to a closer hiding spot.
