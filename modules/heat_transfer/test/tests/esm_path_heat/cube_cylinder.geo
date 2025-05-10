SetFactory("OpenCASCADE");

// === Parameters ===
R  = 0.14142135623;
y0 = 0.25;
z0 = 1.5;
Lx = 3;

lc_core = 0.02;
lc_far  = 0.3;
r_transition = 0.5;

// === Semicylinder Base Points ===
Point(1) = {0, 0.3, z0, lc_core};
Point(2) = {0, y0, z0 - R, lc_core};
Point(3) = {0, y0, z0 + R, lc_core};

// === Arc and Line ===
Circle(1) = {2, 1, 3};
Line(2)   = {3, 2};

// === Semicircle Surface ===
Line Loop(10) = {1, 2};
Plane Surface(11) = {10};

// === Extrude the semicylinder ===
out[] = Extrude {Lx, 0, 0} {
  Surface{11};
  Layers{100};
};

// === External Box ===
Box(100) = {0, 0, 0, 3, 0.25, 3};

// === Boolean Fragment ===
frag[] = BooleanFragments{ Volume{100}; Delete; }{ Volume{out[1]}; Delete; };

// === Mesh size field
Field[1] = Distance;
Field[1].NodesList = {1, 2, 3};
Field[2] = Threshold;
Field[2].InField = 1;
Field[2].SizeMin = lc_core;
Field[2].SizeMax = lc_far;
Field[2].DistMin = 0;
Field[2].DistMax = r_transition;
Background Field = 2;

// === Physical groups
Physical Surface("bottom") = {18};    // Manually identified y=0 surface
Physical Volume("fluid")  = {frag[]}; // <-- tag all volumes created by BooleanFragments
