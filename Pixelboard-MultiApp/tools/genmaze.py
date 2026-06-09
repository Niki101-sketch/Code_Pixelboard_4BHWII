#!/usr/bin/env python3
# Erzeugt + validiert ein ABWECHSLUNGSREICHES, ASYMMETRISCHES Pacman-Labyrinth (32x16)
# - 1 Feld breite Gaenge (Zellen auf ungeraden Koordinaten -> nie 2x2-Flaechen)
# - DFS-Labyrinth, dann "gebraided": alle Sackgassen werden aufgeloest (jede Zelle >=2 Ausgaenge)
# - zusaetzliche Zufalls-Oeffnungen -> mehr Schleifen / mehr Wege
# Aufruf:  python genmaze.py [seed]

import sys, random
from collections import deque

W, H = 32, 16
SEED = int(sys.argv[1]) if len(sys.argv) > 1 else 7
EXTRA_OPEN = 0.14   # Anteil zusaetzlich geoeffneter Innenwaende (mehr Wege)

random.seed(SEED)

# Zellen auf ungeraden Koordinaten: cols 1,3,..,29 (15), rows 1,3,..,13 (7)
CX = 15
CY = 7
def col(cx): return 1 + 2 * cx
def row(cy): return 1 + 2 * cy

PAC_START  = (15, 13)
GHOST_HOME = [(13, 7), (17, 7)]
PP = [(1, 1), (29, 1), (1, 13), (29, 13)]

def build():
    g = [['W'] * W for _ in range(H)]
    # alle Zellen oeffnen
    for cy in range(CY):
        for cx in range(CX):
            g[row(cy)][col(cx)] = '.'

    # --- DFS-Labyrinth (Spannbaum) ---
    visited = [[False] * CX for _ in range(CY)]
    stack = [(0, 0)]
    visited[0][0] = True
    dirs = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    while stack:
        cx, cy = stack[-1]
        nbrs = []
        for dx, dy in dirs:
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < CX and 0 <= ny < CY and not visited[ny][nx]:
                nbrs.append((nx, ny, dx, dy))
        if not nbrs:
            stack.pop(); continue
        nx, ny, dx, dy = random.choice(nbrs)
        # Wand zwischen den Zellen oeffnen
        g[row(cy) + dy][col(cx) + dx] = '.'
        visited[ny][nx] = True
        stack.append((nx, ny))

    def wall_open(cx, cy, dx, dy):
        return g[row(cy) + dy][col(cx) + dx] == '.'

    def degree(cx, cy):
        d = 0
        for dx, dy in dirs:
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < CX and 0 <= ny < CY and wall_open(cx, cy, dx, dy):
                d += 1
        return d

    # --- Braiding: Sackgassen aufloesen (jede Zelle bekommt >=2 Ausgaenge) ---
    changed = True
    while changed:
        changed = False
        for cy in range(CY):
            for cx in range(CX):
                if degree(cx, cy) <= 1:
                    closed = []
                    for dx, dy in dirs:
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < CX and 0 <= ny < CY and not wall_open(cx, cy, dx, dy):
                            closed.append((dx, dy))
                    if closed:
                        dx, dy = random.choice(closed)
                        g[row(cy) + dy][col(cx) + dx] = '.'
                        changed = True

    # --- zusaetzliche Oeffnungen fuer mehr Schleifen/Wege ---
    inner = []
    for cy in range(CY):
        for cx in range(CX):
            for dx, dy in [(1, 0), (0, 1)]:
                nx, ny = cx + dx, cy + dy
                if 0 <= nx < CX and 0 <= ny < CY and not wall_open(cx, cy, dx, dy):
                    inner.append((cx, cy, dx, dy))
    random.shuffle(inner)
    for cx, cy, dx, dy in inner[:int(len(inner) * EXTRA_OPEN)]:
        g[row(cy) + dy][col(cx) + dx] = '.'

    return g

def neighbors_path(g, x, y):
    n = 0
    for dx, dy in [(1,0),(-1,0),(0,1),(0,-1)]:
        nx, ny = x+dx, y+dy
        if 0 <= nx < W and 0 <= ny < H and g[ny][nx] == '.':
            n += 1
    return n

def validate(g):
    errs = []
    if len(g) != H: errs.append(f"Hoehe {len(g)} != {H}")
    for r, rowv in enumerate(g):
        if len(rowv) != W: errs.append(f"Zeile {r} Breite {len(rowv)} != {W}")
    for c in range(W):
        if g[0][c] != 'W' or g[H-1][c] != 'W': errs.append(f"Rand oben/unten Spalte {c}")
    for r in range(H):
        if g[r][0] != 'W' or g[r][W-1] != 'W': errs.append(f"Rand links/rechts Zeile {r}")
    # keine 2x2-Pfadflaeche
    for r in range(H-1):
        for c in range(W-1):
            if all(g[rr][cc] == '.' for rr,cc in [(r,c),(r,c+1),(r+1,c),(r+1,c+1)]):
                errs.append(f"2x2-Plaza bei ({c},{r})")
    # keine Sackgassen: jede Pfadzelle hat >=2 Pfad-Nachbarn
    for r in range(H):
        for c in range(W):
            if g[r][c] == '.' and neighbors_path(g, c, r) < 2:
                errs.append(f"Sackgasse bei ({c},{r})")
    # Verbindung: Flood-Fill von PAC_START
    sx, sy = PAC_START
    if g[sy][sx] != '.': errs.append(f"PAC_START {PAC_START} ist Wand")
    seen = set([(sx, sy)]); q = deque([(sx, sy)])
    while q:
        x, y = q.popleft()
        for dx, dy in [(1,0),(-1,0),(0,1),(0,-1)]:
            nx, ny = x+dx, y+dy
            if 0 <= nx < W and 0 <= ny < H and g[ny][nx] == '.' and (nx,ny) not in seen:
                seen.add((nx,ny)); q.append((nx,ny))
    total = sum(rowv.count('.') for rowv in g)
    if len(seen) != total: errs.append(f"Nicht verbunden: {len(seen)}/{total}")
    for p in PP:
        if g[p[1]][p[0]] != '.': errs.append(f"PowerPellet {p} ist Wand")
    for h in GHOST_HOME:
        if g[h[1]][h[0]] != '.': errs.append(f"GhostHome {h} ist Wand")
    return errs, total

g = build()
errs, total = validate(g)
print(f"=== MAZE (seed={SEED}) ===")
for rowv in g:
    print(''.join(rowv))
print("\n=== C++ ===")
for rowv in g:
    print(f'    "{"".join(rowv)}",')
print(f"\nDots gesamt: {total}")
print(f"PAC_START={PAC_START}  GHOST_HOME={GHOST_HOME}  PP={PP}")
print()
if errs:
    print("!!! FEHLER:");  [print("  -", e) for e in errs[:40]]
else:
    print(">>> VALIDIERUNG OK")
