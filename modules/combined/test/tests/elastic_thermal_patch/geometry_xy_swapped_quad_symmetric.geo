
// Auto-generated symmetric shape
lc = 1e-2;
Point(1) = {0.0, 0.0, 0, lc};
Point(2) = {0.035, 0.0, 0, lc};
Point(3) = {0.0349285714285714, 0.0842687074829932, 0, lc};
Point(4) = {0.0349264950166113, 0.08681972789115652, 0, lc};
Point(5) = {0.0321583610188261, 0.08752342595083111, 0, lc};
Point(6) = {0.0300418050941306, 0.08806148382613363, 0, lc};
Point(7) = {0.0269479512735326, 0.08884798438290349, 0, lc};
Point(8) = {0.0245056755260243, 0.08946884475530128, 0, lc};
Point(9) = {0.0214120985603543, 0.09025527493170812, 0, lc};
Point(10) = {0.0184811738648947, 0.09100035664483959, 0, lc};
Point(11) = {0.0148992248062015, 0.09191093778161707, 0, lc};
Point(12) = {0.0118056478405315, 0.09269736795802391, 0, lc};
Point(13) = {0.0080607696566998, 0.0936493679384398, 0, lc};
Point(14) = {0.0041526854928017, 0.0946428571428571, 0, lc};
Point(15) = {0.0021965669988925, 0.0978741496598639, 0, lc};
Point(16) = {-0.0008964562569213, 0.0978741496598639, 0, lc};
Point(17) = {-0.0020372369878183, 0.0994047619047619, 0, lc};
Point(18) = {-0.0020372369878183, 0.10059523809523811, 0, lc};
Point(19) = {-0.0008964562569213, 0.1021258503401361, 0, lc};
Point(20) = {0.0021965669988925, 0.1021258503401361, 0, lc};
Point(21) = {0.0041526854928017, 0.10535714285714291, 0, lc};
Point(22) = {0.0080607696566998, 0.1063506320615602, 0, lc};
Point(23) = {0.0118056478405315, 0.1073026320419761, 0, lc};
Point(24) = {0.0148992248062015, 0.10808906121838293, 0, lc};
Point(25) = {0.0184811738648947, 0.10900064235516042, 0, lc};
Point(26) = {0.0214120985603543, 0.10974572406829189, 0, lc};
Point(27) = {0.0245056755260243, 0.11053215424469873, 0, lc};
Point(28) = {0.0269479512735326, 0.11115301461709653, 0, lc};
Point(29) = {0.0300418050941306, 0.11193951517386639, 0, lc};
Point(30) = {0.0321583610188261, 0.11247757304916891, 0, lc};
Point(31) = {0.0349264950166113, 0.11318127110884348, 0, lc};
Point(33) = {0.035, 0.2, 0, lc};
Point(34) = {0.0, 0.2, 0, lc};
Point(35) = {0.0, 0.0, 0, lc};
Point(36) = {0.0, 0.0978741496598639, 0, lc};
Point(37) = {0.0, 0.1021258503401361, 0, lc};
Point(38) = {0.0, 0.0865136054421769, 0, lc};
Point(39) = {0.035, 0.0785136054421769, 0, lc};
Point(40) = {0.0349264950166113, 0.1000000000000000, 0, lc};
Point(41) = {0.0321583610188261, 0.1000000000000000, 0, lc};
Point(42) = {0.0300418050941306, 0.1000000000000000, 0, lc};
Point(43) = {0.0269479512735326, 0.1000000000000000, 0, lc};
Point(44) = {0.0245056755260243, 0.1000000000000000, 0, lc};
Point(45) = {0.0214120985603543, 0.1000000000000000, 0, lc};
Point(46) = {0.0184811738648947, 0.1000000000000000, 0, lc};
Point(47) = {0.0148992248062015, 0.1000000000000000, 0, lc};
Point(48) = {0.0118056478405315, 0.1000000000000000, 0, lc};
Point(49) = {0.0080607696566998, 0.1000000000000000, 0, lc};
Point(50) = {0.0041526854928017, 0.1000000000000000, 0, lc};
Point(51) = {0.0, 0.0956985273970042, 0, lc};
Point(2000) = {0.037345078815609806, 0.0866023628386266, 0, lc};
Point(2001) = {0.037345078815609806, 0.1000000000000000, 0, lc};
Point(2002) = {0.037345078815609806, 0.1133976371521769, 0, lc};

offset = 36;
start = 52;
end = 62;

For i In {start:end - 1}
  Line Loop(i) = {i, offset + i - 1, offset + i, i + 1, i};
EndFor


Line(1) = {1, 2};
Line(2) = {2, 39};
Line(3) = {3, 4};
Line(4) = {4, 5};
Line(5) = {5, 6};
Line(6) = {6, 7};
Line(7) = {7, 8};
Line(8) = {8, 9};
Line(9) = {9, 10};
Line(10) = {10, 11};
Line(11) = {11, 12};
Line(12) = {12, 13};
Line(13) = {13, 14};
Line(14) = {14, 15};
Line(35) = {15, 36};
Line(15) = {36, 16};
Line(16) = {16, 17};
//Line(17) = {17, 18};
//Line(18) = {18, 19};
//Line(19) = {19, 37};
//Line(20) = {20, 21};
//Line(21) = {21, 22};
//Line(22) = {22, 23};
//Line(23) = {23, 24};
//Line(24) = {24, 25};
//Line(25) = {25, 26};
//Line(26) = {26, 27};
//Line(27) = {27, 28};
//Line(28) = {28, 29};
//Line(29) = {29, 30};
//Line(30) = {30, 31};
//Line(37) = {31, 4};
//Line(38) = {37, 20};
//Line(31) = {31, 32};
//Line(32) = {32, 33};
//Line(33) = {33, 34};
//Line(34) = {34, 37};
Line(39) = {39, 38};
Line(40) = {38, 1};
Line(41) = {39, 3};
Line(43) = {38, 39};


offset = 36;
start = 4;
end_ = 14;

lineID = 100;
loopID = 20;

lineID_right = 200;
loopID_right = 21;


For i In {start:end_ - 1}
  Line(lineID) = {i, offset + i};       // A
  Line(lineID + 1) = {offset + i, offset + i+1}; // B
  Line(lineID + 2) = {offset + i+1, i + 1};    // C
  Line(lineID + 3) = {i + 1, i};             // D

  Line(lineID_right) = {offset + i, offset + 2*start - i - 9};       // A
  Line(lineID_right + 1) = {offset + 2*start - i - 9, offset + 2*start - i - 10}; // B
  Line(lineID_right + 2) = {offset + 2*start - i - 10, offset + i + 1};    // C
  Line(lineID_right + 3) = {offset + i + 1, offset + i};             // D


  Line Loop(loopID+2) = {lineID, lineID + 1, lineID + 2, lineID + 3};
  Plane Surface(loopID+2) = {loopID+2};
  Transfinite Line{lineID, lineID + 1, lineID + 2, lineID + 3} = 11;
  Transfinite Surface{loopID+2};
  Recombine Surface{loopID+2};

  Line Loop(loopID_right) = {lineID_right, lineID_right + 1, lineID_right + 2, lineID_right + 3};
  Plane Surface(loopID_right) = {loopID_right};
  Transfinite Line{lineID_right, lineID_right + 1, lineID_right + 2, lineID_right + 3} = 11;
  Transfinite Surface{loopID_right};
  Recombine Surface{loopID_right};


  lineID += 4;
  loopID -= 2;
  lineID_right += 4;
  loopID_right -=2;
EndFor

// welding bottom part (1 and 2)
// (a)
Line(1001) = {21, 20};
Line(1002) = {20, 15};
Line(1003) = {15, 14};
Line Loop(1000) = {-138,-238,1001,1002,1003};
Plane Surface(2) ={1000};
Transfinite Line{1000,1001,1002,1003} = 11;
Recombine Surface{2};


// (b)
Line(1004) = {20, 37};
Line(1005) = {37, 19};
Line(1006) = {19, 18};
Line(1007) = {18, 17};
Line(1008) = {17, 16};
Line(1009) = {16, 36};
Line(1010) = {36, 15};
Line(1011) = {15, 20};
Line Loop(1001) = {1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011};
Plane Surface(1) ={1001};
Transfinite Line{1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011,16,15} = 11;
Recombine Surface{1};


// left trapezoid (big)
Line Loop(300) = {1, 2, 39,40};
Plane Surface(300) = {300};

Transfinite Line{1, 39} = 11;
Transfinite Line{2, 40} = 21;
Transfinite Surface{300};
Recombine Surface{300};


// left trapezoid (small)
Line(44) = {14,51};
Line(45) = {51,38};
Line(46) = {4,51};
Line Loop(301) = {41, 3,4, 5, 6, 7, 8, 9, 10, 11, 12, 13,44, 45,43};
Plane Surface(301) = {301};
Recombine Surface{301};
Transfinite Line{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 44,45} =11;
Transfinite Line{3} = 7;

// left smallest trapezoid
Line(47) = {36,51};
Line(48) = {51,14};
Line Loop(302) = {48,14,35,47};
Plane Surface(302) = {302};
Recombine Surface{302};
Transfinite Line{48,14,35,47} =11;

// symmetric copy
ySym = -0.1;

Symmetry {0, 1, 0, ySym} {
  Duplicata {
    Surface{ 302};
    Point{39,38,3}; // 2019, 2020,2021

  }
}

Transfinite Line{1004,1001,1025,1016,1026} =11;
Transfinite Line{1025,237,233,229,225,221,217,213,209,205,201} =11;
Transfinite Line{1014} = 7;


// Right trapezoid (big)
Line(50) = {2019, 33};
Line(51) = {33, 34};
Line(52) = {34, 2020};
Line(1027) = {2020,2019};
Line Loop(303) = {1027,50,51,52};
Plane Surface(303) = {303};
Transfinite Line{1027,51} = 11;
Transfinite Line{50,52} = 21;
Transfinite Surface{303};
Recombine Surface{303};


// Right trapezoid (small)
Line(1026) = {2003,2020};
Line(1014) = {2019,2021};
Line(1015) = {2021,31};
Line Loop(1013) = {201,205,209,213,217,221,225,229,233,237,-1013,1026,1027,1014,1015};
Plane Surface(1013) = {1013};
Recombine Surface{1013};
Transfinite Line{201,205,209,213,217,221,225,229,233,237, 1013,1026} =11;
Transfinite Line{1015} = 7;


// default subdomain
Physical Surface("default") = {300,301, 302,303,1013,1012};


// top two subdomains
// (a) left
Line(2000) = {3, 2000};
Line(2001) = {2000, 2001};
Line(2002) = {2001, 40};
Line Loop(24) = {2000,2001,2002,-100,-3};
Plane Surface(24) = {24};
Transfinite Line{2000} = 9;
Transfinite Line{2001} = 17;
Transfinite Line{2002} = 9;
Transfinite Line{2003} = 11;
Transfinite Line{2004} = 7;
Recombine Surface{24};

// (b) right
Line(2005) = {2001, 2002};
Line(2006) = {2002, 2021};
Line Loop(23) = {2005,2006,1015,-200, -2002};
Plane Surface(23) = {23};
Transfinite Line{2006} = 9;
Transfinite Line{2005} = 17;
Transfinite Line{2008} = 11;
Recombine Surface{23};



// Set the correct subdomain ID
For i In {1:24}
  Physical Surface(Sprintf("pass-%g", i)) = {i};
EndFor


// quad
Mesh.RecombineAll = 1;
// 0=Standard, 1=Blossom, 2=Simple, 3=SimpleFull (might be slow)
Mesh.RecombinationAlgorithm = 3;

// triangle
//Mesh.RecombineAll = 0;

Physical Line("bottom") = {40, 45, 47,1016,1026,52};
Physical Line("left") = {1};
Physical Line("right") = {51};

Physical Line("top") = {2, 41, 1014, 50, 3, 1015};

Physical Line("weld") = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 35, 1004, 1001, 237, 233, 229, 225, 221, 217, 213, 209, 205, 201};
Physical Line("weld_interior") = {1002,138,238,137,134,234,133,130,230,129,126,226,125,122,222,121,118,218,117,114,214,113,110,210,109,106,206,105,102,202,101,100,200,2002,2006,2005,2001,2000,15,16,1007,1006,1005};

Physical Point("fix_pt_right") = {33};
Physical Point("fix_pt_left") = {2};
