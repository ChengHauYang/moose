//------------------------------------------------------------
// refined_box.geo  – hex mesh with embedded refined slab
//------------------------------------------------------------
SetFactory("OpenCASCADE");

// ── geometry sizes ─────────────────────────────────────────
xmax  = 2;        // box length  in x
ymax  = 0.25;     // box length  in y
zmax  = 1;        // box length  in z

yCut  = 0.10;     // y-plane  that isolates refined strip
zCut1 = 0.30;     // lower z-plane of the slab
zCut2 = 0.70;     // upper z-plane of the slab

// ── base box ───────────────────────────────────────────────
Box(1) = {0, 0, 0,  xmax, ymax, zmax};

// ── helper boxes that will *fragment* the base volume ——
// (i) split at y = 0.1  →  two y-regions
Box(2) = {0, yCut, 0,  xmax, ymax - yCut,  zmax};

// (ii) refined slab 0.1<y<0.25, 0.3<z<0.7
Box(3) = {0, yCut, zCut1, xmax, ymax - yCut, zCut2 - zCut1};

// ── Boolean fragmentation so the three planes become *true faces*
BooleanFragments{ Volume{1}; Delete; }
                { Volume{2}; Volume{3}; Delete; }

// after fragmentation the model contains several axis-aligned volumes
// use bounding boxes to collect them into named sets
slab[]        = Volume In BoundingBox { 0, yCut-1e-6, zCut1-1e-6,
                                        xmax, ymax+1e-6, zCut2+1e-6 };

lowerYmidZ[]  = Volume In BoundingBox { 0, 0, zCut1-1e-6,
                                        xmax, yCut+1e-6, zCut2+1e-6 };

lowerZ[]      = Volume In BoundingBox { 0, 0, 0,
                                        xmax, ymax+1e-6, zCut1+1e-6 };

upperZ[]      = Volume In BoundingBox { 0, 0, zCut2-1e-6,
                                        xmax, ymax+1e-6, zmax+1e-6 };

// ── transfinite (structured) meshing parameters ───────────
// x – uniform 10 elements  →  dx = 0.2 m
nxTot = 10;

// y: 0–0.1   → 2 elements (0.05 m)
//    0.1–0.25→ 6 elements (0.025 m)
nyCoarse = 2;
nyRef    = 6;

// z: 0–0.3   → 3 elements (0.10 m)
//    0.3–0.7 → 8 elements (0.05 m)
//    0.7–1.0 → 3 elements (0.10 m)
nzLow  = 3;
nzMid  = 8;
nzHigh = 3;

// apply the counts
Transfinite Line "*" = nxTot + 1;                // every x-line

Transfinite Volume{ slab[] }        = {nxTot+1, nyRef   +1, nzMid +1};
Transfinite Volume{ lowerYmidZ[] }  = {nxTot+1, nyCoarse+1, nzMid +1};
Transfinite Volume{ lowerZ[] }      = {nxTot+1, nyCoarse+nyRef+1, nzLow +1};
Transfinite Volume{ upperZ[] }      = {nxTot+1, nyCoarse+nyRef+1, nzHigh+1};

// recombine structured tets → hexes
Recombine Volume "*";

// ── physical tags for MOOSE (or any FEM code) ─────────────
Physical Volume(1) = slab[];                          // refined block
Physical Volume(0) = {lowerZ[], upperZ[], lowerYmidZ[]}; // everything else

// uncomment to see edges & IDs in the GUI
//Mesh.SurfaceEdges = 1;

