// Gmsh project created on Thu Apr 18 14:07:52 2024
SetFactory("OpenCASCADE");
Point(1) = {0, 0, -1.5, 1.0};
Point(2) = {-0.5, 0, -1.5, 1.0};
Point(3) = {0.5, 0, -1.5, 1.0};
Point(4) = {-0.65, 0, -1.5, 1.0};
Point(5) = {0.65, 0, -1.5, 1.0};
Line(1) = {3, 5};
Line(2) = {2, 4};
Circle(3) = {4, 1, 5};
Circle(4) = {5, 1, 4};
Circle(5) = {2, 1, 3};
Circle(6) = {3, 1, 2};
Curve Loop(1) = {3, -1, -5, 2};
Plane Surface(1) = {1};
Curve Loop(2) = {6, 2, -4, -1};
Plane Surface(2) = {2};
Curve Loop(3) = {3, 4};
Plane Surface(3) = {3};
Rectangle(4) = {-3, -2, -1.5, 6, 4, 0};
BooleanDifference{ Surface{4}; Delete; }{ Surface{3};}
Extrude {0, 0, 3} {
  Surface{1};
  Surface{2};
  Surface{4};
}
Coherence;
Save "channel.brep";
Delete All;
Merge "channel.brep";
Mesh.MeshSizeMin = 2;
Transfinite Curve {4, 10, 14, 16, 3, 9, 13, 15} = 5 Using Progression 1;
Transfinite Curve {1, 2, 5, 8} = 7 Using Progression 1;
Transfinite Curve {6, 7, 11, 12} = 2 Using Progression 1;
Transfinite Surface {1, 3, 7, 8, 5, 9, 6, 10, 4, 2};
Recombine Surface {1, 3, 7, 8, 5, 9, 6, 10, 4, 2};
Transfinite Volume {1, 2};
Mesh 3;
Coherence Mesh;

// Save the mesh
Mesh.MshFileVersion = 4.1;
Mesh.SaveParametric = 1;
Save "channel.msh";
