#include <TCanvas.h>
#include <TChain.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLeaf.h>
#include <TLegend.h>
#include <TLine.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
double value(TTree* tree, const char* name, int index) {
  auto* leaf = tree->GetLeaf(name);
  return leaf ? leaf->GetValue(index) : 0.0;
}

int count(TTree* tree, const char* name) {
  auto* leaf = tree->GetLeaf(name);
  return leaf ? leaf->GetNdata() : 0;
}

struct EventFeature {
  long long event = 0;
  int job = -1;
  int split = 0;
  double trueEnergy = 0;
  double primaryX = std::numeric_limits<double>::quiet_NaN();
  double primaryY = std::numeric_limits<double>::quiet_NaN();
  double primaryZ = std::numeric_limits<double>::quiet_NaN();
  double primaryPx = std::numeric_limits<double>::quiet_NaN();
  double primaryPy = std::numeric_limits<double>::quiet_NaN();
  double primaryPz = std::numeric_limits<double>::quiet_NaN();
  int converted = 0;
  int unconvertedFinal = 0;
  double convX = std::numeric_limits<double>::quiet_NaN();
  double convY = std::numeric_limits<double>::quiet_NaN();
  double convZ = std::numeric_limits<double>::quiet_NaN();
  double caloEdep = 0;
  int nCells = 0;
  int nCells1MeV = 0;
  int nCells20MeV = 0;
  double centroidIx = std::numeric_limits<double>::quiet_NaN();
  double centroidIy = std::numeric_limits<double>::quiet_NaN();
  double centroidIz = std::numeric_limits<double>::quiet_NaN();
  double centroidXcm = std::numeric_limits<double>::quiet_NaN();
  double centroidYcm = std::numeric_limits<double>::quiet_NaN();
  double centroidZcm = std::numeric_limits<double>::quiet_NaN();
  double truthResidualCm = std::numeric_limits<double>::quiet_NaN();
  double transverseRms = std::numeric_limits<double>::quiet_NaN();
  double longitudinalRms = std::numeric_limits<double>::quiet_NaN();
  double maxCellFraction = 0;
  double pairEnergyShare = std::numeric_limits<double>::quiet_NaN();
  double pairOpeningDeg = std::numeric_limits<double>::quiet_NaN();
  double pairProjectedSeparationCm =
      std::numeric_limits<double>::quiet_NaN();
  double boundaryDistanceCells = std::numeric_limits<double>::quiet_NaN();
  double lastLayerFraction = 0;
  int nStkHits = 0;
  double stkEdep = 0;
  std::vector<double> layerEdep = std::vector<double>(21, 0.0);
};

std::set<int> parseJobs(const char* text) {
  std::set<int> jobs;
  std::stringstream stream(text ? text : "");
  std::string token;
  while (std::getline(stream, token, ','))
    if (!token.empty()) jobs.insert(std::stoi(token));
  return jobs;
}

int jobFromFilename(const std::string& filename) {
  std::smatch match;
  if (std::regex_search(filename, match, std::regex("job([0-9]{6})")))
    return std::stoi(match[1]);
  return -1;
}

void saveCanvas(TCanvas& canvas, const std::string& outputDir,
                const std::string& name) {
  canvas.SaveAs((outputDir + "/" + name + ".png").c_str());
  canvas.SaveAs((outputDir + "/" + name + ".pdf").c_str());
}

void annotate(long long total, long long selected,
              const char* selection = nullptr) {
  TLatex label;
  label.SetNDC(true);
  // The canvas reserves a top margin for this metadata, keeping it out of
  // the histogram/heat-map data area.
  label.SetTextSize(0.028);
  label.SetTextAlign(11);
  label.DrawLatex(0.13, 0.925, Form("N_{generated} = %lld", total));
  label.DrawLatex(0.48, 0.925, Form("N_{selected} = %lld", selected));
  if (selection && selection[0])
    label.DrawLatex(0.13, 0.875, selection);
}

std::vector<double> logEdges(double minimum, double maximum, int bins) {
  if (minimum <= 0 || maximum <= minimum) {
    minimum = std::max(1e-6, minimum * 0.9);
    maximum = std::max(minimum * 1.01, maximum * 1.1);
  }
  std::vector<double> edges(bins + 1);
  const double logMin = std::log(minimum);
  const double step = (std::log(maximum) - logMin) / bins;
  for (int i = 0; i <= bins; ++i)
    edges[i] = std::exp(logMin + i * step);
  return edges;
}

// v2025a CALO placement from calogeo_sphere_v3.cc and v2025a_cfg.cfgxml.
// ix/iy are 0..22, iz is 0..20; coordinates are cell centers in global cm.
double caloXcm(double ix) {
  return (ix - 11.0) * 3.8;
}

double caloYcm(double iy) {
  const double cy = iy - 11.0;
  const double gapShift = std::abs(cy) >= 4.0 ? std::copysign(1.65, cy) : 0.0;
  return cy * 3.3 + gapShift;
}

double caloZcm(double iz) {
  return -36.3 + (iz - 10.0) * 3.3;
}
}

void analyze_gamma_calo_ml(
    const char* inputPattern,
    const char* outputDirectory,
    int expectedFiles = 0,
    long long expectedEntries = -1,
    double energyMinGeV = 1.0,
    double energyMaxGeV = 1.0,
    const char* trainJobsText = "0,1",
    const char* validationJobsText = "2",
    const char* testJobsText = "3",
    int caloIndexMin = 0,
    int caloIndexMax = 22,
    int caloLayerMin = 0,
    int caloLayerMax = 20,
    int caloLastLayer = 0) {
  gStyle->SetOptStat(0);
  gStyle->SetPalette(kViridis);
  gSystem->mkdir(outputDirectory, true);
  const std::string outDir(outputDirectory);

  TChain events("events");
  const int filesAdded = events.Add(inputPattern);
  if (filesAdded <= 0 || (expectedFiles > 0 && filesAdded != expectedFiles)) {
    std::cerr << "ERROR: expected " << expectedFiles
              << " ROOT files, added " << filesAdded << "\n";
    return;
  }
  const Long64_t entries = events.GetEntries();
  if (expectedEntries >= 0 && entries != expectedEntries) {
    std::cerr << "ERROR: expected " << expectedEntries
              << " events, found " << entries << "\n";
    return;
  }
  const auto trainJobs = parseJobs(trainJobsText);
  const auto validationJobs = parseJobs(validationJobsText);
  const auto testJobs = parseJobs(testJobsText);
  const double plotEnergyMax = std::max(1.2 * energyMaxGeV, 0.1);
  const auto energyEdges = logEdges(
      energyMinGeV == energyMaxGeV ? energyMinGeV * 0.9 : energyMinGeV,
      energyMinGeV == energyMaxGeV ? energyMaxGeV * 1.1 : energyMaxGeV,
      energyMinGeV == energyMaxGeV ? 20 : 30);

  TH1D hCaloEdepAll("hCaloEdepAll",
      "CALO deposited energy;E_{dep} [GeV];Events", 240, 0, plotEnergyMax);
  TH1D hCaloEdepConverted("hCaloEdepConverted",
      "CALO deposited energy;E_{dep} [GeV];Events", 240, 0, plotEnergyMax);
  TH1D hCaloEdepUnconverted("hCaloEdepUnconverted",
      "CALO deposited energy;E_{dep} [GeV];Events", 200, 0, 0.02);
  TH1D hResponse("hResponse",
      "Raw CALO response for converted events;E_{dep}/E_{true};Events",
      240, 0, 1.2);
  TH1D hNCells("hNCells",
      "CALO hit-cell multiplicity;N cells with E_{dep}>0;Events",
      220, 0, 220);
  TH1D hNCells1MeV("hNCells1MeV",
      "CALO cell multiplicity above 1 MeV;N cells;Events", 160, 0, 160);
  TH1D hMaxCellFraction("hMaxCellFraction",
      "Leading-cell energy fraction;max(E_{cell})/E_{dep};Events",
      100, 0, 1);
  TH1D hCentroidZ("hCentroidZ",
      "Energy-weighted shower centroid;iz centroid [layer index];Events",
      84, -0.5, 20.5);
  TH1D hTransverseRms("hTransverseRms",
      "Transverse shower width;#sqrt{#sigma_{ix}^{2}+#sigma_{iy}^{2}} [cell];Events",
      100, 0, 8);
  TH1D hLongitudinalRms("hLongitudinalRms",
      "Longitudinal shower width;#sigma_{iz} [layer];Events",
      100, 0, 10);
  TH1D hConversionZ("hConversionZ",
      "First gamma-conversion vertex;z_{conv} [cm];Events",
      180, -100, 80);
  TH1D hPairEnergyShare("hPairEnergyShare",
      "Pair energy sharing;min(E_{e-},E_{e+})/(E_{e-}+E_{e+});Events",
      100, 0, 0.5);
  TH1D hPairOpening("hPairOpening",
      "e^{-}e^{+} opening-angle core;Opening angle [deg];Events",
      200, 0, 20);
  TH2D hEdepVsConversionZ("hEdepVsConversionZ",
      "CALO response versus conversion depth;z_{conv} [cm];E_{dep} [GeV]",
      180, -100, 80, 160, 0, plotEnergyMax);
  TH2D hCentroidXY("hCentroidXY",
      "Energy-weighted CALO shower centroid;ix;iy",
      92, -0.5, 22.5, 92, -0.5, 22.5);
  TH2D hCentroidPhysicalXY("hCentroidPhysicalXY",
      "Energy-weighted CALO shower centroid;x centroid [cm];y centroid [cm]",
      120, -50, 50, 120, -50, 50);
  TH2D hConversionXY("hConversionXY",
      "First gamma-conversion position;x_{conv} [cm];y_{conv} [cm]",
      120, -60, 60, 120, -60, 60);
  TH1D hEnergyByIz("hEnergyByIz",
      "Mean deposited-energy profile versus CALO iz;iz;Mean E per event [GeV]",
      21, -0.5, 20.5);
  TH2D hCentroidZVsConversionZ("hCentroidZVsConversionZ",
      "Shower depth versus first conversion;z_{conv} [cm];"
      "CALO centroid z [cm]", 180, -100, 80, 140, -75, -2);
  TH1D hTruthResidual("hTruthResidual",
      "CALO centroid distance from generated gamma line;"
      "#DeltaR_{centroid,truth} [cm];Events", 120, 0, 30);
  TH2D hOpeningVsTransverseWidth("hOpeningVsTransverseWidth",
      "CALO width versus pair opening angle;Opening angle [deg];"
      "Transverse RMS [cell]", 200, 0, 20, 100, 0, 8);
  const auto separationEdges = logEdges(1e-3, 200.0, 120);
  TH1D hPairProjectedSeparation("hPairProjectedSeparation",
      "Straight-line e^{-}e^{+} separation at CALO shower centroid;"
      "Projected separation [cm];Events",
      separationEdges.size() - 1, separationEdges.data());
  TH2D hProjectedSeparationVsWidth("hProjectedSeparationVsWidth",
      "CALO width versus projected pair separation;"
      "Projected e^{-}e^{+} separation [cm];Transverse RMS [cell]",
      separationEdges.size() - 1, separationEdges.data(), 100, 0, 8);
  TH1D hLastLayerFraction("hLastLayerFraction",
      "Last-layer energy fraction;E_{last}/E_{dep};Events", 100, 0, 1);
  TH1D hMaxCellFractionPlot("hMaxCellFractionPlot",
      "Leading-cell energy fraction;max(E_{cell})/E_{dep};Events",
      100, 0, 1);
  TH2D hResponseVsLastLayer("hResponseVsLastLayer",
      "Response versus longitudinal leakage proxy;"
      "E_{last}/E_{dep};E_{dep}/E_{true}", 100, 0, 1, 180, 0, 1.5);
  TH2D hResponseVsBoundary("hResponseVsBoundary",
      "Response versus lateral containment;"
      "Distance of centroid to CALO edge [cell];E_{dep}/E_{true}",
      125, -1, 11.5, 180, 0, 1.5);
  TH2D hResponseVsCentroidZ("hResponseVsCentroidZ",
      "Response versus shower depth;CALO centroid iz [layer index];"
      "E_{dep}/E_{true}", 84, -0.5, 20.5, 180, 0, 1.5);
  TH1D hTrueEnergy("hTrueEnergy",
      "Generated gamma energy;E_{true} [GeV];Events", 240,
      std::max(0.0, energyMinGeV), energyMaxGeV * 1.001);
  TH2D hEdepVsTrueEnergy("hEdepVsTrueEnergy",
      "CALO response;E_{true} [GeV];E_{dep} [GeV]", 160,
      std::max(0.0, energyMinGeV), energyMaxGeV * 1.001,
      160, 0, plotEnergyMax);
  TH1D hTrueEnergyLog("hTrueEnergyLog",
      "Generated gamma energy;E_{true} [GeV];Events",
      energyEdges.size() - 1, energyEdges.data());
  TH1D hConvertedTrueEnergy("hConvertedTrueEnergy",
      "Converted gamma energy;E_{true} [GeV];Events",
      energyEdges.size() - 1, energyEdges.data());
  TH1D hActiveTrueEnergy("hActiveTrueEnergy",
      "CALO-active gamma energy;E_{true} [GeV];Events",
      energyEdges.size() - 1, energyEdges.data());
  TH2D hResponseVsTrueEnergy("hResponseVsTrueEnergy",
      "CALO response versus energy;E_{true} [GeV];E_{dep}/E_{true}",
      energyEdges.size() - 1, energyEdges.data(), 180, 0, 1.5);

  std::vector<EventFeature> features;
  features.reserve(entries);
  long long convertedCount = 0;
  long long unconvertedCount = 0;
  long long zeroCaloCount = 0;
  long long convertedActiveCount = 0;
  long long pairTruthCount = 0;
  long long pairOpeningAbove20DegCount = 0;
  long long lastLayerNonzeroCount = 0;
  long long lastLayerAbove10PercentCount = 0;
  double calibrationEdepSum = 0;
  long long calibrationCount = 0;

  for (Long64_t globalEntry = 0; globalEntry < entries; ++globalEntry) {
    events.LoadTree(globalEntry);
    events.GetEntry(globalEntry);
    EventFeature feature;
    feature.event = globalEntry;
    feature.job = jobFromFilename(events.GetCurrentFile()->GetName());
    if (trainJobs.count(feature.job)) feature.split = 0;
    else if (validationJobs.count(feature.job)) feature.split = 1;
    else if (testJobs.count(feature.job)) feature.split = 2;
    else feature.split = -1;

    int primaryTrack = -1;
    std::vector<double> pairEnergy;
    std::vector<std::vector<double>> pairMomentum;
    auto* mcPdg = events.GetLeaf("mcparts.pdgID");
    auto* mcTrack = events.GetLeaf("mcparts.trackID");
    auto* mcParent = events.GetLeaf("mcparts.parentID");
    auto* mcStatus = events.GetLeaf("mcparts.simstat");
    auto* mcPx = events.GetLeaf("mcparts.momentum.x");
    auto* mcPy = events.GetLeaf("mcparts.momentum.y");
    auto* mcPz = events.GetLeaf("mcparts.momentum.z");
    auto* mcVx = events.GetLeaf("mcparts.vertex.x");
    auto* mcVy = events.GetLeaf("mcparts.vertex.y");
    auto* mcVz = events.GetLeaf("mcparts.vertex.z");
    const int nmc = mcPdg ? mcPdg->GetNdata() : 0;
    for (int i = 0; i < nmc; ++i) {
      const int pdg = static_cast<int>(mcPdg->GetValue(i));
      const int track = static_cast<int>(mcTrack->GetValue(i));
      const int parent = static_cast<int>(mcParent->GetValue(i));
      const unsigned status = static_cast<unsigned>(mcStatus->GetValue(i));
      const double px = mcPx->GetValue(i);
      const double py = mcPy->GetValue(i);
      const double pz = mcPz->GetValue(i);
      const double momentum = std::sqrt(px*px + py*py + pz*pz);
      if (pdg == 22 && (status & 1u)) {
        primaryTrack = track;
        feature.trueEnergy = momentum;
        feature.primaryX = mcVx->GetValue(i);
        feature.primaryY = mcVy->GetValue(i);
        feature.primaryZ = mcVz->GetValue(i);
        feature.primaryPx = px;
        feature.primaryPy = py;
        feature.primaryPz = pz;
      }
      if (pdg == 22 && (status & 4u))
        feature.unconvertedFinal = 1;
      if ((status & 2u) && parent == primaryTrack &&
          (pdg == 11 || pdg == -11)) {
        feature.converted = 1;
        if (std::isnan(feature.convZ)) {
          feature.convX = mcVx->GetValue(i);
          feature.convY = mcVy->GetValue(i);
          feature.convZ = mcVz->GetValue(i);
        }
        pairEnergy.push_back(momentum);
        pairMomentum.push_back({px, py, pz});
      }
    }

    if (pairEnergy.size() == 2) {
      ++pairTruthCount;
      feature.pairEnergyShare =
          std::min(pairEnergy[0], pairEnergy[1]) /
          (pairEnergy[0] + pairEnergy[1]);
      const auto& a = pairMomentum[0];
      const auto& b = pairMomentum[1];
      double cosine = (a[0]*b[0] + a[1]*b[1] + a[2]*b[2]) /
          (pairEnergy[0] * pairEnergy[1]);
      cosine = std::max(-1.0, std::min(1.0, cosine));
      feature.pairOpeningDeg = std::acos(cosine) * 180.0 / M_PI;
    }

    double weightedX = 0, weightedY = 0, weightedZ = 0;
    double weightedX2 = 0, weightedY2 = 0, weightedZ2 = 0;
    double weightedXcm = 0, weightedYcm = 0, weightedZcm = 0;
    double maxCellEnergy = 0;
    auto* caloEnergy = events.GetLeaf("calohits.edep");
    auto* caloIx = events.GetLeaf("calohits.ix");
    auto* caloIy = events.GetLeaf("calohits.iy");
    auto* caloIz = events.GetLeaf("calohits.iz");
    const int ncalo = caloEnergy ? caloEnergy->GetNdata() : 0;
    for (int i = 0; i < ncalo; ++i) {
      const double energy = caloEnergy->GetValue(i);
      if (energy <= 0) continue;
      const double x = caloIx->GetValue(i);
      const double y = caloIy->GetValue(i);
      const double z = caloIz->GetValue(i);
      const int iz = static_cast<int>(z);
      feature.caloEdep += energy;
      ++feature.nCells;
      if (energy > 0.001) ++feature.nCells1MeV;
      if (energy > 0.020) ++feature.nCells20MeV;
      weightedX += energy*x;
      weightedY += energy*y;
      weightedZ += energy*z;
      weightedX2 += energy*x*x;
      weightedY2 += energy*y*y;
      weightedZ2 += energy*z*z;
      weightedXcm += energy*caloXcm(x);
      weightedYcm += energy*caloYcm(y);
      weightedZcm += energy*caloZcm(z);
      maxCellEnergy = std::max(maxCellEnergy, energy);
      hEnergyByIz.Fill(iz, energy);
      if (iz >= 0 && iz < static_cast<int>(feature.layerEdep.size()))
        feature.layerEdep[iz] += energy;
    }
    if (feature.caloEdep > 0) {
      feature.centroidIx = weightedX / feature.caloEdep;
      feature.centroidIy = weightedY / feature.caloEdep;
      feature.centroidIz = weightedZ / feature.caloEdep;
      feature.centroidXcm = weightedXcm / feature.caloEdep;
      feature.centroidYcm = weightedYcm / feature.caloEdep;
      feature.centroidZcm = weightedZcm / feature.caloEdep;
      const double varX = std::max(
          0.0, weightedX2/feature.caloEdep -
          feature.centroidIx*feature.centroidIx);
      const double varY = std::max(
          0.0, weightedY2/feature.caloEdep -
          feature.centroidIy*feature.centroidIy);
      const double varZ = std::max(
          0.0, weightedZ2/feature.caloEdep -
          feature.centroidIz*feature.centroidIz);
      feature.transverseRms = std::sqrt(varX + varY);
      feature.longitudinalRms = std::sqrt(varZ);
      feature.maxCellFraction = maxCellEnergy / feature.caloEdep;
      feature.boundaryDistanceCells = std::min({
          feature.centroidIx - caloIndexMin,
          caloIndexMax - feature.centroidIx,
          feature.centroidIy - caloIndexMin,
          caloIndexMax - feature.centroidIy});
      if (caloLastLayer >= caloLayerMin &&
          caloLastLayer <= caloLayerMax &&
          caloLastLayer < static_cast<int>(feature.layerEdep.size()))
        feature.lastLayerFraction =
            feature.layerEdep[caloLastLayer] / feature.caloEdep;
      if (std::abs(feature.primaryPz) > 1e-12) {
        const double dz = feature.centroidZcm - feature.primaryZ;
        const double truthX =
            feature.primaryX + dz*feature.primaryPx/feature.primaryPz;
        const double truthY =
            feature.primaryY + dz*feature.primaryPy/feature.primaryPz;
        feature.truthResidualCm = std::hypot(
            feature.centroidXcm - truthX, feature.centroidYcm - truthY);
      }
      if (pairMomentum.size() == 2 &&
          std::abs(pairMomentum[0][2]) > 1e-12 &&
          std::abs(pairMomentum[1][2]) > 1e-12) {
        const double dz = feature.centroidZcm - feature.convZ;
        const double flight0 = dz / pairMomentum[0][2];
        const double flight1 = dz / pairMomentum[1][2];
        const double x0 = feature.convX +
            dz*pairMomentum[0][0]/pairMomentum[0][2];
        const double y0 = feature.convY +
            dz*pairMomentum[0][1]/pairMomentum[0][2];
        const double x1 = feature.convX +
            dz*pairMomentum[1][0]/pairMomentum[1][2];
        const double y1 = feature.convY +
            dz*pairMomentum[1][1]/pairMomentum[1][2];
        if (flight0 >= 0 && flight1 >= 0)
          feature.pairProjectedSeparationCm = std::hypot(x0 - x1, y0 - y1);
      }
    } else {
      ++zeroCaloCount;
    }

    auto* stkEnergy = events.GetLeaf("stkhits.edep");
    feature.nStkHits = stkEnergy ? stkEnergy->GetNdata() : 0;
    for (int i = 0; i < feature.nStkHits; ++i)
      feature.stkEdep += stkEnergy->GetValue(i);

    hTrueEnergy.Fill(feature.trueEnergy);
    hTrueEnergyLog.Fill(feature.trueEnergy);
    hCaloEdepAll.Fill(feature.caloEdep);
    hEdepVsTrueEnergy.Fill(feature.trueEnergy, feature.caloEdep);
    if (feature.trueEnergy > 0)
      hResponseVsTrueEnergy.Fill(
          feature.trueEnergy, feature.caloEdep / feature.trueEnergy);
    if (feature.converted) {
      ++convertedCount;
      hConvertedTrueEnergy.Fill(feature.trueEnergy);
      hCaloEdepConverted.Fill(feature.caloEdep);
      if (feature.trueEnergy > 0)
        hResponse.Fill(feature.caloEdep / feature.trueEnergy);
      hConversionZ.Fill(feature.convZ);
      hConversionXY.Fill(feature.convX, feature.convY);
      hEdepVsConversionZ.Fill(feature.convZ, feature.caloEdep);
      if (!std::isnan(feature.pairEnergyShare))
        hPairEnergyShare.Fill(feature.pairEnergyShare);
      if (!std::isnan(feature.pairOpeningDeg))
        hPairOpening.Fill(feature.pairOpeningDeg);
      if (!std::isnan(feature.pairOpeningDeg) &&
          feature.pairOpeningDeg > 20.0)
        ++pairOpeningAbove20DegCount;
      if (feature.caloEdep > 0) {
        ++convertedActiveCount;
        hCentroidZVsConversionZ.Fill(feature.convZ, feature.centroidZcm);
        if (!std::isnan(feature.pairOpeningDeg))
          hOpeningVsTransverseWidth.Fill(
              feature.pairOpeningDeg, feature.transverseRms);
        if (!std::isnan(feature.pairProjectedSeparationCm)) {
          hPairProjectedSeparation.Fill(feature.pairProjectedSeparationCm);
          hProjectedSeparationVsWidth.Fill(
              feature.pairProjectedSeparationCm, feature.transverseRms);
        }
      }
      if (feature.split == 0 && feature.caloEdep > 0) {
        calibrationEdepSum += feature.caloEdep;
        ++calibrationCount;
      }
    }
    if (feature.unconvertedFinal) {
      ++unconvertedCount;
      hCaloEdepUnconverted.Fill(feature.caloEdep);
    }
    if (feature.caloEdep > 0) {
      hActiveTrueEnergy.Fill(feature.trueEnergy);
      hNCells.Fill(feature.nCells);
      hNCells1MeV.Fill(feature.nCells1MeV);
      hMaxCellFraction.Fill(feature.maxCellFraction);
      hCentroidZ.Fill(feature.centroidIz);
      hTransverseRms.Fill(feature.transverseRms);
      hLongitudinalRms.Fill(feature.longitudinalRms);
      hCentroidXY.Fill(feature.centroidIx, feature.centroidIy);
      hCentroidPhysicalXY.Fill(feature.centroidXcm, feature.centroidYcm);
      if (!std::isnan(feature.truthResidualCm))
        hTruthResidual.Fill(feature.truthResidualCm);
      hLastLayerFraction.Fill(feature.lastLayerFraction);
      if (feature.lastLayerFraction > 0) ++lastLayerNonzeroCount;
      if (feature.lastLayerFraction > 0.1)
        ++lastLayerAbove10PercentCount;
      hMaxCellFractionPlot.Fill(feature.maxCellFraction);
      if (feature.trueEnergy > 0) {
        const double response = feature.caloEdep / feature.trueEnergy;
        hResponseVsLastLayer.Fill(feature.lastLayerFraction, response);
        hResponseVsBoundary.Fill(feature.boundaryDistanceCells, response);
        hResponseVsCentroidZ.Fill(feature.centroidIz, response);
      }
    }
    features.push_back(feature);
  }

  const double calibrationMean =
      calibrationCount ? calibrationEdepSum/calibrationCount : 0;
  const double calibrationScale =
      calibrationMean > 0 ? 1.0/calibrationMean : 0;
  TH1D hConversionEfficiency(hConvertedTrueEnergy);
  hConversionEfficiency.SetName("hConversionEfficiency");
  hConversionEfficiency.SetTitle(
      "First-pair conversion fraction;E_{true} [GeV];Fraction");
  hConversionEfficiency.Divide(
      &hConvertedTrueEnergy, &hTrueEnergyLog, 1.0, 1.0, "B");
  TH1D hActiveEfficiency(hActiveTrueEnergy);
  hActiveEfficiency.SetName("hActiveEfficiency");
  hActiveEfficiency.SetTitle(
      "CALO-active fraction;E_{true} [GeV];Fraction");
  hActiveEfficiency.Divide(
      &hActiveTrueEnergy, &hTrueEnergyLog, 1.0, 1.0, "B");
  TH1D hBaselineReco("hBaselineReco",
      "Mono-energy scale baseline on independent test;"
      "E_{reco}=E_{dep}E_{mono}/<E_{dep}>_{train} [GeV];Events",
      240, 0, std::max(1.8 * energyMaxGeV, 0.1));
  for (const auto& feature : features) {
    if (feature.split == 2 && feature.converted && feature.caloEdep > 0) {
      if (std::abs(energyMaxGeV - energyMinGeV) < 1e-12)
        hBaselineReco.Fill(feature.caloEdep * calibrationScale *
                           energyMinGeV);
    }
  }

  std::ofstream csv(outDir + "/event_features.csv");
  csv << "event,job_id,split,true_energy_GeV,converted,unconverted_final,"
         "conversion_x_cm,conversion_y_cm,conversion_z_cm,calo_edep_GeV,"
         "n_cells,n_cells_gt_1MeV,n_cells_gt_20MeV,centroid_ix,"
         "centroid_iy,centroid_iz,centroid_x_cm,centroid_y_cm,centroid_z_cm,"
         "truth_line_residual_cm,transverse_rms_cells,longitudinal_rms_layers,"
         "max_cell_fraction,boundary_distance_cells,last_layer_fraction,"
         "n_stk_hits,stk_edep_GeV,pair_energy_share,pair_opening_deg,"
         "pair_projected_separation_cm";
  for (int layer = 0; layer < 21; ++layer)
    csv << ",layer_edep_" << layer << "_GeV";
  csv << '\n';
  csv << std::setprecision(9);
  for (const auto& f : features) {
    csv << f.event << ',' << f.job << ',' << f.split << ','
        << f.trueEnergy << ','
        << f.converted << ',' << f.unconvertedFinal << ','
        << f.convX << ',' << f.convY << ',' << f.convZ << ','
        << f.caloEdep << ',' << f.nCells << ',' << f.nCells1MeV << ','
        << f.nCells20MeV << ',' << f.centroidIx << ',' << f.centroidIy << ','
        << f.centroidIz << ',' << f.centroidXcm << ',' << f.centroidYcm << ','
        << f.centroidZcm << ',' << f.truthResidualCm << ','
        << f.transverseRms << ','
        << f.longitudinalRms << ',' << f.maxCellFraction << ','
        << f.boundaryDistanceCells << ',' << f.lastLayerFraction << ','
        << f.nStkHits << ',' << f.stkEdep << ','
        << f.pairEnergyShare << ',' << f.pairOpeningDeg << ','
        << f.pairProjectedSeparationCm;
    for (double layerEnergy : f.layerEdep) csv << ',' << layerEnergy;
    csv << '\n';
  }

  hEnergyByIz.Scale(1.0 / entries);
  TFile output((outDir + "/analysis_histograms.root").c_str(), "RECREATE");
  hCaloEdepAll.Write();
  hCaloEdepConverted.Write();
  hCaloEdepUnconverted.Write();
  hResponse.Write();
  hNCells.Write();
  hNCells1MeV.Write();
  hMaxCellFraction.Write();
  hCentroidZ.Write();
  hTransverseRms.Write();
  hLongitudinalRms.Write();
  hConversionZ.Write();
  hPairEnergyShare.Write();
  hPairOpening.Write();
  hEdepVsConversionZ.Write();
  hCentroidXY.Write();
  hCentroidPhysicalXY.Write();
  hConversionXY.Write();
  hEnergyByIz.Write();
  hCentroidZVsConversionZ.Write();
  hTruthResidual.Write();
  hOpeningVsTransverseWidth.Write();
  hPairProjectedSeparation.Write();
  hProjectedSeparationVsWidth.Write();
  hLastLayerFraction.Write();
  hMaxCellFractionPlot.Write();
  hResponseVsLastLayer.Write();
  hResponseVsBoundary.Write();
  hResponseVsCentroidZ.Write();
  hTrueEnergy.Write();
  hTrueEnergyLog.Write();
  hConvertedTrueEnergy.Write();
  hActiveTrueEnergy.Write();
  hConversionEfficiency.Write();
  hActiveEfficiency.Write();
  hEdepVsTrueEnergy.Write();
  hResponseVsTrueEnergy.Write();
  hBaselineReco.Write();
  output.Close();

  TCanvas canvas("canvas", "canvas", 900, 700);
  canvas.SetTopMargin(0.20);
  gStyle->SetTitleY(0.99);
  gStyle->SetTitleH(0.045);
  hCaloEdepConverted.SetLineColor(kBlue+1);
  hCaloEdepConverted.SetLineWidth(2);
  hCaloEdepConverted.Draw("hist");
  annotate(entries, hCaloEdepConverted.GetEntries(), "first-pair converted");
  saveCanvas(canvas, outDir, "01_calo_edep_converted");
  canvas.Clear();
  hResponse.SetLineColor(kBlue+1);
  hResponse.SetLineWidth(2);
  hResponse.Draw("hist");
  annotate(entries, hResponse.GetEntries(), "first-pair converted");
  saveCanvas(canvas, outDir, "02_raw_response");
  canvas.Clear();
  hBaselineReco.SetLineColor(kRed+1);
  hBaselineReco.SetLineWidth(2);
  hBaselineReco.Draw("hist");
  TLine unity(energyMinGeV, 0, energyMinGeV, hBaselineReco.GetMaximum());
  unity.SetLineStyle(2);
  unity.Draw();
  if (std::abs(energyMaxGeV - energyMinGeV) < 1e-12) {
    annotate(entries, hBaselineReco.GetEntries(),
             "test split, converted and Edep>0");
  } else {
    TLatex notice;
    notice.SetNDC(true);
    notice.SetTextAlign(22);
    notice.SetTextSize(0.04);
    notice.DrawLatex(0.5, 0.52,
        "Not defined for a variable-energy sample");
    annotate(entries, 0, "mono-energy baseline only");
  }
  saveCanvas(canvas, outDir, "03_baseline_reco_test");
  canvas.Clear();
  hNCells1MeV.SetLineColor(kGreen+2);
  hNCells1MeV.SetLineWidth(2);
  hNCells1MeV.Draw("hist");
  annotate(entries, hNCells1MeV.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "04_hit_cell_multiplicity");
  canvas.Clear();
  hConversionZ.SetLineColor(kMagenta+1);
  hConversionZ.SetLineWidth(2);
  hConversionZ.Draw("hist");
  annotate(entries, hConversionZ.GetEntries(), "first-pair converted");
  saveCanvas(canvas, outDir, "05_conversion_z");
  canvas.Clear();
  hEdepVsConversionZ.Draw("colz");
  annotate(entries, hEdepVsConversionZ.GetEntries(), "first-pair converted");
  saveCanvas(canvas, outDir, "06_edep_vs_conversion_z");
  canvas.Clear();
  hCentroidXY.Draw("colz");
  annotate(entries, hCentroidXY.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "07_shower_centroid_xy");
  canvas.Clear();
  hTransverseRms.SetLineColor(kOrange+7);
  hTransverseRms.SetLineWidth(2);
  hTransverseRms.Draw("hist");
  annotate(entries, hTransverseRms.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "08_transverse_width");
  canvas.Clear();
  hEnergyByIz.SetLineColor(kCyan+2);
  hEnergyByIz.SetLineWidth(2);
  hEnergyByIz.Draw("hist");
  annotate(entries, entries, "mean over generated events");
  saveCanvas(canvas, outDir, "09_longitudinal_profile");
  canvas.Clear();
  hPairEnergyShare.SetLineColor(kViolet+1);
  hPairEnergyShare.SetLineWidth(2);
  hPairEnergyShare.Draw("hist");
  annotate(entries, hPairEnergyShare.GetEntries(), "first e^{-}e^{+} pair");
  saveCanvas(canvas, outDir, "10_pair_energy_share");
  canvas.Clear();
  hTrueEnergy.SetLineColor(kBlue+2);
  hTrueEnergy.SetLineWidth(2);
  hTrueEnergy.Draw("hist");
  annotate(entries, hTrueEnergy.GetEntries(), "all generated gamma");
  saveCanvas(canvas, outDir, "11_true_energy");
  canvas.Clear();
  hEdepVsTrueEnergy.Draw("colz");
  annotate(entries, hEdepVsTrueEnergy.GetEntries(), "all generated gamma");
  saveCanvas(canvas, outDir, "12_edep_vs_true_energy");
  canvas.Clear();
  canvas.SetLogx(true);
  hTrueEnergyLog.SetLineColor(kBlue+2);
  hTrueEnergyLog.SetLineWidth(2);
  hTrueEnergyLog.Draw("hist");
  annotate(entries, hTrueEnergyLog.GetEntries(), "all generated gamma");
  saveCanvas(canvas, outDir, "13_true_energy_log_bins");
  canvas.Clear();
  hConversionEfficiency.SetMinimum(0);
  hConversionEfficiency.SetMaximum(1.05);
  hConversionEfficiency.SetLineColor(kMagenta+1);
  hConversionEfficiency.SetLineWidth(2);
  hConversionEfficiency.Draw("hist e");
  annotate(entries, convertedCount, "converted / generated");
  saveCanvas(canvas, outDir, "14_conversion_efficiency_vs_energy");
  canvas.Clear();
  hActiveEfficiency.SetMinimum(0);
  hActiveEfficiency.SetMaximum(1.05);
  hActiveEfficiency.SetLineColor(kGreen+2);
  hActiveEfficiency.SetLineWidth(2);
  hActiveEfficiency.Draw("hist e");
  annotate(entries, entries - zeroCaloCount, "CALO-active / generated");
  saveCanvas(canvas, outDir, "15_calo_active_efficiency_vs_energy");
  canvas.Clear();
  hResponseVsTrueEnergy.Draw("colz");
  annotate(entries, hResponseVsTrueEnergy.GetEntries(), "all generated gamma");
  saveCanvas(canvas, outDir, "16_response_vs_true_energy");
  canvas.SetLogx(false);
  canvas.Clear();
  hCentroidZ.SetLineColor(kBlue+1);
  hCentroidZ.SetLineWidth(2);
  hCentroidZ.Draw("hist");
  annotate(entries, hCentroidZ.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "17_shower_centroid_z");
  canvas.Clear();
  hLongitudinalRms.SetLineColor(kOrange+7);
  hLongitudinalRms.SetLineWidth(2);
  hLongitudinalRms.Draw("hist");
  annotate(entries, hLongitudinalRms.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "18_longitudinal_width");
  canvas.Clear();
  hCentroidPhysicalXY.Draw("colz");
  annotate(entries, hCentroidPhysicalXY.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "19_shower_centroid_xy_cm");
  canvas.Clear();
  hCentroidZVsConversionZ.Draw("colz");
  annotate(entries, hCentroidZVsConversionZ.GetEntries(),
           "converted and CALO Edep>0");
  saveCanvas(canvas, outDir, "20_centroid_z_vs_conversion_z");
  canvas.Clear();
  hTruthResidual.SetLineColor(kBlue+1);
  hTruthResidual.SetLineWidth(2);
  hTruthResidual.Draw("hist");
  annotate(entries, hTruthResidual.GetEntries(),
           "CALO centroid vs generated gamma line");
  saveCanvas(canvas, outDir, "21_centroid_truth_line_residual");
  canvas.Clear();
  hPairOpening.SetLineColor(kViolet+1);
  hPairOpening.SetLineWidth(2);
  // Keep the physically meaningful angle coordinate linear while using a
  // logarithmic event-count axis to expose the long conversion-angle tail.
  canvas.SetLogy(true);
  hPairOpening.Draw("hist");
  annotate(entries, hPairOpening.GetEntries(),
           "first e^{-}e^{+} pair; linear x axis, log event axis, 0-20 deg");
  saveCanvas(canvas, outDir, "22_pair_opening_angle");
  canvas.SetLogy(false);
  canvas.Clear();
  hOpeningVsTransverseWidth.Draw("colz");
  annotate(entries, hOpeningVsTransverseWidth.GetEntries(),
           "converted and CALO Edep>0; linear angle axis, 0-20 deg");
  saveCanvas(canvas, outDir, "23_pair_opening_vs_calo_width");
  canvas.Clear();
  hPairProjectedSeparation.SetLineColor(kViolet+1);
  hPairProjectedSeparation.SetLineWidth(2);
  canvas.SetLogx(true);
  hPairProjectedSeparation.Draw("hist");
  annotate(entries, hPairProjectedSeparation.GetEntries(),
           "straight-line projection, converted and active");
  saveCanvas(canvas, outDir, "24_pair_projected_separation");
  canvas.Clear();
  hProjectedSeparationVsWidth.Draw("colz");
  annotate(entries, hProjectedSeparationVsWidth.GetEntries(),
           "converted and CALO Edep>0");
  saveCanvas(canvas, outDir, "25_projected_separation_vs_calo_width");
  canvas.SetLogx(false);
  canvas.Clear();
  hLastLayerFraction.SetLineColor(kRed+1);
  hLastLayerFraction.SetLineWidth(2);
  canvas.SetLogy(true);
  hLastLayerFraction.Draw("hist");
  annotate(entries, hLastLayerFraction.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "26_last_layer_fraction");
  canvas.SetLogy(false);
  canvas.Clear();
  hResponseVsLastLayer.Draw("colz");
  annotate(entries, hResponseVsLastLayer.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "27_response_vs_last_layer_fraction");
  canvas.Clear();
  hResponseVsBoundary.Draw("colz");
  annotate(entries, hResponseVsBoundary.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "28_response_vs_boundary_distance");
  canvas.Clear();
  hResponseVsCentroidZ.Draw("colz");
  annotate(entries, hResponseVsCentroidZ.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "29_response_vs_shower_depth");
  canvas.Clear();
  hMaxCellFractionPlot.SetLineColor(kGreen+2);
  hMaxCellFractionPlot.SetLineWidth(2);
  hMaxCellFractionPlot.Draw("hist");
  annotate(entries, hMaxCellFractionPlot.GetEntries(), "CALO Edep>0");
  saveCanvas(canvas, outDir, "30_max_cell_fraction");

  std::ofstream summary(outDir + "/numeric_summary.txt");
  summary << std::setprecision(9)
          << "files=" << filesAdded << "\n"
          << "events=" << entries << "\n"
          << "expected_events=" << expectedEntries << "\n"
          << "energy_min_GeV=" << energyMinGeV << "\n"
          << "energy_max_GeV=" << energyMaxGeV << "\n"
          << "converted=" << convertedCount << "\n"
          << "unconverted_final=" << unconvertedCount << "\n"
          << "zero_calo_edep=" << zeroCaloCount << "\n"
          << "calo_active=" << entries - zeroCaloCount << "\n"
          << "converted_calo_active=" << convertedActiveCount << "\n"
          << "first_pair_truth=" << pairTruthCount << "\n"
          << "first_pair_opening_gt_20deg=" << pairOpeningAbove20DegCount
          << "\n"
          << "last_layer_nonzero=" << lastLayerNonzeroCount << "\n"
          << "last_layer_fraction_gt_0p1="
          << lastLayerAbove10PercentCount << "\n"
          << "converted_fraction=" << double(convertedCount)/entries << "\n"
          << "raw_edep_mean_GeV=" << hCaloEdepConverted.GetMean() << "\n"
          << "raw_edep_rms_GeV=" << hCaloEdepConverted.GetRMS() << "\n"
          << "raw_response_mean=" << hResponse.GetMean() << "\n"
          << "raw_response_rms=" << hResponse.GetRMS() << "\n"
          << "calibration_train_mean_edep_GeV=" << calibrationMean << "\n"
          << "calibration_scale=" << calibrationScale << "\n"
          << "test_reco_entries=" << hBaselineReco.GetEntries() << "\n"
          << "test_reco_mean_GeV=" << hBaselineReco.GetMean() << "\n"
          << "test_reco_rms_GeV=" << hBaselineReco.GetRMS() << "\n"
          << "test_reco_relative_rms=" <<
              (hBaselineReco.GetMean() > 0
                  ? hBaselineReco.GetRMS()/hBaselineReco.GetMean() : 0) << "\n"
          << "n_cells_1MeV_mean=" << hNCells1MeV.GetMean() << "\n"
          << "transverse_rms_mean_cells=" << hTransverseRms.GetMean() << "\n"
          << "longitudinal_rms_mean_layers=" << hLongitudinalRms.GetMean() << "\n"
          << "conversion_z_mean_cm=" << hConversionZ.GetMean() << "\n";

  std::cout << "ANALYSIS_SUCCESS\n"
            << "events=" << entries << "\n"
            << "converted=" << convertedCount << "\n"
            << "unconverted_final=" << unconvertedCount << "\n"
            << "test_relative_resolution=" <<
               (hBaselineReco.GetMean() > 0
                  ? hBaselineReco.GetRMS()/hBaselineReco.GetMean() : 0) << "\n"
            << "output=" << outDir << "\n";
}
