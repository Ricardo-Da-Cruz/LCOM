# LCOM Project — 2D Pac-Man for MINIX

Overview

LCOM is a 2D interactive Pac‑Man-style game implemented in C for the MINIX operating system. The project demonstrates low-level systems programming concepts such as interrupt handling, device drivers, frame-timed game loops, event-driven input, sprite rendering, and simple AI.

Key features

- Player-controlled Pac‑Man collecting pellets and power pellets
- Four ghosts (Blinky, Pinky, Inky, Clyde) with distinct AI behaviors (chase / scatter / frightened)
- Power pellet mechanic granting temporary invincibility and the ability to eat ghosts for bonus points
- Lives system, scoring, and persistent leaderboard (top scores saved to file)
- Double-buffered VBE rendering (mode 0x115) with XPM sprites for smooth animation
- Maze edge-wrapping, collision detection, and efficient pathfinding

Architecture

The codebase uses a layered and modular design:

- src/main.c — main controller / state machine
- src/game.c, src/game.h — game engine: movement, collisions, pathfinding, scoring, rendering hooks
- src/menu.c, src/menu.h — menu system and hierarchical navigation (dynamic scoreboards)
- drivers/ — device drivers and hardware abstractions (timer, keyboard, GPU, interrupts)
- assets/ — XPM sprites, fonts, and maze definitions

Devices and timing

- Timer: drives the game loop at ~60 Hz for consistent animation and gameplay timing.
- Keyboard: interrupt-driven scancode handling (WASD / arrow keys, Enter, Esc).
- GPU: VBE graphics via mode 0x115; double buffering and XPM sprite rendering.
- Interrupt controller: unified handling of timer and keyboard interrupts for responsive input.

Ghost AI and gameplay notes

- Ghosts follow classic Pac‑Man-inspired behaviors: Blinky targets Pac‑Man, Pinky anticipates, Inky uses a vector-based tactic, and Clyde alternates between chase and home behavior.
- Pathfinding favors efficiency (squared Euclidean checks) with wall-collision awareness and direction constraints (no immediate 180° reversals except when frightened).

Controls

- Move: Arrow keys or WASD
- Select / Confirm: Enter
- Back / Exit: Esc

Building and running

This project targets MINIX. Use the repository's Makefile or your MINIX toolchain to build. Run inside a MINIX instance or compatible emulator that supports VBE graphics. Exact build/run commands depend on your MINIX setup.

Project structure (high level)

- src/ — main source files
- drivers/ — low-level device code
- assets/ — sprites and fonts
- docs/ — design notes and extra documentation (if present)

Contributing

Contributions, bug reports, and improvements are welcome. Please open issues and pull requests in the repository.

Group: GRUPO_2LEIC15_2

Group members:

1. Ana Silva (up202004380@edu.fc.up.pt)
2. Artur Santos (up201900839@edu.fe.up.pt)
3. Divaldo Dias (up202309923@edu.fe.up.pt)
4. Oleksandr Aleshchenko (up202210478@edu.fe.up.pt)
5. Ricardo Cruz (up202008789@edu.fc.up.pt)
