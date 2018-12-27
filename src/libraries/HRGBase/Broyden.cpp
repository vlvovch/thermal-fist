/*
 * Thermal-FIST package
 * 
 * Copyright (c) 2014-2018 Volodymyr Vovchenko
 *
 * GNU General Public License (GPLv3 or later)
 */
#include "HRGBase/Broyden.h"

#include <cmath>

using namespace Eigen;

using namespace std;

namespace thermalfist {

  const double BroydenJacobian::EPS = 1.0E-6;
  const double Broyden::TOL         = 1.0E-10;
  const int    Broyden::MAX_ITERS   = 200;


  MatrixXd BroydenJacobian::Jacobian(const std::vector<double>& x)
  {
    if (m_Equations == NULL) {
      printf("**ERROR** BroydenJacobian::Jacobian: Equations to solve not specified!\n");
      exit(1);
      //return MatrixXd();
    }

    if (m_Equations->Dimension() != x.size()) {
      printf("**ERROR** BroydenJacobian::Jacobian: Equations dimension does not match that of input!\n");
      exit(1);
      //return MatrixXd();
    }

    int N = m_Equations->Dimension();

    std::vector<double> h = x;

    std::vector< std::vector<double> > xh(N);

    for (int i = 0; i < x.size(); ++i) {
      h[i] = m_dx*abs(h[i]);
      if (h[i] == 0.0) h[i] = m_dx;
      //h[i] = max(m_dx, h[i]);
    }

    for (int i = 0; i < x.size(); ++i) {
      xh[i].resize(N);
      for (int j = 0; j < x.size(); ++j) {
        if (i != j)
          xh[i][j] = x[j];
        else
          xh[i][j] = x[j] + h[j];
      }
    }

    MatrixXd Jac(N,N);

    std::vector<double> fx = m_Equations->Equations(x);

    for (int j = 0; j < N; ++j) {
      std::vector<double> dfdxj = m_Equations->Equations(xh[j]);
      for (int i = 0; i < dfdxj.size(); ++i) {
        dfdxj[i] = (dfdxj[i] - fx[i]) / h[j];
        Jac(i, j) = dfdxj[i];
      }
    }
    
    return Jac;
  }

  std::vector<double> Broyden::Solve(const std::vector<double> &x0, BroydenSolutionCriterium *solcrit, int max_iterations)
  {
    if (m_Equations == NULL) {
      printf("**ERROR** Broyden::Solve: Equations to solve not specified!\n");
      exit(1);
    }

    m_MaxIterations = max_iterations;

    BroydenSolutionCriterium *SolutionCriterium = solcrit;
    bool UseDefaultSolutionCriterium = false;
    if (SolutionCriterium == NULL) {
      SolutionCriterium = new BroydenSolutionCriterium(TOL);
      UseDefaultSolutionCriterium = true;
    }
    
    BroydenJacobian *JacobianInUse = m_Jacobian;
    bool UseDefaultJacobian = false;
    if (JacobianInUse == NULL) {
      JacobianInUse = new BroydenJacobian(m_Equations);
      UseDefaultJacobian = true;
    }
    m_Iterations = 0;
    double &maxdiff = m_MaxDifference;
    int N = m_Equations->Dimension();
    std::vector<double> xcur = x0, tmpvec, xdeltavec = x0;
    VectorXd xold(N), xnew(N), xdelta(N);
    VectorXd fold(N), fnew(N), fdelta(N);

    xold = VectorXd::Map(&xcur[0], xcur.size());

    MatrixXd Jac(N, N), Jinv(N, N);
    Jac = JacobianInUse->Jacobian(xcur);

    if (Jac.determinant() == 0.0)
    {
      printf("**WARNING** Singular Jacobian in Broyden::Solve\n");
      return xcur;
    }

    Jinv   = Jac.inverse();
    tmpvec = m_Equations->Equations(xcur);
    fold   = VectorXd::Map(&tmpvec[0], tmpvec.size());

    for (m_Iterations = 1; m_Iterations < max_iterations; ++m_Iterations) {
      xnew = xold - Jinv * fold;

      VectorXd::Map(&xcur[0], xcur.size()) = xnew;

      tmpvec = m_Equations->Equations(xcur);
      fnew   = VectorXd::Map(&tmpvec[0], tmpvec.size());

      
      maxdiff = 0.;
      for (int i = 0; i < xcur.size(); ++i) {
        maxdiff = std::max(maxdiff, fabs(fnew[i]));
      }

      xdelta = xnew - xold;
      fdelta = fnew - fold;

      VectorXd::Map(&xdeltavec[0], xdeltavec.size()) = xdelta;

      if (SolutionCriterium->IsSolved(xcur, tmpvec, xdeltavec))
        break;
      //if (maxdiff < relative_error) break;

      if (!m_UseNewton) // Use Broyden's method
      {
        
        double norm = 0.;
        for (int i = 0; i < N; ++i)
          for (int j = 0; j < N; ++j)
            norm += xdelta[i] * Jinv(i, j) * fdelta[j];
        VectorXd p1(N);
        p1 = (xdelta - Jinv * fdelta);
        for (int i = 0; i < N; ++i) p1[i] *= 1. / norm;
        Jinv = Jinv + (p1 * xdelta.transpose()) * Jinv;
      }
      else // Use Newton's method
      {
        Jac = JacobianInUse->Jacobian(xcur);
        Jinv = Jac.inverse();
      }

      xold = xnew;
      fold = fnew;
    }

    if (UseDefaultSolutionCriterium) {
      delete SolutionCriterium;
      SolutionCriterium = NULL;
    }
    if (UseDefaultJacobian) {
      delete JacobianInUse;
      JacobianInUse = NULL;
    }
    return xcur;
  }

  //std::vector<double> Broyden::NewtonSolve(const std::vector<double>& x0, double relative_error, int max_iterations)
  //{
  //  if (m_Equations == NULL) {
  //    printf("**ERROR** Broyden::Solve: Equations to solve not specified!\n");
  //    exit(1);
  //  }

  //  BroydenJacobian *JacobianInUse = m_Jacobian;
  //  bool UseDefaultJacobian = false;
  //  if (JacobianInUse == NULL) {
  //    JacobianInUse = new BroydenJacobian(m_Equations);
  //    UseDefaultJacobian = true;
  //  }
  //  m_Iterations = 0;
  //  double &maxdiff = m_MaxDifference;
  //  int N = m_Equations->Dimension();
  //  std::vector<double> xcur = x0, tmpvec;
  //  VectorXd xold(N), xnew(N), xdelta(N);
  //  VectorXd fold(N), fnew(N), fdelta(N);

  //  xold = VectorXd::Map(&xcur[0], xcur.size());

  //  MatrixXd Jac(N, N), Jinv(N, N);
  //  Jac = JacobianInUse->Jacobian(xcur);

  //  if (Jac.determinant() == 0.0)
  //  {
  //    printf("**WARNING** Singular Jacobian in Broyden::SolveUsingJacobian\n");
  //    return xcur;
  //  }

  //  Jinv = Jac.inverse();
  //  tmpvec = m_Equations->Equations(xcur);
  //  fold = VectorXd::Map(&tmpvec[0], tmpvec.size());

  //  for (m_Iterations = 1; m_Iterations < max_iterations; ++m_Iterations) {
  //    xnew = xold - Jinv * fold;

  //    VectorXd::Map(&xcur[0], xcur.size()) = xnew;

  //    tmpvec = m_Equations->Equations(xcur);
  //    fnew = VectorXd::Map(&tmpvec[0], tmpvec.size());    

  //    maxdiff = 0.;
  //    for (int i = 0; i < xcur.size(); ++i) {
  //      maxdiff = std::max(maxdiff, fabs(fnew[i]));
  //      //if (xnew[i] > 0.0) maxdiff = std::max(maxdiff, fabs(fnew[i] / xnew[i]));
  //    }
  //    if (maxdiff < relative_error) break;

  //    Jac = JacobianInUse->Jacobian(xcur);
  //    Jinv = Jac.inverse();

  //    xold = xnew;
  //    fold = fnew;
  //  }

  //  if (UseDefaultJacobian) {
  //    delete JacobianInUse;
  //    JacobianInUse = NULL;
  //  }
  //  return xcur;
  //}

  bool Broyden::BroydenSolutionCriterium::IsSolved(const std::vector<double>& x, const std::vector<double>& f, const std::vector<double>& xdelta) const
  {
    double maxdiff = 0.;
    for (int i = 0; i < x.size(); ++i) {
      maxdiff = std::max(maxdiff, fabs(f[i]));
    }
    return (maxdiff < m_RelativeError);
  }

} // namespace thermalfist