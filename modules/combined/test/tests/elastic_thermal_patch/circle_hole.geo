pitch = 1;
circle_r = 0.38;
bottom_left_circle = pitch / 2;

// Outer square points
Point(1) = {0, 0, 0};
Point(2) = {pitch, 0, 0};
Point(3) = {pitch, pitch, 0};
Point(4) = {0, pitch, 0};

// Outer box lines
Line(1) = {1, 2};
Line(2) = {2, 3};
Line(3) = {3, 4};
Line(4) = {4, 1};
Curve Loop(1) = {1, 2, 3, 4};

// Circle points
Point(5) = {bottom_left_circle, bottom_left_circle, 0.0};
Point(6) = {bottom_left_circle - circle_r * Cos(Pi / 4), bottom_left_circle + circle_r * Sin(Pi / 4), 0.0};
Point(7) = {bottom_left_circle - circle_r * Cos(Pi / 4), bottom_left_circle - circle_r * Sin(Pi / 4), 0.0};
Point(8) = {bottom_left_circle + circle_r * Cos(Pi / 4), bottom_left_circle - circle_r * Sin(Pi / 4), 0.0};
Point(9) = {bottom_left_circle + circle_r * Cos(Pi / 4), bottom_left_circle + circle_r * Sin(Pi / 4), 0.0};

// Arcs for circle
Circle(5) = {6, 5, 7};
Circle(6) = {7, 5, 8};
Circle(7) = {8, 5, 9};
Circle(8) = {9, 5, 6};
Curve Loop(2) = {5, 6, 7, 8};  // Circle loop (positive = counterclockwise)

// The surface is outer loop minus inner loop (circle is a hole)
Plane Surface(1) = {1, 2};

// Optionally, define mesh size or transfinite if needed
// Mesh.ElementOrder = 2; // e.g., second-order elements
