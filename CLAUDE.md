## Building
For non-trivial change - try to perform a build after the change. 
- If you are integrated with vscode, run the debug build directly.
- If you are copilot CLI - use build-command.ps1 to build

Its ok that the final linking may fail as the game might be running, you can consider that as build succeed.

## Coding
- Refactor as needed, no duplicate code
- I repeat, NO duplicate code by copy pasting. We shall refactor and reuse.
- Drop all backwards compatability, migrate existing code if needed
- Use traditional chinese over simplified chinese in source code

## Config
- Each build has its own asset and config copying, you should not care for example the copied config under android. Focus on the one in the toplevel config/chess_*.yaml.