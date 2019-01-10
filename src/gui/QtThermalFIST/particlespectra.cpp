/*
 * Thermal-FIST package
 * 
 * Copyright (c) 2014-2018 Volodymyr Vovchenko
 *
 * GNU General Public License (GPLv3 or later)
 */
#include "particlespectra.h"

#include <QDebug>
#include <QThread>

#include "HRGBase/ThermalModelBase.h"
#include "HRGEventGenerator/MomentumDistribution.h"

using namespace thermalfist;

void ParticleSpectrum::AddParticle(const SimpleParticle &part) {
    tmpn++;
    dndp.insert(sqrt(part.p0*part.p0 - part.m*part.m));
    dndy.insert(part.GetY());
    dndmt.insert(part.GetMt());
    d2ndptdy.insert(part.GetY(), part.GetPt());
}

void ParticleSpectrum::FinishEvent(double weight) {
    double tmn = static_cast<double>(tmpn);
    n  += weight*tmn;
    n2 += weight*tmn*tmn;
    n3 += weight*tmn*tmn*tmn;
    n4 += weight*tmn*tmn*tmn*tmn;
    n5 += weight*tmn*tmn*tmn*tmn*tmn;
    n6 += weight*tmn*tmn*tmn*tmn*tmn*tmn;
    n7 += weight*tmn*tmn*tmn*tmn*tmn*tmn*tmn;
    n8 += weight*tmn*tmn*tmn*tmn*tmn*tmn*tmn*tmn;
    wsum  += weight;
    w2sum += weight*weight;
    tmpn = 0;
    dndp.updateEvent(weight);
    dndy.updateEvent(weight);
    dndmt.updateEvent(weight);
    d2ndptdy.updateEvent(weight);
    events++;
    isAveragesCalculated = false;
}

void ParticleSpectrum::CalculateAverages() {
    means.resize(9);
    means[0] = 1.;
    means[1] = n  / wsum;
    means[2] = n2 / wsum;
    means[3] = n3 / wsum;
    means[4] = n4 / wsum;
    means[5] = n5 / wsum;
    means[6] = n6 / wsum;
    means[7] = n7 / wsum;
    means[8] = n8 / wsum;
    nE = events * wsum * wsum / w2sum / events;
    //qDebug() << nE << " " << events;
    isAveragesCalculated = true;
}

double ParticleSpectrum::GetMean() const {
    return means[1];
}

double ParticleSpectrum::GetMeanError() const {
    return sqrt((means[2]-means[1]*means[1])/(nE-1.));
}

double ParticleSpectrum::GetVariance() const {
    return (means[2]-means[1]*means[1]);
}

double ParticleSpectrum::GetStdDev() const {
    return sqrt(means[2]-means[1]*means[1]);
}

double ParticleSpectrum::GetScaledVariance() const {
    return GetVariance() / GetMean();
}

double ParticleSpectrum::GetSkewness() const {
    return (means[3]-3.*means[2]*means[1] + 2. * means[1] * means[1] * means[1]) / GetVariance();
}

double ParticleSpectrum::GetSkewnessError() const {
    double nav  = means[1];
    double n2av = means[2];
    double n3av = means[3];
    double n4av = means[4];
    double n5av = means[5];
    double n6av = means[6];
		double dm32 = (n6av - 6.*n5av*nav + 15.*n4av*nav*nav - 20.*n3av*nav*nav*nav + 15.*n2av*nav*nav*nav*nav
                   -9.*nav*nav*nav*nav*nav*nav + 12.*nav*nav*nav*nav*n2av - 9.*nav*nav*n2av*n2av
                   -4.*nav*nav*nav*n3av + 6.*nav*n2av*n3av - n3av*n3av) / nE;

    return sqrt(dm32) / GetVariance();
}

double ParticleSpectrum::GetKurtosis() const {
    return ( means[4] - 4. * means[3] * means[1] + 6. * means[2]*means[1]*means[1] - 3. * means[1]*means[1]*means[1]*means[1] )/GetVariance() - 3. * GetVariance();
}

double ParticleSpectrum::GetKurtosisError() const {
    double nav  = means[1];
    double n2av = means[2];
    double n3av = means[3];
    double n4av = means[4];
    double n5av = means[5];
    double n6av = means[6];
    double n7av = means[7];
    double n8av = means[8];
    double dm42 = (n8av - 8.*n7av*nav + 28.*n6av*nav*nav - 56.*n5av*nav*nav*nav + 70.*n4av*nav*nav*nav*nav
                   -56.*n3av*nav*nav*nav*nav*nav + 28.*n2av*nav*nav*nav*nav*nav*nav - 16.*nav*nav*nav*nav*nav*nav*nav*nav
                   +36.*n2av*nav*nav*nav*nav*nav*nav - 36.*n2av*n2av*nav*nav*nav*nav - 24.*n3av*nav*nav*nav*nav*nav
                   +48.*nav*nav*nav*n2av*n3av - 16.*nav*nav*n3av*n3av + 6.*nav*nav*nav*nav*n4av
                   -12.*nav*nav*n2av*n4av + 8.*nav*n3av*n4av - n4av*n4av) / nE;
    double tmp = 3. + (GetKurtosis()+3.*GetVariance())/GetVariance();
    return sqrt(dm42) / GetVariance();
}

double ParticleSpectrum::GetN2Error2() const {
    return 1. / (nE-1.) * (means[4] - means[2]*means[2]);
}

double ParticleSpectrum::GetVarianceError() const {
    double nav  = means[1];
    double n2av = means[2];
    double n3av = means[3];
    double n4av = means[4];
    double tmp = n4av-4.*n3av*nav+8.*n2av*nav*nav-4.*nav*nav*nav*nav-n2av*n2av;

    return sqrt(n4av-4.*n3av*nav+8.*n2av*nav*nav-4.*nav*nav*nav*nav-n2av*n2av)/sqrt(nE);
}

double ParticleSpectrum::GetScaledVarianceError() const {
    return GetVarianceError() / GetMean();
}

ParticlesSpectra::ParticlesSpectra(ThermalModelBase *model, double T, double beta, int distrtype, double etamax) {
    fEtaMax = etamax;
    fDistributionType = distrtype;
    fPDGtoID.clear();
    fPDGtoIDnet.clear();
    fPDGtoIDall.clear();
    fNetParticles.resize(0);
    fNetCharges.resize(0);
    fTotalCharges.resize(0);
    fPositiveCharges.resize(0);
    fNegativeCharges.resize(0);
    fParticleCharges.resize(0);
    if (model!=NULL) {
        for(int i=0;i<model->TPS()->Particles().size();++i) {
            if (model->TPS()->Particles()[i].IsStable()) {
                if (distrtype==0) fParticles.push_back(ParticleSpectrum(model->TPS()->Particles()[i].PdgId(), model->TPS()->Particles()[i].Mass()));
                else fParticles.push_back(ParticleSpectrum(model->TPS()->Particles()[i].PdgId(), model->TPS()->Particles()[i].Mass(), etamax));
                MomentumDistributionBase *ptr;
                if (distrtype==0) ptr = new SiemensRasmussenDistribution(model->TPS()->Particles()[i].PdgId(),model->TPS()->Particles()[i].Mass(),T,beta);
                else ptr = new SSHDistribution(model->TPS()->Particles()[i].PdgId(), model->TPS()->Particles()[i].Mass(), T, beta, etamax, false);
								distrs.push_back(ptr);
								fParticles[fParticles.size()-1].SetDistribution(ptr);
								fNames.push_back(model->TPS()->Particles()[i].Name());
                fMasses.push_back(model->TPS()->Particles()[i].Mass());
                fPDGtoID[model->TPS()->Particles()[i].PdgId()] = fMasses.size()-1;

                if (model->TPS()->Particles()[i].PdgId()>0 && model->TPS()->PdgToId(-model->TPS()->Particles()[i].PdgId())!=-1) {
                    fNetParticles.push_back(NumberStatistics("net-" + model->TPS()->Particles()[i].Name()));
                    fPDGtoIDnet[model->TPS()->Particles()[i].PdgId()] = fNetParticles.size()-1;
                }
            }
            fPDGtoIDall[model->TPS()->Particles()[i].PdgId()] = i;

            std::vector<int> tchr(4,0);
            tchr[0] = model->TPS()->Particles()[i].BaryonCharge();
            tchr[1] = model->TPS()->Particles()[i].ElectricCharge();
            tchr[2] = model->TPS()->Particles()[i].Strangeness();
            tchr[3] = model->TPS()->Particles()[i].Charm();
            fParticleCharges.push_back(tchr);
        }

        fNetCharges.push_back(NumberStatistics("net-baryon"));
        fNetCharges.push_back(NumberStatistics("net-charge"));
        fNetCharges.push_back(NumberStatistics("net-strangeness"));
        fNetCharges.push_back(NumberStatistics("net-charm"));
        fTotalCharges.push_back(NumberStatistics("baryonic hadrons"));
        fTotalCharges.push_back(NumberStatistics("charged hadrons"));
        fTotalCharges.push_back(NumberStatistics("strange hadrons"));
        fTotalCharges.push_back(NumberStatistics("charmed hadrons"));
        fPositiveCharges.push_back(NumberStatistics("baryon+ hadrons"));
        fPositiveCharges.push_back(NumberStatistics("charge+ hadrons"));
        fPositiveCharges.push_back(NumberStatistics("strange+ hadrons"));
        fPositiveCharges.push_back(NumberStatistics("charm+ hadrons"));
        fNegativeCharges.push_back(NumberStatistics("baryon- hadrons"));
        fNegativeCharges.push_back(NumberStatistics("charge- hadrons"));
        fNegativeCharges.push_back(NumberStatistics("strange- hadrons"));
        fNegativeCharges.push_back(NumberStatistics("charm- hadrons"));
    }
    else {
        fParticles.resize(0);
        fNames.resize(0);
        fMasses.resize(0);
        fNetParticles.resize(0);
        fNetCharges.resize(0);
        fTotalCharges.resize(0);
        fPositiveCharges.resize(0);
        fNegativeCharges.resize(0);
        fParticleCharges.resize(0);
        for(int i=0;i<distrs.size();++i)
            delete distrs[i];
        distrs.resize(0);
    }
}

ParticlesSpectra::~ParticlesSpectra() {
    for(int i=0;i<distrs.size();++i)
        delete distrs[i];
}

void ParticlesSpectra::ProcessEvent(const SimpleEvent &evt) {
    std::vector<int> netparts(fNetParticles.size(), 0);
    std::vector<int> netcharges(4, 0);
    std::vector<int> totalcharges(4, 0);
    std::vector<int> positivecharges(4, 0);
    std::vector<int> negativecharges(4, 0);

    for(int i=0;i<evt.Particles.size();++i) {
        {
            if (fPDGtoID.count(evt.Particles[i].PDGID)!=0)
                fParticles[fPDGtoID[evt.Particles[i].PDGID]].AddParticle(evt.Particles[i]);

            if (fPDGtoIDnet.count(evt.Particles[i].PDGID)!=0)
                netparts[fPDGtoIDnet[evt.Particles[i].PDGID]]++;
            if (fPDGtoIDnet.count(-evt.Particles[i].PDGID)!=0)
                netparts[fPDGtoIDnet[-evt.Particles[i].PDGID]]--;

            if (fPDGtoIDall.count(evt.Particles[i].PDGID)!=0) {
                for(int ii=0;ii<4;++ii) {
                    netcharges[ii] += fParticleCharges[fPDGtoIDall[evt.Particles[i].PDGID]][ii];
                    if (fParticleCharges[fPDGtoIDall[evt.Particles[i].PDGID]][ii]>0)
                        positivecharges[ii]++;
                    else if (fParticleCharges[fPDGtoIDall[evt.Particles[i].PDGID]][ii]<0)
                        negativecharges[ii]++;
                    if (fParticleCharges[fPDGtoIDall[evt.Particles[i].PDGID]][ii]!=0)
                        totalcharges[ii]++;
                }
            }
        }
    }
    for(int i=0;i<fParticles.size();++i)
        fParticles[i].FinishEvent(evt.weight);

    for(int i=0;i<netparts.size();++i)
        fNetParticles[i].AddEvent(netparts[i], evt.weight);
    for(int i=0;i<netcharges.size();++i) {
        fNetCharges[i].AddEvent(netcharges[i], evt.weight);
        fTotalCharges[i].AddEvent(totalcharges[i], evt.weight);
        fPositiveCharges[i].AddEvent(positivecharges[i], evt.weight);
        fNegativeCharges[i].AddEvent(negativecharges[i], evt.weight);
    }

}

void ParticlesSpectra::Reset() {
    for(int i=0;i<fParticles.size();++i)
        fParticles[i].Reset();
    for(int i=0;i<fNetParticles.size();++i)
        fNetParticles[i].Reset();
    for(int i=0;i<fNetCharges.size();++i) {
        fNetCharges[i].Reset();
        fTotalCharges[i].Reset();
        fPositiveCharges[i].Reset();
        fNegativeCharges[i].Reset();
    }
}

void ParticlesSpectra::Reset(ThermalModelBase *model, double T, double beta, int distrtype, double etamax, double npow) {
    fEtaMax = etamax;
    fDistributionType = distrtype;
    fPDGtoID.clear();
    fPDGtoIDnet.clear();
    fPDGtoIDall.clear();
    fParticles.resize(0);
    fNetParticles.resize(0);
    fNetCharges.resize(0);
    fTotalCharges.resize(0);
    fPositiveCharges.resize(0);
    fNegativeCharges.resize(0);
    fParticleCharges.resize(0);
    fNames.resize(0);
    fMasses.resize(0);
    for(int i=0;i<distrs.size();++i)
        delete distrs[i];
    distrs.resize(0);
    if (model!=NULL) {
        for(int i=0;i<model->TPS()->Particles().size();++i) {
            if (model->TPS()->Particles()[i].IsStable()) {
                if (distrtype==0) fParticles.push_back(ParticleSpectrum(model->TPS()->Particles()[i].PdgId(), model->TPS()->Particles()[i].Mass()));
                else fParticles.push_back(ParticleSpectrum(model->TPS()->Particles()[i].PdgId(), model->TPS()->Particles()[i].Mass(), etamax));
								MomentumDistributionBase *ptr;
                if (distrtype==0) ptr = new SiemensRasmussenDistribution(model->TPS()->Particles()[i].PdgId(),model->TPS()->Particles()[i].Mass(),T,beta);
                else ptr = new SSHDistribution(model->TPS()->Particles()[i].PdgId(), model->TPS()->Particles()[i].Mass(), T, beta, etamax, npow, false);
                distrs.push_back(ptr);
                fParticles[fParticles.size()-1].SetDistribution(ptr);
								fNames.push_back(model->TPS()->Particles()[i].Name());
                fMasses.push_back(model->TPS()->Particles()[i].Mass());
                fPDGtoID[model->TPS()->Particles()[i].PdgId()] = fMasses.size()-1;

                if (model->TPS()->Particles()[i].PdgId()>0 && model->TPS()->PdgToId(-model->TPS()->Particles()[i].PdgId())!=-1) {
                    fNetParticles.push_back(NumberStatistics("net-" + model->TPS()->Particles()[i].Name()));
                    fPDGtoIDnet[model->TPS()->Particles()[i].PdgId()] = fNetParticles.size()-1;
                }
            }

            fPDGtoIDall[model->TPS()->Particles()[i].PdgId()] = i;

            std::vector<int> tchr(4,0);
            tchr[0] = model->TPS()->Particles()[i].BaryonCharge();
            tchr[1] = model->TPS()->Particles()[i].ElectricCharge();
            tchr[2] = model->TPS()->Particles()[i].Strangeness();
            tchr[3] = model->TPS()->Particles()[i].Charm();
            fParticleCharges.push_back(tchr);
        }
        fNetCharges.push_back(NumberStatistics("net-baryon"));
        fNetCharges.push_back(NumberStatistics("net-charge"));
        fNetCharges.push_back(NumberStatistics("net-strangeness"));
        fNetCharges.push_back(NumberStatistics("net-charm"));
        fTotalCharges.push_back(NumberStatistics("baryonic hadrons"));
        fTotalCharges.push_back(NumberStatistics("charged hadrons"));
        fTotalCharges.push_back(NumberStatistics("strange hadrons"));
        fTotalCharges.push_back(NumberStatistics("charmed hadrons"));
        fPositiveCharges.push_back(NumberStatistics("baryon+ hadrons"));
        fPositiveCharges.push_back(NumberStatistics("charge+ hadrons"));
        fPositiveCharges.push_back(NumberStatistics("strange+ hadrons"));
        fPositiveCharges.push_back(NumberStatistics("charm+ hadrons"));
        fNegativeCharges.push_back(NumberStatistics("baryon- hadrons"));
        fNegativeCharges.push_back(NumberStatistics("charge- hadrons"));
        fNegativeCharges.push_back(NumberStatistics("strange- hadrons"));
        fNegativeCharges.push_back(NumberStatistics("charm- hadrons"));
    }
}
