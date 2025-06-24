
// Auto-generated symmetric shape
lc = 1e-2;
Point(1) = {0.0, 0.0, 0, lc};
Point(2) = {0.035, 0.0, 0, lc};
Point(3) = {0.0349285714285714, 0.0877551020408163, 0, lc};
Point(4) = {0.0349264950166113, 0.09030612244897962, 0, lc};
Point(5) = {0.0321583610188261, 0.09100982050865421, 0, lc};
Point(6) = {0.0300418050941306, 0.09154787838395673, 0, lc};
Point(7) = {0.0269479512735326, 0.09233437894072659, 0, lc};
Point(8) = {0.0245056755260243, 0.09295523931312438, 0, lc};
Point(9) = {0.0214120985603543, 0.09374166948953122, 0, lc};
Point(10) = {0.0184811738648947, 0.09448675120266269, 0, lc};
Point(11) = {0.0148992248062015, 0.09539733233944017, 0, lc};
Point(12) = {0.0118056478405315, 0.09618376251584701, 0, lc};
Point(13) = {0.0080607696566998, 0.0971357624962629, 0, lc};
Point(14) = {0.0041526854928017, 0.0981292517006802, 0, lc};
Point(15) = {0.0021965669988925, 0.101360544217687, 0, lc};
Point(16) = {-0.0008964562569213, 0.101360544217687, 0, lc};
Point(17) = {-0.0020372369878183, 0.102891156462585, 0, lc};
Point(18) = {-0.0020372369878183, 0.10408163265306121, 0, lc};
Point(19) = {-0.0008964562569213, 0.1056122448979592, 0, lc};
Point(20) = {0.0021965669988925, 0.1056122448979592, 0, lc};
Point(21) = {0.0041526854928017, 0.10884353741496601, 0, lc};
Point(22) = {0.0080607696566998, 0.1098370266193833, 0, lc};
Point(23) = {0.0118056478405315, 0.1107890265997992, 0, lc};
Point(24) = {0.0148992248062015, 0.11157545677620603, 0, lc};
Point(25) = {0.0184811738648947, 0.11248603791298352, 0, lc};
Point(26) = {0.0214120985603543, 0.11323111962611498, 0, lc};
Point(27) = {0.0245056755260243, 0.11401754980252182, 0, lc};
Point(28) = {0.0269479512735326, 0.11463841017491962, 0, lc};
Point(29) = {0.0300418050941306, 0.11542491073168948, 0, lc};
Point(30) = {0.0321583610188261, 0.115962968606992, 0, lc};
Point(31) = {0.0349264950166113, 0.11666666666666659, 0, lc};
Point(32) = {0.0349285714285714, 0.11921768707482991, 0, lc};
Point(33) = {0.035, 0.2, 0, lc};
Point(34) = {0.0, 0.2, 0, lc};
Point(35) = {0.0, 0.0, 0, lc};
Point(36) = {0.0, 0.101360544217687, 0, lc};
Point(37) = {0.0, 0.1056122448979592, 0, lc};
Point(38) = {0.0, 0.09, 0, lc};
Point(39) = {0.035, 0.082, 0, lc};
Point(40) = {0.0349264950166113, 0.1034863945578231, 0, lc};
Point(41) = {0.0321583610188261, 0.1034863945578231, 0, lc};
Point(42) = {0.0300418050941306, 0.1034863945578231, 0, lc};
Point(43) = {0.0269479512735326, 0.1034863945578231, 0, lc};
Point(44) = {0.0245056755260243, 0.1034863945578231, 0, lc};
Point(45) = {0.0214120985603543, 0.1034863945578231, 0, lc};
Point(46) = {0.0184811738648947, 0.1034863945578231, 0, lc};
Point(47) = {0.0148992248062015, 0.1034863945578231, 0, lc};
Point(48) = {0.0118056478405315, 0.1034863945578231, 0, lc};
Point(49) = {0.0080607696566998, 0.1034863945578231, 0, lc};
Point(50) = {0.0041526854928017, 0.1034863945578231, 0, lc};
Point(51) = {0.0, 0.09918492195582734, 0, lc};
Point(2000) = {0.037345078815609806, 0.09008875739644973, 0, lc};
Point(2001) = {0.037345078815609806, 0.1034863945578231, 0, lc};
Point(2002) = {0.037345078815609806, 0.11688403171, 0, lc};

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
  Transfinite Line{lineID, lineID + 1, lineID + 2, lineID + 3} = 10;
  Transfinite Surface{loopID+2};
  //Recombine Surface{loopID+2};

  Line Loop(loopID_right) = {lineID_right, lineID_right + 1, lineID_right + 2, lineID_right + 3};
  Plane Surface(loopID_right) = {loopID_right};
  Transfinite Line{lineID_right, lineID_right + 1, lineID_right + 2, lineID_right + 3} = 10;
  Transfinite Surface{loopID_right};
  //Recombine Surface{loopID_right};


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
Transfinite Line{1000,1001,1002,1003} = 10;
//Recombine Surface{2};


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
Transfinite Line{1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011,16,15} = 10;
//Recombine Surface{1};


// left trapezoid (big)
Line Loop(300) = {1, 2, 39,40};
Plane Surface(300) = {300};

Transfinite Line{1, 39} = 11;
Transfinite Line{2, 40} = 20;
Transfinite Surface{300};
//Recombine Surface{300};


// left trapezoid (small)
Line(44) = {14,51};
Line(45) = {51,38};
Line(46) = {4,51};
Line Loop(301) = {41, 3,4, 5, 6, 7, 8, 9, 10, 11, 12, 13,44, 45,43};
Plane Surface(301) = {301};
//Recombine Surface{301};
Transfinite Line{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 44,45} =10;
Transfinite Line{3} = 7;
Transfinite Line{43} =11;

// left smallest trapezoid
Line(47) = {36,51};
Line(48) = {51,14};
Line Loop(302) = {48,14,35,47};
Plane Surface(302) = {302};
//Recombine Surface{302};
Transfinite Line{48,14,35,47} =10;

// symmetric copy
ySym = -0.1034863945578231;

Symmetry {0, 1, 0, ySym} {
  Duplicata {
    Surface{301, 302}; // become surface 1012 and 1028
  }
}

Transfinite Line{1004,1001,1025,1032,1026} =10;
Transfinite Line{1025,237,233,229,225,221,217,213,209,205,201} =10;
Transfinite Line{1014} = 7;
Transfinite Line{1027} =11;


// Right trapezoid (big)
Line(50) = {2003, 33};
Line(51) = {33, 34};
Line(52) = {34, 2056};
Line Loop(303) = {1027,50,51,52};
Plane Surface(303) = {303};
Transfinite Line{1027,51} = 11;
Transfinite Line{50,52} = 20;
Transfinite Surface{303};
//Recombine Surface{303};

// default subdomain
Physical Surface("default") = {300,301, 302,303,1012,1028};


// top two subdomains
// (a) left
Line(2000) = {3, 2000};
Line(2001) = {2000, 2001};
Line(2002) = {2001, 40};
Line Loop(24) = {2000,2001,2002,-100,-3};
Plane Surface(24) = {24};
Transfinite Line{2000} = 8;
Transfinite Line{2001} = 17;
Transfinite Line{2002} = 8;
Transfinite Line{2003} = 10;
Transfinite Line{2004} = 7;
//Recombine Surface{24};

// (b) right
Line(2005) = {2001, 2002};
Line(2006) = {2002, 32};
Line Loop(23) = {2005,2006,1014,-200, -2002};
Plane Surface(23) = {23};
Transfinite Line{2006} = 8;
Transfinite Line{2005} = 17;
Transfinite Line{2008} = 10;
Transfinite Line{2007} = 7;
//Recombine Surface{24};



// Set the correct subdomain ID
For i In {1:24}
  Physical Surface(Sprintf("pass-%g", i)) = {i};
EndFor


// quad
//Mesh.RecombineAll = 1;

// triangle
Mesh.RecombineAll = 0;

Physical Line("bottom") = {40, 45, 47,1032,1026,52};
Physical Line("left") = {1};
Physical Line("right") = {51};

Physical Line("top") = {2, 41, 1013, 50, 3, 1014};

Physical Line("weld") = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 35, 1004, 1001, 237, 233, 229, 225, 221, 217, 213, 209, 205, 201};
Physical Line("weld_interior") = {1002,138,238,137,134,234,133,130,230,129,126,226,125,122,222,121,118,218,117,114,214,113,110,210,109,106,206,105,102,202,101,100,200,2002,2006,2005,2001,2000,15,16,1007,1006,1005};

Physical Point("fix_pt") = {33, 2};
