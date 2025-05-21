SetFactory("OpenCASCADE");

// ------------------------------------------------------------
// ❶ Parameters
// ------------------------------------------------------------
R  = 0.14142135623;   // semicylinder radius
y0 = 0.25;            // center-y of semicylinder
z0 = 1.5;             // center-z of semicylinder
Lx = 3;               // extrusion length in x
lc_core = 0.03;       // mesh size near the wall
lc_far  = 0.3;        // mesh size in the far field
r_transition = 0.5;   // transition distance (core → far)

// ------------------------------------------------------------
// ❷ Outer rectangle in the y-z plane
// ------------------------------------------------------------
Point(101) = {0, 0    , 0, lc_far};
Point(102) = {0, 0.25 , 0, lc_far};
Point(103) = {0, 0.25 , 3, lc_far};
Point(104) = {0, 0    , 3, lc_far};

Line(101) = {101,102};
Line(102) = {102,103};
Line(103) = {103,104};
Line(104) = {104,101};

Line Loop(110)     = {101,102,103,104};
Plane Surface(120) = {110};          // outer rectangular cross-section

// ------------------------------------------------------------
// ❸ Semicylinder cross-section
// ------------------------------------------------------------
Point(1) = {0, 0.3, z0     , lc_core};
Point(2) = {0, y0 , z0 - R , lc_core};
Point(3) = {0, y0 , z0 + R , lc_core};

Circle(1) = {2,1,3};
Line  (2) = {3,2};

Line Loop(10)   = {1,2};
Plane Surface(11) = {10};            // semicircular face

// ------------------------------------------------------------
// ❹ BooleanFragments: split into fluid and solid faces
// ------------------------------------------------------------
frags[] = BooleanFragments{ Surface{120}; Delete; }
                         { Surface{11};  Delete;  };

// frags[0] = outer rectangle fragment (fluid)
// frags[1] = semicylinder fragment   (solid)

// ------------------------------------------------------------
// ❺ 2-D mesh algorithm
// ------------------------------------------------------------
Mesh.Algorithm = 2;   // Delaunay triangles

// ------------------------------------------------------------
// ❻ Extrude faces → 3-D volumes (shared interface, prisms)
// ------------------------------------------------------------
vol_fluid[] = Extrude{ Lx,0,0 }{
  Surface{ frags[0] };
  Layers{50};
  Recombine;          // tri × extrude → prism
};

vol_solid[] = Extrude{ Lx,0,0 }{
  Surface{ frags[1] };
  Layers{50};
  Recombine;
};

// ------------------------------------------------------------
// ❼ Size field: fine near wall, coarse far away
// ------------------------------------------------------------
Field[1] = Distance;
Field[1].NodesList = {1,2,3};

Field[2] = Threshold;
Field[2].InField  = 1;
Field[2].SizeMin  = lc_core;
Field[2].SizeMax  = lc_far;
Field[2].DistMin  = 0;
Field[2].DistMax  = r_transition;
Background Field  = 2;

// ------------------------------------------------------------
// ❽ Physical groups
// ------------------------------------------------------------
Physical Volume("1") = { vol_fluid[], vol_solid[] };
//Physical Volume("solid") = {};

// single required surface: bottom face (y = 0)
Physical Surface("bottom") = {
  Surface In BoundingBox{ -1e-6, 0-1e-6, -1e-6,  Lx+1e-6, 0+1e-6, 3+1e-6 }
};
