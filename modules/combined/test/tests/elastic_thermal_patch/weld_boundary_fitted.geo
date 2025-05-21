//----------------------------------------------
// Parameters
//----------------------------------------------
pitch = 1;
circle_r = 0.38;
r0 = 0.2;
r1 = circle_r;
dr = 0.03;
n_radial = Round((r1 - r0) / dr);
n_circle = 128;
center[] = {0.5, 0.5, 0};
lc_outer = 0.015;

//----------------------------------------------
// Outer rectangle (box domain)
//----------------------------------------------
Point(1) = {0, 0, 0, lc_outer};
Point(2) = {pitch, 0, 0, lc_outer};
Point(3) = {pitch, pitch, 0, lc_outer};
Point(4) = {0, pitch, 0, lc_outer};

Line(1) = {1, 2};
Line(2) = {2, 3};
Line(3) = {3, 4};
Line(4) = {4, 1};
Curve Loop(100) = {1, 2, 3, 4};  // Outer rectangle loop

Physical Curve("left") = {4};
Physical Curve("right") = {2};
Physical Curve("top") = {3};
Physical Curve("bottom") = {1};

//----------------------------------------------
// Structured annulus: points
//----------------------------------------------
For i In {0:n_radial}
  r = r0 + i * dr;
  For j In {0:n_circle-1}
    theta = 2*Pi*j/n_circle;
    x = center[0] + r * Cos(theta);
    y = center[1] + r * Sin(theta);
    p_id = 10000 + i * n_circle + j;
    Point(p_id) = {x, y, 0, dr};
  EndFor
EndFor

//----------------------------------------------
// Structured annulus: quad mesh
//----------------------------------------------
structured_outer_lines[] = {};
s_id = 1000;
annulus_surfaces[] = {};
For i In {0:n_radial-1}
  For j In {0:n_circle-1}
    p1 = 10000 + i * n_circle + j;
    p2 = 10000 + i * n_circle + (j+1)%n_circle;
    p3 = 10000 + (i+1) * n_circle + (j+1)%n_circle;
    p4 = 10000 + (i+1) * n_circle + j;

    l1 = newl; Line(l1) = {p1, p2};
    l2 = newl; Line(l2) = {p2, p3};
    l3 = newl; Line(l3) = {p3, p4};
    l4 = newl; Line(l4) = {p4, p1};

    If (i == n_radial - 1)
      structured_outer_lines[j] = l3;
    EndIf

    ll_id = news;
    Line Loop(ll_id) = {l1, l2, l3, l4};
    srf_id = news;
    Plane Surface(srf_id) = {ll_id};

    Transfinite Line{l1, l3} = 2 Using Progression 1;
    Transfinite Line{l2, l4} = 2 Using Progression 1;
    Transfinite Surface{srf_id} = {p1, p2, p3, p4};
    Recombine Surface{srf_id};

    annulus_surfaces[] += {srf_id};
    s_id += 1;
  EndFor
EndFor

//----------------------------------------------
// Outermost circle as hole loop
//----------------------------------------------
For j In {0:n_circle-1}
  outer_loop_lines[j] = -structured_outer_lines[j];
EndFor
Curve Loop(200) = {outer_loop_lines[]};

//----------------------------------------------
// Final outer region (rectangle minus annulus)
//----------------------------------------------
Plane Surface(9999) = {100, 200};
Recombine Surface{9999};

//----------------------------------------------
// Physical surfaces to ensure mesh is exported
//----------------------------------------------
Physical Surface("structured_annulus") = {annulus_surfaces[]};
Physical Surface("outer_domain") = {9999};

//----------------------------------------------
// Mesh size smoothing: Field[]
//----------------------------------------------
Field[1] = Distance;
nodes[] = {};
For j In {0:n_circle-1}
  nodes[j] = 10000 + n_radial * n_circle + j;
EndFor
Field[1].NodesList = {nodes[]};

Field[2] = Threshold;
Field[2].InField = 1;
Field[2].SizeMin = dr;
Field[2].SizeMax = lc_outer;
Field[2].DistMin = 0.01;
Field[2].DistMax = 0.2;

Background Field = 2;

//----------------------------------------------
// Done. Ready for mesh generation
//----------------------------------------------
Mesh 2;
