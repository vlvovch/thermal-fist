/*
 * Thermal-FIST package
 * 
 * Copyright (c) 2014-2018 Volodymyr Vovchenko
 *
 * GNU General Public License (GPLv3 or later)
 */
#include "HRGEV/ThermalModelEVCrossterms.h"

#ifdef USE_OPENMP
#include <omp.h>
#endif

#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <sstream>

#include "HRGBase/xMath.h"
#include "HRGEV/ExcludedVolumeHelper.h"

#include <Eigen/Dense>

using namespace Eigen;

using namespace std;

namespace thermalfist {

  ThermalModelEVCrossterms::ThermalModelEVCrossterms(ThermalParticleSystem *TPS_, const ThermalModelParameters& params, double RHad_, int mode) :
    ThermalModelBase(TPS_, params),
    m_RHad(RHad_),
    m_Mode(mode)
  {
    m_densitiesid.resize(m_TPS->Particles().size());
    m_Volume = params.V;
    m_Ps.resize(m_TPS->Particles().size());
    FillVirial(std::vector<double>(m_TPS->Particles().size(), 0.));
    m_TAG = "ThermalModelEVCrossterms";

    m_Ensemble = GCE;
    m_InteractionModel = CrosstermsEV;
  }

  ThermalModelEVCrossterms::~ThermalModelEVCrossterms(void)
  {
  }

  void ThermalModelEVCrossterms::FillVirial(const std::vector<double> & ri) {
    if (ri.size() != m_TPS->Particles().size()) {
      printf("**WARNING** %s::FillVirial(const std::vector<double> & ri): size %d of ri does not match number of hadrons %d in the list", m_TAG.c_str(), static_cast<int>(ri.size()), static_cast<int>(m_TPS->Particles().size()));
      return;
    }
    m_Virial.resize(m_TPS->Particles().size());
    for (int i = 0; i < m_TPS->Particles().size(); ++i) {
      m_Virial[i].resize(m_TPS->Particles().size());
      for (int j = 0; j < m_TPS->Particles().size(); ++j)
        m_Virial[i][j] = CuteHRGHelper::brr(ri[i], ri[j]);
    }

    // Correction for non-diagonal terms R1=R2 and R2=0
    std::vector< std::vector<double> > fVirialTmp = m_Virial;
    for (int i = 0; i < m_TPS->Particles().size(); ++i)
      for (int j = 0; j < m_TPS->Particles().size(); ++j) {
        if (i == j) m_Virial[i][j] = fVirialTmp[i][j];
        else if ((fVirialTmp[i][i] + fVirialTmp[j][j]) > 0.0) m_Virial[i][j] = 2. * fVirialTmp[i][j] * fVirialTmp[i][i] / (fVirialTmp[i][i] + fVirialTmp[j][j]);
      }
  }

  void ThermalModelEVCrossterms::SetParameters(double T, double muB, double muS, double muQ, double gammaS, double V, double R) {
    m_Parameters.T = T;
    m_Parameters.muB = muB;
    m_Parameters.muS = muS;
    m_Parameters.muQ = muQ;
    m_Parameters.gammaS = gammaS;
    m_Parameters.V = V;
    m_Calculated = false;
  }

  void ThermalModelEVCrossterms::ReadInteractionParameters(const std::string & filename)
  {
    m_Virial = std::vector< std::vector<double> >(m_TPS->Particles().size(), std::vector<double>(m_TPS->Particles().size(), 0.));

    ifstream fin(filename);
    char cc[2000];
    while (!fin.eof()) {
      fin.getline(cc, 2000);
      string tmp = string(cc);
      vector<string> elems = CuteHRGHelper::split(tmp, '#');
      if (elems.size() < 1)
        continue;
      istringstream iss(elems[0]);
      int pdgid1, pdgid2;
      double b;
      if (iss >> pdgid1 >> pdgid2 >> b) {
        int ind1 = m_TPS->PdgToId(pdgid1);
        int ind2 = m_TPS->PdgToId(pdgid2);
        if (ind1 != -1 && ind2 != -1)
          m_Virial[ind1][ind2] = b;
      }
    }
    fin.close();
  }

  void ThermalModelEVCrossterms::WriteInteractionParameters(const std::string & filename)
  {
    ofstream fout(filename);
    for (int i = 0; i < m_TPS->Particles().size(); ++i) {
      for (int j = 0; j < m_TPS->Particles().size(); ++j) {
        fout << std::setw(15) << m_TPS->Particle(i).PdgId();
        fout << std::setw(15) << m_TPS->Particle(j).PdgId();
        fout << std::setw(15) << m_Virial[i][j];
        fout << std::endl;
      }
    }
    fout.close();
  }

  void ThermalModelEVCrossterms::SetRadius(double rad) {
    m_RHad = rad;
    FillVirial(vector<double>(m_TPS->Particles().size(), rad));
  }

  void ThermalModelEVCrossterms::DisableBBarRepulsion() {
    for (int i = 0; i < m_TPS->Particles().size(); ++i)
      for (int j = 0; j < m_TPS->Particles().size(); ++j) {
        if (m_TPS->Particles()[i].BaryonCharge() != 0 && m_TPS->Particles()[j].BaryonCharge() != 0 && m_TPS->Particles()[i].BaryonCharge()*m_TPS->Particles()[j].BaryonCharge() < 0) {
          m_Virial[i][j] = m_Virial[j][i] = 0.;
        }
      }
  }

  double ThermalModelEVCrossterms::VirialCoefficient(int i, int j) const {
    if (i < 0 || i >= m_Virial.size() || j<0 || j>m_Virial.size())
      return 0.;
    return m_Virial[i][j];
  }

  void ThermalModelEVCrossterms::SetVirial(int i, int j, double b) {
    if (i >= 0 && i < m_Virial.size() && j >= 0 && j < m_Virial.size()) m_Virial[i][j] = b;
    else printf("**WARNING** Index overflow in ThermalModelEVCrossterms::SetVirial\n");
  }

  void ThermalModelEVCrossterms::SetParameters(const ThermalModelParameters& params) {
    m_Parameters = params;
    m_Calculated = false;
  }

  void ThermalModelEVCrossterms::ChangeTPS(ThermalParticleSystem *TPS_) {
    ThermalModelBase::ChangeTPS(TPS_);
    m_densitiesid.resize(m_TPS->Particles().size());
    FillVirial();
  }

  double ThermalModelEVCrossterms::DensityId(int i) {
    double ret = 0.;

    double dMu = 0.;
    for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * m_Ps[j];

    return m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::ParticleDensity, m_UseWidth, m_Chem[i], dMu);
  }

  double ThermalModelEVCrossterms::Pressure(int i) {
    double ret = 0.;

    double dMu = 0.;
    for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * m_Ps[j];

    return m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::Pressure, m_UseWidth, m_Chem[i], dMu);
  }

  double ThermalModelEVCrossterms::DensityId(int i, const std::vector<double>& pstars)
  {
    double ret = 0.;

    double dMu = 0.;
    for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * pstars[j];

    return m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::ParticleDensity, m_UseWidth, m_Chem[i], dMu);
  }

  double ThermalModelEVCrossterms::Pressure(int i, const std::vector<double>& pstars)
  {
    double ret = 0.;

    double dMu = 0.;
    for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * pstars[j];

    return m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::Pressure, m_UseWidth, m_Chem[i], dMu);
  }

  double ThermalModelEVCrossterms::ScaledVarianceId(int i) {
    double ret = 0.;

    double dMu = 0.;
    for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * m_Ps[j];

    return m_TPS->Particles()[i].ScaledVariance(m_Parameters, m_UseWidth, m_Chem[i], dMu);
  }

  double ThermalModelEVCrossterms::PressureDiagonal(int i, double P) {
    double ret = 0.;

    double dMu = 0.;
    dMu += -m_Virial[i][i] * P;

    return m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::Pressure, m_UseWidth, m_Chem[i], dMu);
  }


  double ThermalModelEVCrossterms::PressureDiagonalTotal(double P) {
    double ret = 0.;
    for (int i = 0; i < m_TPS->Particles().size(); ++i)
      ret += PressureDiagonal(i, P);
    return ret;
  }

  void ThermalModelEVCrossterms::SolveDiagonal() {
    BroydenEquationsCRSDEV eqs(this);
    BroydenJacobian jac(&eqs);
    jac.SetDx(1.0E-8);
    Broyden broydn(&eqs, &jac);
    Broyden::BroydenSolutionCriterium crit(1.0E-8);

    m_Pressure = 0.;
    double x0 = m_Pressure;
    std::vector<double> x(1, x0);

    x = broydn.Solve(x, &crit);

    m_Pressure = x[0];
    for (int i = 0; i < m_Ps.size(); ++i)
      m_Ps[i] = PressureDiagonal(i, m_Pressure);
  }


  void ThermalModelEVCrossterms::SolvePressure(bool resetPartials) {
    if (resetPartials) {
      m_Ps.resize(m_TPS->Particles().size());
      for (int i = 0; i < m_Ps.size(); ++i) m_Ps[i] = 0.;
      SolveDiagonal();
    }

    BroydenEquationsCRS eqs(this);
    BroydenJacobianCRS  jac(this);
    Broyden broydn(&eqs, &jac);
    BroydenSolutionCriteriumCRS crit(this);

    m_Ps = broydn.Solve(m_Ps, &crit);
    m_Pressure = 0.;
    for (int i = 0; i < m_Ps.size(); ++i) m_Pressure += m_Ps[i];

    if (broydn.Iterations() == broydn.MaxIterations())
      m_LastCalculationSuccessFlag = false;
    else m_LastCalculationSuccessFlag = true;

    m_MaxDiff = broydn.MaxDifference();
  }

  void ThermalModelEVCrossterms::CalculateDensities() {
    m_FluctuationsCalculated = false;

    // Pressure
    SolvePressure();
    vector<double> tN(m_densities.size());
    for (int i = 0; i < m_Ps.size(); ++i) tN[i] = DensityId(i);

    // Densities

    int NN = m_densities.size();

    MatrixXd densMatrix(NN, NN);
    VectorXd solVector(NN), xVector(NN);

    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j) {
        densMatrix(i, j) = m_Virial[i][j] * tN[i];
        if (i == j) densMatrix(i, j) += 1.;
      }

    PartialPivLU<MatrixXd> decomp(densMatrix);

    for (int i = 0; i < NN; ++i) xVector[i] = 0.;
    for (int i = 0; i < NN; ++i) {
      xVector[i] = tN[i];
      solVector = decomp.solve(xVector);
      if (1) {
        m_densities[i] = 0.;
        for (int j = 0; j < NN; ++j)
          m_densities[i] += solVector[j];
      }
      else {
        cout << "Could not recover m_densities from partial pressures!\n";
      }
      xVector[i] = 0.;
    }

    std::vector<double> fEntropyP(m_densities.size());
    for (int i = 0; i < NN; ++i) {
      double dMu = 0.;
      for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * m_Ps[j];
      xVector[i] = m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::EntropyDensity, m_UseWidth, m_Chem[i], dMu);
    }

    solVector = decomp.solve(xVector);

    if (1) {
      m_TotalEntropyDensity = 0.;
      for (int i = 0; i < NN; ++i)
        m_TotalEntropyDensity += solVector[i];
    }
    else {
      cout << "**ERROR** Could not recover m_densities from partial pressures!\n";
      return;
    }


    // Decays
    CalculateFeeddown();

    m_Calculated = true;
    ValidateCalculation();
  }

  void ThermalModelEVCrossterms::CalculateDensitiesNoReset() {
    // Pressure
    SolvePressure(false);
    vector<double> tN(m_densities.size());
    for (int i = 0; i < m_Ps.size(); ++i) tN[i] = DensityId(i);

    // Densities

    int NN = m_densities.size();

    MatrixXd densMatrix(NN, NN);
    VectorXd solVector(NN), xVector(NN);

    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j) {
        densMatrix(i, j) = m_Virial[i][j] * tN[i];
        if (i == j) densMatrix(i, j) += 1.;
      }

    PartialPivLU<MatrixXd> decomp(densMatrix);

    for (int i = 0; i < NN; ++i) xVector[i] = 0.;
    for (int i = 0; i < NN; ++i) {
      xVector[i] = tN[i];//m_Ps[i] / m_Parameters.T;
      //solVector = lu.Solve(xVector, ok);
      solVector = decomp.solve(xVector);
      //if (ok) {
      if (1) {
        //if (decomp.info()==Eigen::Success) {
        m_densities[i] = 0.;
        for (int j = 0; j < NN; ++j)
          m_densities[i] += solVector[j];
      }
      else {
        cout << "**ERROR** Could not recover m_densities from partial pressures!\n";
        return;
      }
      xVector[i] = 0.;
    }

    std::vector<double> fEntropyP(m_densities.size());
    for (int i = 0; i < NN; ++i) {
      double dMu = 0.;
      for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * m_Ps[j];
      xVector[i] = m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::EntropyDensity, m_UseWidth, m_Chem[i], dMu);
    }

    solVector = decomp.solve(xVector);

    //if (ok) {
    if (1) {
      //if (decomp.info()==Eigen::Success) {
      m_TotalEntropyDensity = 0.;
      for (int i = 0; i < NN; ++i)
        m_TotalEntropyDensity += solVector[i];
    }
    else {
      cout << "**ERROR** Could not recover m_densities from partial pressures!\n";
      return;
    }

    // Decays

    CalculateFeeddown();

    m_Calculated = true;
  }

  void ThermalModelEVCrossterms::SolvePressureIter() {
    m_Ps.resize(m_TPS->Particles().size());
    for (int i = 0; i < m_Ps.size(); ++i) m_Ps[i] = 0.;
    SolveDiagonal();
    vector<double> Pstmp = m_Ps;
    int iter = 0;
    double maxdiff = 0.;
    for (iter = 0; iter < 1000; ++iter)
    {
      maxdiff = 0.;
      for (int i = 0; i < m_Ps.size(); ++i) {
        Pstmp[i] = Pressure(i);
        maxdiff = max(maxdiff, fabs((Pstmp[i] - m_Ps[i]) / Pstmp[i]));
      }
      m_Ps = Pstmp;
      //cout << iter << "\t" << maxdiff << "\n";
      if (maxdiff < 1.e-10) break;
    }
    if (iter == 1000) cout << iter << "\t" << maxdiff << "\n";
    m_Pressure = 0.;
    for (int i = 0; i < m_Ps.size(); ++i) m_Pressure += m_Ps[i];
  }

  void ThermalModelEVCrossterms::CalculateDensitiesIter() {
    // Pressure
    SolvePressureIter();

    int NN = m_densities.size();
    vector<double> tN(NN);
    for (int i = 0; i < NN; ++i) tN[i] = DensityId(i);

    MatrixXd densMatrix(NN, NN);
    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j) {
        densMatrix(i, j) = m_Virial[j][i] * tN[i];
        if (i == j) densMatrix(i, j) += 1.;
      }
    //densMatrix.SetMatrixArray(matr.GetArray());

    VectorXd solVector(NN), xVector(NN);
    for (int i = 0; i < NN; ++i) xVector[i] = tN[i];

    PartialPivLU<MatrixXd> decomp(densMatrix);

    solVector = decomp.solve(xVector);

    if (1) {
      //if (decomp.info()==Eigen::Success) {
      for (int i = 0; i < NN; ++i)
        m_densities[i] = solVector[i];
    }
    else {
      cout << "**ERROR** Could not recover m_densities from partial pressures!\n";
      return;
    }

    // Entropy
    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j) {
        densMatrix(i, j) = m_Virial[i][j] * tN[i];
        if (i == j) densMatrix(i, j) += 1.;
      }

    std::vector<double> fEntropyP(m_densities.size());
    for (int i = 0; i < NN; ++i) {
      double dMu = 0.;
      for (int j = 0; j < m_TPS->Particles().size(); ++j) dMu += -m_Virial[i][j] * m_Ps[j];
      xVector[i] = m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::EntropyDensity, m_UseWidth, m_Chem[i], dMu);
    }

    decomp = PartialPivLU<MatrixXd>(densMatrix);
    solVector = decomp.solve(xVector);

    if (1) {
      //if (decomp.info()==Eigen::Success) {
      m_TotalEntropyDensity = 0.;
      for (int i = 0; i < NN; ++i)
        m_TotalEntropyDensity += solVector[i];
    }
    else {
      cout << "Could not recover entropy m_densities from partial pressures!\n";
    }

    // Decays

    CalculateFeeddown();

    m_Calculated = true;
  }

  void ThermalModelEVCrossterms::CalculateTwoParticleCorrelations() {
    int NN = m_densities.size();
    vector<double> tN(NN), tW(NN);
    for (int i = 0; i < NN; ++i) tN[i] = DensityId(i);
    for (int i = 0; i < NN; ++i) tW[i] = ScaledVarianceId(i);
    MatrixXd densMatrix(NN, NN);
    VectorXd solVector(NN), xVector(NN), xVector2(NN);

    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j) {
        densMatrix(i, j) = m_Virial[i][j] * tN[i];
        if (i == j) densMatrix(i, j) += 1.;
      }

    PartialPivLU<MatrixXd> decomp(densMatrix);

    vector< vector<double> > ders, coefs;

    ders.resize(NN);
    coefs.resize(NN);

    for (int i = 0; i < NN; ++i) {
      ders[i].resize(NN);
      coefs[i].resize(NN);
    }

    for (int i = 0; i < NN; ++i) xVector[i] = 0.;
    for (int i = 0; i < NN; ++i) {
      xVector[i] = tN[i];
      solVector = decomp.solve(xVector);
      if (1) {
        //if (decomp.info()==Eigen::Success) {
        for (int j = 0; j < NN; ++j) {
          ders[j][i] = solVector[j];
        }

        for (int l = 0; l < NN; ++l) {
          coefs[l][i] = 0.;
          for (int k = 0; k < NN; ++k) {
            coefs[l][i] += -m_Virial[l][k] * ders[k][i];
          }
          if (l == i) coefs[l][i] += 1.;
        }
      }
      else {
        cout << "**WARNING** Could not recover fluctuations!\n";
      }
      xVector[i] = 0.;
    }


    m_PrimCorrel.resize(NN);
    for (int i = 0; i < NN; ++i) m_PrimCorrel[i].resize(NN);
    m_TotalCorrel = m_PrimCorrel;

    for (int i = 0; i < NN; ++i)
      for (int j = i; j < NN; ++j) {
        for (int l = 0; l < NN; ++l)
          xVector[l] = tN[l] / m_Parameters.T * tW[l] * coefs[l][i] * coefs[l][j];
        solVector = decomp.solve(xVector);
        if (1) {
          //if (decomp.info()==Eigen::Success) {
          m_PrimCorrel[i][j] = 0.;
          for (int k = 0; k < NN; ++k) {
            m_PrimCorrel[i][j] += solVector[k];
          }
          m_PrimCorrel[j][i] = m_PrimCorrel[i][j];
        }
        else {
          cout << "**WARNING** Could not recover fluctuations!\n";
        }
      }

    //cout << "Primaries solved!\n";

    for (int i = 0; i < NN; ++i) {
      m_wprim[i] = m_PrimCorrel[i][i];
      if (m_densities[i] > 0.) m_wprim[i] *= m_Parameters.T / m_densities[i];
      else m_wprim[i] = 1.;
    }

    CalculateSusceptibilityMatrix();
    CalculateTwoParticleFluctuationsDecays();
    CalculateProxySusceptibilityMatrix();
  }

  // TODO include correlations
  void ThermalModelEVCrossterms::CalculateFluctuations() {

    CalculateTwoParticleCorrelations();

    m_FluctuationsCalculated = true;

    for (int i = 0; i < m_wprim.size(); ++i) {
      //m_wprim[i] = CalculateParticleScaledVariance(i);
      m_skewprim[i] = 1.;//CalculateParticleSkewness(i);
      m_kurtprim[i] = 1.;//CalculateParticleKurtosis(i);
    }
    for (int i = 0; i < m_wtot.size(); ++i) {
      double tmp1 = 0., tmp2 = 0., tmp3 = 0., tmp4 = 0.;
      tmp2 = m_densities[i] * m_wprim[i];
      tmp3 = m_densities[i] * m_wprim[i] * m_skewprim[i];
      tmp4 = m_densities[i] * m_wprim[i] * m_kurtprim[i];
      for (int r = 0; r < m_TPS->Particles()[i].DecayContributions().size(); ++r) {
        tmp2 += m_densities[m_TPS->Particles()[i].DecayContributions()[r].second] *
          (m_wprim[m_TPS->Particles()[i].DecayContributions()[r].second] * m_TPS->Particles()[i].DecayContributions()[r].first * m_TPS->Particles()[i].DecayContributions()[r].first
            + m_TPS->Particles()[i].DecayContributionsSigmas()[r].first);

        int rr = m_TPS->Particles()[i].DecayContributions()[r].second;
        double ni = m_TPS->Particles()[i].DecayContributions()[r].first;
        tmp3 += m_densities[rr] * m_wprim[rr] * (m_skewprim[rr] * ni * ni * ni + 3. * ni * m_TPS->Particles()[i].DecayCumulants()[r].first[1]);
        tmp3 += m_densities[rr] * m_TPS->Particles()[i].DecayCumulants()[r].first[2];

        tmp4 += m_densities[rr] * m_wprim[rr] * (m_kurtprim[rr] * ni * ni * ni * ni
          + 6. * m_skewprim[rr] * ni * ni * m_TPS->Particles()[i].DecayCumulants()[r].first[1]
          + 3. * m_TPS->Particles()[i].DecayCumulants()[r].first[1] * m_TPS->Particles()[i].DecayCumulants()[r].first[1]
          + 4. * ni * m_TPS->Particles()[i].DecayCumulants()[r].first[2]);

        tmp4 += m_densities[rr] * m_TPS->Particles()[i].DecayCumulants()[r].first[3];
      }

      tmp1 = m_densitiestotal[i];

      m_skewtot[i] = tmp3 / tmp2;
      m_kurttot[i] = tmp4 / tmp2;
    }
  }

  std::vector<double> ThermalModelEVCrossterms::CalculateChargeFluctuations(const std::vector<double>& chgs, int order)
  {
    vector<double> ret(order + 1, 0.);

    // chi1
    for (int i = 0; i < m_densities.size(); ++i)
      ret[0] += chgs[i] * m_densities[i];

    ret[0] /= pow(m_Parameters.T * xMath::GeVtoifm(), 3);

    if (order < 2) return ret;
    // Preparing matrix for system of linear equations
    int NN = m_densities.size();

    vector<double> MuStar(NN, 0.);
    for (int i = 0; i < NN; ++i) {
      MuStar[i] = m_Chem[i] + MuShift(i);
    }

    MatrixXd densMatrix(2 * NN, 2 * NN);
    VectorXd solVector(2 * NN), xVector(2 * NN);

    vector<double> DensitiesId(m_densities.size()), chi2id(m_densities.size());
    for (int i = 0; i < NN; ++i) {
      DensitiesId[i] = m_TPS->Particles()[i].Density(m_Parameters, IdealGasFunctions::ParticleDensity, m_UseWidth, MuStar[i], 0.);
      chi2id[i] = m_TPS->Particles()[i].chi(2, m_Parameters, m_UseWidth, MuStar[i], 0.);
    }

    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j) {
        densMatrix(i, j) = m_Virial[j][i] * DensitiesId[i];
        if (i == j) densMatrix(i, j) += 1.;
      }

    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j)
        densMatrix(i, NN + j) = 0.;

    for (int i = 0; i < NN; ++i) {
      densMatrix(i, NN + i) = 0.;
      for (int k = 0; k < NN; ++k) {
        densMatrix(i, NN + i) += m_Virial[k][i] * m_densities[k];
      }
      densMatrix(i, NN + i) = (densMatrix(i, NN + i) - 1.) * chi2id[i] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * m_Parameters.T;
    }

    for (int i = 0; i < NN; ++i)
      for (int j = 0; j < NN; ++j) {
        densMatrix(NN + i, NN + j) = m_Virial[i][j] * DensitiesId[j];
        if (i == j) densMatrix(NN + i, NN + j) += 1.;
      }


    PartialPivLU<MatrixXd> decomp(densMatrix);

    // chi2
    vector<double> dni(NN, 0.), dmus(NN, 0.);

    for (int i = 0; i < NN; ++i) {
      xVector[i] = 0.;
      xVector[NN + i] = chgs[i];
    }

    solVector = decomp.solve(xVector);

    for (int i = 0; i < NN; ++i) {
      dni[i] = solVector[i];
      dmus[i] = solVector[NN + i];
    }

    for (int i = 0; i < NN; ++i)
      ret[1] += chgs[i] * dni[i];

    ret[1] /= pow(m_Parameters.T, 2) * pow(xMath::GeVtoifm(), 3);

    if (order < 3) return ret;
    // chi3
    vector<double> d2ni(NN, 0.), d2mus(NN, 0.);

    vector<double> chi3id(m_densities.size());
    for (int i = 0; i < NN; ++i)
      chi3id[i] = m_TPS->Particles()[i].chi(3, m_Parameters, m_UseWidth, MuStar[i], 0.);

    for (int i = 0; i < NN; ++i) {
      xVector[i] = 0.;

      double tmp = 0.;
      for (int j = 0; j < NN; ++j) tmp += m_Virial[j][i] * dni[j];
      tmp = -2. * tmp * chi2id[i] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * m_Parameters.T * dmus[i];
      xVector[i] += tmp;

      tmp = 0.;
      for (int j = 0; j < NN; ++j) tmp += m_Virial[j][i] * m_densities[j];
      tmp = -(tmp - 1.) * chi3id[i] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * dmus[i] * dmus[i];
      xVector[i] += tmp;
    }
    for (int i = 0; i < NN; ++i) {
      xVector[NN + i] = 0.;

      double tmp = 0.;
      for (int j = 0; j < NN; ++j) tmp += -m_Virial[i][j] * dmus[j] * chi2id[j] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * m_Parameters.T * dmus[j];

      xVector[NN + i] = tmp;
    }

    solVector = decomp.solve(xVector);

    for (int i = 0; i < NN; ++i) {
      d2ni[i] = solVector[i];
      d2mus[i] = solVector[NN + i];
    }

    for (int i = 0; i < NN; ++i)
      ret[2] += chgs[i] * d2ni[i];

    ret[2] /= m_Parameters.T * pow(xMath::GeVtoifm(), 3);


    if (order < 4) return ret;

    // chi4
    vector<double> d3ni(NN, 0.), d3mus(NN, 0.);

    vector<double> chi4id(m_densities.size());
    for (int i = 0; i < NN; ++i)
      chi4id[i] = m_TPS->Particles()[i].chi(4, m_Parameters, m_UseWidth, MuStar[i], 0.);

    vector<double> dnis(NN, 0.);
    for (int i = 0; i < NN; ++i) {
      dnis[i] = chi2id[i] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * m_Parameters.T * dmus[i];
    }

    vector<double> d2nis(NN, 0.);
    for (int i = 0; i < NN; ++i) {
      d2nis[i] = chi3id[i] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * dmus[i] * dmus[i] +
        chi2id[i] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * m_Parameters.T * d2mus[i];
    }

    for (int i = 0; i < NN; ++i) {
      xVector[i] = 0.;

      double tmp = 0.;
      for (int j = 0; j < NN; ++j) tmp += m_Virial[j][i] * dni[j];
      tmp = -3. * tmp * d2nis[i];
      xVector[i] += tmp;

      tmp = 0.;
      for (int j = 0; j < NN; ++j) tmp += m_Virial[j][i] * d2ni[j];
      tmp = -3. * tmp * dnis[i];
      xVector[i] += tmp;

      double tmps = 0.;
      for (int j = 0; j < NN; ++j) tmps += m_Virial[j][i] * m_densities[j];

      tmp = -(tmps - 1.) * chi3id[i] * pow(xMath::GeVtoifm(), 3) * m_Parameters.T * d2mus[i] * 3. * dmus[i];
      xVector[i] += tmp;

      tmp = -(tmps - 1.) * chi4id[i] * pow(xMath::GeVtoifm(), 3) * dmus[i] * dmus[i] * dmus[i];
      xVector[i] += tmp;
    }
    for (int i = 0; i < NN; ++i) {
      xVector[NN + i] = 0.;

      double tmp = 0.;
      for (int j = 0; j < NN; ++j) tmp += -2. * m_Virial[i][j] * d2mus[j] * dnis[j];
      xVector[NN + i] += tmp;

      tmp = 0.;
      for (int j = 0; j < NN; ++j) tmp += -m_Virial[i][j] * dmus[j] * d2nis[j];
      xVector[NN + i] += tmp;
    }

    solVector = decomp.solve(xVector);

    for (int i = 0; i < NN; ++i) {
      d3ni[i] = solVector[i];
      d3mus[i] = solVector[NN + i];
    }

    for (int i = 0; i < NN; ++i)
      ret[3] += chgs[i] * d3ni[i];

    ret[3] /= pow(xMath::GeVtoifm(), 3);

    return ret;
  }

  double ThermalModelEVCrossterms::CalculateEnergyDensity() {
    double ret = 0.;
    ret += m_Parameters.T * CalculateEntropyDensity();
    ret += -CalculatePressure();
    for (int i = 0; i < m_TPS->Particles().size(); ++i)
      ret += m_Chem[i] * m_densities[i];
    return ret;
  }

  double ThermalModelEVCrossterms::CalculateEntropyDensity() {
    if (!m_Calculated) CalculateDensities();
    return m_TotalEntropyDensity;
  }

  double ThermalModelEVCrossterms::CalculatePressure() {
    if (!m_Calculated) CalculateDensities();
    return m_Pressure;
  }

  double ThermalModelEVCrossterms::CalculateBaryonScaledVariance(bool susc) {
    return 1.;
  }

  double ThermalModelEVCrossterms::CalculateChargeScaledVariance(bool susc) {
    return 1.;
  }

  double ThermalModelEVCrossterms::CalculateStrangenessScaledVariance(bool susc) {
    return 1.;
  }

  double ThermalModelEVCrossterms::MuShift(int id)
  {
    if (id >= 0. && id < m_Virial.size()) {
      double dMu = 0.;
      for (int j = 0; j < m_TPS->Particles().size(); ++j)
        dMu += -m_Virial[id][j] * m_Ps[j];
      return dMu;
    }
    else
      return 0.0;
  }

  std::vector<double> ThermalModelEVCrossterms::BroydenEquationsCRS::Equations(const std::vector<double>& x)
  {
    std::vector<double> ret(m_N);
    for (int i = 0; i < x.size(); ++i)
      ret[i] = x[i] - m_THM->Pressure(i, x);
    return ret;
  }

  Eigen::MatrixXd ThermalModelEVCrossterms::BroydenJacobianCRS::Jacobian(const std::vector<double>& x)
  {
    int N = x.size();

    vector<double> tN(N);
    for (int i = 0; i < N; ++i) {
      tN[i] = m_THM->DensityId(i, x);
    }

    MatrixXd Jac(N, N), Jinv(N, N);
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < N; ++j) {
        Jac(i, j) = 0.;
        if (i == j) Jac(i, j) += 1.;
        Jac(i, j) += m_THM->VirialCoefficient(i, j) * tN[i];
      }
    }

    return Jac;
  }

  bool ThermalModelEVCrossterms::BroydenSolutionCriteriumCRS::IsSolved(const std::vector<double>& x, const std::vector<double>& f, const std::vector<double>& xdelta) const
  {
    double maxdiff = 0.;
    for (int i = 0; i < x.size(); ++i) {
      maxdiff = std::max(maxdiff, fabs(f[i]) / x[i]);
    }
    return (maxdiff < m_RelativeError);
  }

  std::vector<double> ThermalModelEVCrossterms::BroydenEquationsCRSDEV::Equations(const std::vector<double>& x)
  {
    std::vector<double> ret(1);
    ret[0] = x[0] - m_THM->PressureDiagonalTotal(x[0]);
    return ret;
  }
} // namespace thermalfist