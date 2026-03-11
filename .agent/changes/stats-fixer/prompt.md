tools\apply_balance.py is an old script that fixed balancing "issues" we had in the game and it truned out to be ok. Do note the game has involved quite significantly where the skill levels has changed to direct power values (instead of a 2 step conversion) see migrate_chess_save.py - these are for reference and are no longer applicable.

In anyway - we added a whole slew of characters in chess_combos.yaml and applied some balancing changes here agent\changes\add-more-chess-piece\03-tasks-01-python-db-script.md with new skills too see initial plan D:\projects\kys-cpp\kys-cpp\.agent\changes\add-more-chess-piece\倚天+飞狐+部分补完第二版.md (beaware of simplified and traditional name diff).

There are few issues we need to solve.

1. some skills messed up the simplified vs traditional name and ends up being duplicated and some other added names remained simplified.
2. the eft and music effect does not exist in our resources D:\projects\kys-cpp\kys-cpp\work\game-dev\resource. I am happy to "steal" resource from D:\games\人在江湖cpp版\人在江湖cpp版 - but make sure eft effects are added incrementally so the game can load them correctly. Refer to the CSV saves in 人在江湖 for what effects/music they use for their skills. If you are capable you can unzip the eft and view the images to see if they fit the skill.
3. review the newly added skills in general to see if there are any problems.

On top of those, we need to review are stats balance for newly added characters too. We missed the part on skill damage and weapon mastery and in general do another round of review on basic stats - refer to formulas in game to gauge strength.

Do some analysis on existing database data first in 0.db for just the characters we care about in chess combos and chess pool to gauge where we are currently.

We want character to have a normal skill and a strong skill (ultimate), usually it is around ~400 on normal skill and ~900 on ultimate - please try to balance it out using current character stats and refer to our initial planning (you are allowed to deviate if you see fit). Additionally there are the "fist, sword, knife, unusual" that was also left out. We want the players primary weapon stat (or weapon mastery) (eg. matching the type of skill they use) to start out around 40ish, higher tiered piece are allowed to start a little higher. We need to normalize all 4 of those masteries, not just the one they are good. Use your judgement on the higher tiered characters as variation is welcomed as long as its not too unreasonable.

The balancing is only focused on the in game stats, synergy is out of scope.

Write a new script to apply balancing to our 0.db for new characters added.

For verification - just rerun the analysis script to make sure things are looking ok.