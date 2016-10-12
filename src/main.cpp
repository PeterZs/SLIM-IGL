#include <iostream>

#include "Slim.h"

#include "igl/components.h"
#include "igl/readOBJ.h"
#include "igl/writeOBJ.h"
#include "igl/Timer.h"

#include "igl/boundary_loop.h"
#include "igl/map_vertices_to_circle.h"
#include "igl/harmonic.h"
#include <igl/serialize.h>
#include <igl/read_triangle_mesh.h>
#include <igl/viewer/Viewer.h>
#include <igl/flipped_triangles.h>
#include <igl/euler_characteristic.h>

#include <stdlib.h>

#include <string>
#include <vector>

using namespace std;
using namespace Eigen;

void check_mesh_for_issues(Eigen::MatrixXd& V, Eigen::MatrixXi& F, Eigen::VectorXd& areas);
void param_2d_demo_iter(igl::viewer::Viewer& viewer);
void set_soft_constraint_for_circle();
void soft_const_demo_iter(igl::viewer::Viewer& viewer);
void deform_3d_demo_iter(igl::viewer::Viewer& viewer);
void set_cube_corner_constraints();
void display_3d_mesh(igl::viewer::Viewer& viewer);
void int_set_to_eigen_vector(const std::set<int>& int_set, Eigen::VectorXi& vec);

Eigen::MatrixXd V;
Eigen::MatrixXi F;
bool first_iter = true;
Slim* slim = NULL;
igl::Timer timer;

double uv_scale_param;

enum DEMO_TYPE {
  PARAM_2D,
  SOFT_CONST,
  DEFORM_3D
};
DEMO_TYPE demo_type;

bool key_down(igl::viewer::Viewer& viewer, unsigned char key, int modifier){
  if (key == ' ') {
    switch (demo_type) {
      case PARAM_2D: {
        param_2d_demo_iter(viewer);
        break;
      }
      case SOFT_CONST: {
        soft_const_demo_iter(viewer);
        break;
      }
      case DEFORM_3D: {
        deform_3d_demo_iter(viewer);
        break;
      }
      default:
        break;
    }
  }

  return false;
}

void param_2d_demo_iter(igl::viewer::Viewer& viewer) {
  if (first_iter) {
    timer.start();
    igl::read_triangle_mesh("../models/face.obj", V, F);

    slim = new Slim(V,F);
    check_mesh_for_issues(V,F, slim->M);
    cout << "\tMesh is valid!" << endl;

    slim->slim_energy = Slim::SYMMETRIC_DIRICHLET;

    Eigen::VectorXi bnd; Eigen::MatrixXd bnd_uv;
    igl::boundary_loop(F,bnd);
    igl::map_vertices_to_circle(V,bnd,bnd_uv);

    igl::harmonic(V,F,bnd,bnd_uv,1,slim->V_o);
    if (igl::flipped_triangles(slim->V_o,slim->F).size() != 0) {
      igl::harmonic(F,bnd,bnd_uv,1,slim->V_o); // use uniform laplacian
    }

    cout << "initialized parametrization" << endl;
    slim->precompute();

    uv_scale_param = 15 * (1./sqrt(slim->mesh_area));
    viewer.data.set_mesh(V, F);
    viewer.core.align_camera_center(V,F);
    viewer.data.set_uv(slim->V_o*uv_scale_param);
    viewer.data.compute_normals();
    viewer.core.show_texture = true;

    first_iter = false;
  } else {
    slim->solve(1); // 1 iter
    viewer.data.set_uv(slim->V_o*uv_scale_param);
    cout << "time = " << timer.getElapsedTime() << endl;
  }
}

void soft_const_demo_iter(igl::viewer::Viewer& viewer) {
  if (first_iter) {

    igl::read_triangle_mesh("../models/circle.obj", V, F);
    slim = new Slim(V,F);

    check_mesh_for_issues(slim->V,slim->F, slim->M);
    cout << "\tMesh is valid!" << endl;
    slim->V_o = V.block(0,0,V.rows(),2);

    set_soft_constraint_for_circle();

    slim->slim_energy = Slim::SYMMETRIC_DIRICHLET;
    slim->soft_const_p = 1e5;
    
    slim->precompute();

    viewer.data.set_mesh(V, F);
    viewer.core.align_camera_center(V,F);
    viewer.data.compute_normals();
    viewer.core.show_lines = true;

    first_iter = false;

  } else {
    slim->solve(1); // 1 iter
    viewer.data.set_mesh(slim->V_o, F);
  }
}

void deform_3d_demo_iter(igl::viewer::Viewer& viewer) {
  if (first_iter) {
    igl::readOBJ("../models/cube_40k.obj", V, F);

    slim = new Slim(V,F);
    slim->V_o = V;

    set_cube_corner_constraints();
    display_3d_mesh(viewer);
    first_iter = false;

    slim->slim_energy = Slim::EXP_CONFORMAL;
    slim->soft_const_p = 1e5;
    slim->exp_factor = 5.0;
    slim = new Slim(*slim);
    slim->precompute();
  } else {
    slim->solve(1); // 1 iter
    display_3d_mesh(viewer);
  }
}

void display_3d_mesh(igl::viewer::Viewer& viewer) {
  MatrixXd V_temp; MatrixXi F_temp;
  Eigen::MatrixXd Barycenters;

  igl::barycenter(slim->V,slim->F,Barycenters);
  //cout << "Barycenters.rows() = " << Barycenters.rows() << endl;
  //double t = double((key - '1')+1) / 9.0;
  double view_depth = 10.;
  double t = view_depth/9.;

  VectorXd v = Barycenters.col(2).array() - Barycenters.col(2).minCoeff();
  v /= v.col(0).maxCoeff();

  vector<int> s;

  for (unsigned i=0; i<v.size();++i)
    if (v(i) < t)
      s.push_back(i);

  V_temp.resize(s.size()*4,3);
  F_temp.resize(s.size()*4,3);

  for (unsigned i=0; i<s.size();++i){
    V_temp.row(i*4+0) = slim->V_o.row(slim->F(s[i],0));
    V_temp.row(i*4+1) = slim->V_o.row(slim->F(s[i],1));
    V_temp.row(i*4+2) = slim->V_o.row(slim->F(s[i],2));
    V_temp.row(i*4+3) = slim->V_o.row(slim->F(s[i],3));
    F_temp.row(i*4+0) << (i*4)+0, (i*4)+1, (i*4)+3;
    F_temp.row(i*4+1) << (i*4)+0, (i*4)+2, (i*4)+1;
    F_temp.row(i*4+2) << (i*4)+3, (i*4)+2, (i*4)+0;
    F_temp.row(i*4+3) << (i*4)+1, (i*4)+2, (i*4)+3;
  }
  viewer.data.set_mesh(V_temp,F_temp);
  viewer.core.align_camera_center(V_temp,F_temp);
  viewer.data.set_face_based(true);
  viewer.core.show_lines = true;
}

int main(int argc, char *argv[]) {

  cerr << "Press space for running an iteration." << std::endl;

   if (argc < 2) {
      cerr << "Syntax: " << argv[0] << " demo_number (1 to 3)" << std::endl;
      cerr << "1. 2D unconstrained parametrization" << std::endl;
      cerr << "2. 2D parametrization with positional constraints" << std::endl;
      cerr << "3. 3D mesh deformation with positional constraints" << std::endl;
      return -1;
  }

  switch (std::atoi(argv[1])) {
    case 1: {
      demo_type = PARAM_2D;
      break;
    } case 2: {
      demo_type = SOFT_CONST;
      break;
    } case 3: {
      demo_type = DEFORM_3D;
      break;
    }
    default: {
      cerr << "Wrong demo number - Please choose one between 1-3" << std:: endl;
      exit(1);
    }
  }

  // Launch the viewer
  igl::viewer::Viewer viewer;
  viewer.callback_key_down = &key_down;

  // Disable wireframe
  viewer.core.show_lines = false;

  // Draw checkerboard texture
  viewer.core.show_texture = false;

  // First iteration
  key_down(viewer, ' ', 0);

  viewer.launch();

  return 0;
}

void check_mesh_for_issues(Eigen::MatrixXd& V, Eigen::MatrixXi& F, Eigen::VectorXd& areas) {

  Eigen::SparseMatrix<double> A;
  igl::adjacency_matrix(F,A);

  Eigen::MatrixXi C, Ci;
  igl::components(A, C, Ci);

  int connected_components = Ci.rows();
  if (connected_components!=1) {
    cout << "Error! Input has multiple connected components" << endl; exit(1);
  }
  int euler_char = igl::euler_characteristic(V, F);
  if (!euler_char) {
    cout << "Error! Input does not have a disk topology, it's euler char is " << euler_char << endl; exit(1);
  }
  bool is_edge_manifold = igl::is_edge_manifold(V, F);
  if (!is_edge_manifold) {
    cout << "Error! Input is not an edge manifold" << endl; exit(1);
  }
  const double eps = 1e-14;
  for (int i = 0; i < areas.rows(); i++) {
    if (areas(i) < eps) {
      cout << "Error! Input has zero area faces" << endl; exit(1);
    }
  }
}

void set_soft_constraint_for_circle() {

    Eigen::VectorXi bnd;
    igl::boundary_loop(slim->F,bnd);
    const int B_STEPS = 22; // constraint every B_STEPS vertices of the boundary

    slim->b.resize(bnd.rows()/B_STEPS);
    slim->bc.resize(slim->b.rows(),2);

    int c_idx = 0;
    for (int i = B_STEPS; i < bnd.rows(); i+=B_STEPS) {
        slim->b(c_idx) = bnd(i);
        c_idx++;
    }

    slim->bc.row(0) = slim->V_o.row(slim->b(0)); // keep it there for now
    slim->bc.row(1) = slim->V_o.row(slim->b(2));
    slim->bc.row(2) = slim->V_o.row(slim->b(3));
    slim->bc.row(3) = slim->V_o.row(slim->b(4));
    slim->bc.row(4) = slim->V_o.row(slim->b(5));


    slim->bc.row(0) << slim->V_o(slim->b(0),0), 0;
    slim->bc.row(4) << slim->V_o(slim->b(4),0), 0;
    slim->bc.row(2) << slim->V_o(slim->b(2),0), 0.1;
    slim->bc.row(3) << slim->V_o(slim->b(3),0), 0.05;
    slim->bc.row(1) << slim->V_o(slim->b(1),0), -0.15;
    slim->bc.row(5) << slim->V_o(slim->b(5),0), +0.15;
}

void set_cube_corner_constraints() {
  double min_x,max_x,min_y,max_y,min_z,max_z;
  min_x = slim->V.col(0).minCoeff(); max_x = slim->V.col(0).maxCoeff();
  min_y = slim->V.col(1).minCoeff(); max_y = slim->V.col(1).maxCoeff();
  min_z = slim->V.col(2).minCoeff(); max_z = slim->V.col(2).maxCoeff();


  // get all cube corners
  slim->b.resize(8,1); slim->bc.resize(8,3);
  int x;
  for (int i = 0; i < slim->V.rows(); i++) {
    if (slim->V.row(i) == Eigen::RowVector3d(min_x,min_y,min_z)) slim->b(0) = i;
    if (slim->V.row(i) == Eigen::RowVector3d(min_x,min_y,max_z)) slim->b(1) = i;
    if (slim->V.row(i) == Eigen::RowVector3d(min_x,max_y,min_z)) slim->b(2) = i;
    if (slim->V.row(i) == Eigen::RowVector3d(min_x,max_y,max_z)) slim->b(3) = i;
    if (slim->V.row(i) == Eigen::RowVector3d(max_x,min_y,min_z)) slim->b(4) = i;
    if (slim->V.row(i) == Eigen::RowVector3d(max_x,max_y,min_z)) slim->b(5) = i;
    if (slim->V.row(i) == Eigen::RowVector3d(max_x,min_y,max_z)) slim->b(6) = i;
    if (slim->V.row(i) == Eigen::RowVector3d(max_x,max_y,max_z)) slim->b(7) = i;
  }

  // get all cube edges
  std::set<int> cube_edge1; Eigen::VectorXi cube_edge1_vec;
  for (int i = 0; i < slim->V.rows(); i++) {
    if ((slim->V(i,0) == min_x && slim->V(i,1) == min_y)) {
      cube_edge1.insert(i);
    }
  }
  Eigen::VectorXi edge1;
  int_set_to_eigen_vector(cube_edge1, edge1);

  std::set<int> cube_edge2; Eigen::VectorXi edge2;
  for (int i = 0; i < slim->V.rows(); i++) {
    if ((slim->V(i,0) == max_x && slim->V(i,1) == max_y)) {
      cube_edge2.insert(i);
    }
  }
  int_set_to_eigen_vector(cube_edge2, edge2);
  slim->b = igl::cat(1,edge1,edge2);

  std::set<int> cube_edge3; Eigen::VectorXi edge3;
  for (int i = 0; i < slim->V.rows(); i++) {
    if ((slim->V(i,0) == max_x && slim->V(i,1) == min_y)) {
      cube_edge3.insert(i);
    }
  }
  int_set_to_eigen_vector(cube_edge3, edge3);
  slim->b = igl::cat(1,slim->b,edge3);

  std::set<int> cube_edge4; Eigen::VectorXi edge4;
  for (int i = 0; i < slim->V.rows(); i++) {
    if ((slim->V(i,0) == min_x && slim->V(i,1) == max_y)) {
      cube_edge4.insert(i);
    }
  }
  int_set_to_eigen_vector(cube_edge4, edge4);
  slim->b = igl::cat(1,slim->b,edge4);

  slim->bc.resize(slim->b.rows(),3);
  Eigen::Matrix3d m; m = Eigen::AngleAxisd(0.3 * M_PI, Eigen::Vector3d(1./sqrt(2.),1./sqrt(2.),0.)/*Eigen::Vector3d::UnitX()*/);
  int i = 0;
  for (; i < cube_edge1.size(); i++) {
    Eigen::RowVector3d edge_rot_center(min_x,min_y,(min_z+max_z)/2.);
    slim->bc.row(i) = (slim->V.row(slim->b(i)) - edge_rot_center) * m + edge_rot_center;
  }
  for (; i < cube_edge1.size() + cube_edge2.size(); i++) {
    Eigen::RowVector3d edge_rot_center(max_x,max_y,(min_z+max_z)/2.);
    slim->bc.row(i) = (slim->V.row(slim->b(i)) - edge_rot_center) * m.transpose() + edge_rot_center;
  }
  for (; i < cube_edge1.size() + cube_edge2.size() + cube_edge3.size(); i++) {
    slim->bc.row(i) = 0.75*slim->V.row(slim->b(i));
  }
  for (; i < slim->b.rows(); i++) {
    slim->bc.row(i) = 0.75*slim->V.row(slim->b(i));
  }
}

void int_set_to_eigen_vector(const std::set<int>& int_set, Eigen::VectorXi& vec) {
  vec.resize(int_set.size()); int idx = 0;
  for(auto f : int_set) {
      vec(idx) = f; idx++;
    }
}
