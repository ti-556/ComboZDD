# readme desu

sudo apt update
sudo apt install -y build-essential cmake ninja-build graphviz
sudo apt install graphviz
cmake -S . -B build -G Ninja
cmake --build build

tests: ctest --test-dir build --output-on-failure

ZDD demo: ./build/combo_lens_demo [startermove]

ZDD demo example: ./build/combo_lens_demo 2H

check list of moves with: ./build/combo_lens_demo --help

simulator webUI: google-chrome tools/frame_debugger.html

# ComboZDD v1

ComboZDD v1 is a minimal C++20 prototype for a fighting-game combo analysis engine.

It implements the architecture we discussed:

```text
Frame-level simulator
  realistic-ish movement, hitboxes, hitstun, wall splat, recovery
        ↓
Hybrid event-driven abstraction
  compact canonical SearchState for route search
        ↓
Route search
  valid move sequences generated from SearchNode = SearchState + representative FrameState
        ↓
Reduced ZDD route family
  compressed route sets over position × move variables
        ↓
State transition graph
  SearchState nodes, move-labeled edges, SCC loop checks
```

The ZDD implementation is an internal header-only reduced zero-suppressed
decision diagram. It is intentionally small, but it supports route insertion,
union, intersection, difference, membership checks, set counting, enumeration,
and Graphviz DOT export.

---

## What is implemented

### Frame-level simulator

The frame-level layer tracks:

- player/opponent x/y position
- velocity
- grounded/airborne/knockdown body state
- current move frame
- startup/active/recovery phases
- rectangular hitboxes and hurtboxes
- hitstun
- hitstop
- meter
- damage scaling bucket
- juggle count
- wall-bounce usage
- ground-bounce usage
- simplified wall splat

The simulator is intentionally small and deterministic.

### Hybrid abstraction

The frame state is converted into a compact `SearchState`:

```cpp
struct SearchState {
    SearchSelfState self;
    SearchOppState opp;

    uint8_t selfActionableIn;
    uint8_t oppHitstunRemaining;
    uint8_t cancelWindowRemaining;
    CancelMask availableCancels;

    uint8_t height;
    uint8_t distance;
    uint8_t wallDist;

    uint8_t meter;
    uint8_t scaling;
    uint8_t juggle;

    bool wallBounceUsed;
    bool groundBounceUsed;

    MoveId lastMove;
};
```

This is the state used for route search and state-graph analysis.

It deliberately does **not** store:

- absolute global frame
- exact hitbox rectangles
- exact x/y position
- exact velocity
- animation history
- total damage

Those details remain in `FrameState` only.

### Route search

The search layer expands `SearchNode`:

```cpp
struct SearchNode {
    SearchState abstract;
    FrameState representative;
};
```

For each candidate move:

1. Run cheap abstract requirements.
2. Check starter/cancel/link timing.
3. Prepare the representative frame state.
4. Simulate the move at frame level.
5. If the move hits, abstract the resulting frame state.
6. Continue DFS.
7. Record every non-empty prefix as a valid route by appending `END`.

### ZDD route family

Routes are converted into position-move variables:

```cpp
ZddVar zddVarFor(int position, MoveId move) {
    return position * MAX_MOVE_ID + move;
}
```

Example:

```text
2M > 5H > 2H > jM > 236H > Super1 > END
```

becomes:

```text
X(0, 2M)
X(1, 5H)
X(2, 2H)
X(3, jM)
X(4, 236H)
X(5, Super1)
X(6, END)
```

Those variables are inserted into a reduced ZDD, where each complete route is
stored as one set of variables. The demo exports `combo_lens_routes.zdd.svg`,
which can be opened directly in a browser. It also exports
`combo_lens_routes.zdd.dot`, which can be rendered with Graphviz:

```bash
dot -Tpng combo_lens_routes.zdd.dot -o combo_lens_routes.zdd.png
```

### State transition graph

The graph layer builds reachable abstract transitions from an initial
`SearchNode`. Nodes are canonical `SearchState` values, and edges are verified
frame-simulated moves. Tarjan SCC analysis reports cyclic components as
degenerate loops, which helps catch move-data or abstraction changes that could
create unbounded route expansion.

The demo exports `combo_lens_state_graph.svg`, which can be opened directly in a
browser. It also exports `combo_lens_state_graph.dot`, which can be rendered
with Graphviz:

```bash
dot -Tpng combo_lens_state_graph.dot -o combo_lens_state_graph.png
```

---

## Example move set

The demo includes 7 moves:

```text
2M
5H
2H
jM
jH
236H
Super1
```

The intended main route is:

```text
2M > 5H > 2H > jM > 236H > Super1 > END
```

There is also a shorter `jH` branch.

---

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Run the demo:

```bash
./build/combo_lens_demo
```

Choose a different forced starter:

```bash
./build/combo_lens_demo 5H
./build/combo_lens_demo --starter 2H
```

Open the frame debugger GUI:

```bash
google-chrome tools/frame_debugger.html
```

The debugger is a static browser GUI for stepping and playing the example frame
simulator data. It draws hurtboxes, active hitboxes, velocity, hitstop, frame
phase, hitstun, meter, scaling, juggle, wall splat, and a recent-frame timeline.
Movement uses `A/D`, jump is `W`, crouch/input down is `S`, and attacks use
`U`/`I`. The debugger parses commands such as `2M`, `5H`, `2H`, airborne
`jM`/`jH`, and `236H`; Super is also available as a single button or `O`/`P`.
Forward/back plus `I` falls back to `5H`, and down diagonals count as crouch
inputs so wall pressure does not require releasing directions perfectly.
It also shows a compact command list and a live combo counter with current hits,
combo damage, and max hits. A 10-frame input buffer stores commands entered
during recovery or hitstop and runs them on the first legal actionable frame.
The debugger enforces the example move requirements used by route search,
including self/opponent grounded or airborne state and the juggle cap.

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

or directly:

```bash
./build/combo_lens_tests
```

---

## Expected demo output

The exact route count can change if you edit move data, but the included version should produce something like:

```text
ComboLens v1 demo
=================
Moves loaded: 7
Routes found: 7
Stored routes: 7
Best damage: 239
ZDD sets: 7, reachable nodes: 16, manager nodes: 74
State graph: 8 nodes, 7 edges, 0 degenerate loop(s)
Visualization exports: combo_lens_routes.zdd.dot, combo_lens_routes.zdd.svg, combo_lens_state_graph.dot, combo_lens_state_graph.svg
Best route: 2M > 5H > 2H > jM > 236H > Super1 > END
Final abstract state: opp=Knockdown, height=0, distance=4, wallDist=1, meterBucket=0, scalingBucket=8, juggle=8
ZDD variables: 2 67 132 197 263 328 385
```

---

## Important limitations

This is a v1 research prototype. It intentionally simplifies many fighting-game systems.

Not implemented yet:

- real input parser
- blocking
- throws
- projectiles
- multi-hit moves
- counter-hit
- air tech
- DI/SDI
- pushblock
- assists
- real animation assets

Also, the current `SearchState -> FrameState` representative strategy stores one representative frame state per search node. In the future, multiple frame representatives may be needed for each abstract state, because different exact positions can bucket to the same abstract state but produce different hitbox outcomes.

---

## Recommended next steps

1. Add JSON move loading.
2. Add route diffing before/after move-data edits.
3. Add filters and projections over the ZDD route family.
4. Extend the browser frame debugger into a route debugger or replace it with a
   native Dear ImGui tool if the project adopts a GUI dependency.
5. Add multiple representatives per abstract state.
6. Swap the internal ZDD manager for CUDD/Sylvan if route counts outgrow the
   prototype implementation.
