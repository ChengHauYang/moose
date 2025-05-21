// === Geometry Parameters ===
R  = 0.15;   // Radius
y0 = 0.3;    // Center y
z0 = 1.5;    // Center z
Lx = 5;      // Extrusion length in x-direction

// Mesh resolution
ny = 20;     // Number of divisions along the arc (y-direction)
nz = 40;     // Number of divisions along base (z-direction)
nx = 30;     // Number of extrusion layers

// === Points ===
Point(1) = {0, y0 + R, z0, 0.005};   // Bottom of semicircle
Point(2) = {0, y0, z0 - R, 0.005};   // Left
Point(3) = {0, y0, z0 + R, 0.005};   // Right

// === Arcs and Lines ===
Circle(1) = {2, 1, 3};   // Arc from left to right (lower half-circle)
Line(2)   = {3, 2};      // Straight line base

// === Line Loop and Surface ===
Line Loop(10)     = {1, 2};
Plane Surface(11) = {10};

// === Transfinite Setup (Structured Mesh) ===
Transfinite Line{1} = ny Using Progression 1;
Transfinite Line{2} = nz Using Progression 1;
Transfinite Surface{11};
Recombine Surface{11};  // Convert triangles to quads

// === Extrude Surface into Volume ===
out[] = Extrude {Lx, 0, 0} {
  Surface{11};
  Layers{nx};
  Recombine;
};
