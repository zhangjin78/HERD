#include <TCanvas.h>
#include <TChain.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLeaf.h>
#include <TLegend.h>
#include <TLine.h>
#include <TStyle.h>
#include <TSystem.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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
  int split = 0;
  double trueEnergy = 0;
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
  double transverseRms = std::numeric_limits<double>::quiet_NaN();
  double longitudinalRms = std::numeric_limits<double>::quiet_NaN();
  double maxCellFraction = 0;
  double pairEnergyShare = std::numeric_limits<double>::quiet_NaN();
  double pairOpeningDeg = std::numeric_limits<double>::quiet_NaN();
};

void saveCanvas(TCanvas& canvas, const std::string& outputDir,
                const std::string& name) {
  canvas.SaveAs((outputDir + "/" + name + ".png").c_str());
  canvas.SaveAs((outputDir + "/" + name + ".pdf").c_str());
}
}

void analyze_gamma_calo_ml(
    const char* inputPattern,
    const char* outputDirectory) {
  gStyle->SetOptStat(0);
  gStyle->SetPalette(kViridis);
  gSystem->mkdir(outputDirectory, true);
  const std::string outDir(outputDirectory);

  TChain events("events");
  const int filesAdded = events.Add(inputPattern);
  if (filesAdded != 4) {
    std::cerr << "ERROR: expected 4 ROOT files, added " << filesAdded << "\n";
    return;
  }
  const Long64_t entries = events.GetEntries();
  if (entries != 200000) {
    std::cerr << "ERROR: expected 200000 events, found " << entries << "\n";
    return;
  }

  TH1D hCaloEdepAll("hCaloEdepAll",
      "CALO deposited energy;E_{dep} [GeV];Events", 240, 0, 1.2);
  TH1D hCaloEdepConverted("hCaloEdepConverted",
      "CALO deposited energy;E_{dep} [GeV];Events", 240, 0, 1.2);
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
      100, -40, 40);
  TH1D hTransverseRms("hTransverseRms",
      "Transverse shower width;#sqrt{#sigma_{ix}^{2}+#sigma_{iy}^{2}} [cell];Events",
      100, 0, 20);
  TH1D hLongitudinalRms("hLongitudinalRms",
      "Longitudinal shower width;#sigma_{iz} [layer];Events",
      100, 0, 20);
  TH1D hConversionZ("hConversionZ",
      "First gamma-conversion vertex;z_{conv} [cm];Events",
      180, -100, 80);
  TH1D hPairEnergyShare("hPairEnergyShare",
      "Pair energy sharing;min(E_{e-},E_{e+})/(E_{e-}+E_{e+});Events",
      100, 0, 0.5);
  TH1D hPairOpening("hPairOpening",
      "e^{-}e^{+} opening angle;Opening angle [deg];Events",
      120, 0, 12);
  TH2D hEdepVsConversionZ("hEdepVsConversionZ",
      "CALO response versus conversion depth;z_{conv} [cm];E_{dep} [GeV]",
      180, -100, 80, 160, 0, 1.2);
  TH2D hCentroidXY("hCentroidXY",
      "Energy-weighted CALO shower centroid;ix;iy",
      100, -40, 40, 100, -40, 40);
  TH2D hConversionXY("hConversionXY",
      "First gamma-conversion position;x_{conv} [cm];y_{conv} [cm]",
      120, -60, 60, 120, -60, 60);
  TH1D hEnergyByIz("hEnergyByIz",
      "Mean deposited-energy profile versus CALO iz;iz;Mean E per event [GeV]",
      81, -40.5, 40.5);

  std::vector<EventFeature> features;
  features.reserve(entries);
  long long convertedCount = 0;
  long long unconvertedCount = 0;
  long long zeroCaloCount = 0;
  double calibrationEdepSum = 0;
  long long calibrationCount = 0;

  for (Long64_t globalEntry = 0; globalEntry < entries; ++globalEntry) {
    events.LoadTree(globalEntry);
    events.GetEntry(globalEntry);
    EventFeature feature;
    feature.event = globalEntry;
    feature.split = events.GetTreeNumber();

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
      maxCellEnergy = std::max(maxCellEnergy, energy);
      hEnergyByIz.Fill(iz, energy);
    }
    if (feature.caloEdep > 0) {
      feature.centroidIx = weightedX / feature.caloEdep;
      feature.centroidIy = weightedY / feature.caloEdep;
      feature.centroidIz = weightedZ / feature.caloEdep;
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
    } else {
      ++zeroCaloCount;
    }

    hCaloEdepAll.Fill(feature.caloEdep);
    if (feature.converted) {
      ++convertedCount;
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
      if (feature.split <= 1 && feature.caloEdep > 0) {
        calibrationEdepSum += feature.caloEdep;
        ++calibrationCount;
      }
    }
    if (feature.unconvertedFinal) {
      ++unconvertedCount;
      hCaloEdepUnconverted.Fill(feature.caloEdep);
    }
    if (feature.caloEdep > 0) {
      hNCells.Fill(feature.nCells);
      hNCells1MeV.Fill(feature.nCells1MeV);
      hMaxCellFraction.Fill(feature.maxCellFraction);
      hCentroidZ.Fill(feature.centroidIz);
      hTransverseRms.Fill(feature.transverseRms);
      hLongitudinalRms.Fill(feature.longitudinalRms);
      hCentroidXY.Fill(feature.centroidIx, feature.centroidIy);
    }
    features.push_back(feature);
  }

  const double calibrationMean =
      calibrationCount ? calibrationEdepSum/calibrationCount : 0;
  const double calibrationScale =
      calibrationMean > 0 ? 1.0/calibrationMean : 0;
  TH1D hBaselineReco("hBaselineReco",
      "Independent-test baseline energy;E_{reco}=E_{dep}/<E_{dep}>_{train} [GeV];Events",
      240, 0, 1.8);
  for (const auto& feature : features) {
    if (feature.split == 3 && feature.converted && feature.caloEdep > 0)
      hBaselineReco.Fill(feature.caloEdep * calibrationScale);
  }

  std::ofstream csv(outDir + "/event_features.csv");
  csv << "event,split,true_energy_GeV,converted,unconverted_final,"
         "conversion_x_cm,conversion_y_cm,conversion_z_cm,calo_edep_GeV,"
         "n_cells,n_cells_gt_1MeV,n_cells_gt_20MeV,centroid_ix,"
         "centroid_iy,centroid_iz,transverse_rms_cells,longitudinal_rms_layers,"
         "max_cell_fraction,pair_energy_share,pair_opening_deg\n";
  csv << std::setprecision(9);
  for (const auto& f : features) {
    csv << f.event << ',' << f.split << ',' << f.trueEnergy << ','
        << f.converted << ',' << f.unconvertedFinal << ','
        << f.convX << ',' << f.convY << ',' << f.convZ << ','
        << f.caloEdep << ',' << f.nCells << ',' << f.nCells1MeV << ','
        << f.nCells20MeV << ',' << f.centroidIx << ',' << f.centroidIy << ','
        << f.centroidIz << ',' << f.transverseRms << ','
        << f.longitudinalRms << ',' << f.maxCellFraction << ','
        << f.pairEnergyShare << ',' << f.pairOpeningDeg << '\n';
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
  hConversionXY.Write();
  hEnergyByIz.Write();
  hBaselineReco.Write();
  output.Close();

  TCanvas canvas("canvas", "canvas", 900, 700);
  hCaloEdepConverted.SetLineColor(kBlue+1);
  hCaloEdepConverted.SetLineWidth(2);
  hCaloEdepConverted.Draw("hist");
  saveCanvas(canvas, outDir, "01_calo_edep_converted");
  canvas.Clear();
  hResponse.SetLineColor(kBlue+1);
  hResponse.SetLineWidth(2);
  hResponse.Draw("hist");
  saveCanvas(canvas, outDir, "02_raw_response");
  canvas.Clear();
  hBaselineReco.SetLineColor(kRed+1);
  hBaselineReco.SetLineWidth(2);
  hBaselineReco.Draw("hist");
  TLine unity(1, 0, 1, hBaselineReco.GetMaximum());
  unity.SetLineStyle(2);
  unity.Draw();
  saveCanvas(canvas, outDir, "03_baseline_reco_test");
  canvas.Clear();
  hNCells1MeV.SetLineColor(kGreen+2);
  hNCells1MeV.SetLineWidth(2);
  hNCells1MeV.Draw("hist");
  saveCanvas(canvas, outDir, "04_hit_cell_multiplicity");
  canvas.Clear();
  hConversionZ.SetLineColor(kMagenta+1);
  hConversionZ.SetLineWidth(2);
  hConversionZ.Draw("hist");
  saveCanvas(canvas, outDir, "05_conversion_z");
  canvas.Clear();
  hEdepVsConversionZ.Draw("colz");
  saveCanvas(canvas, outDir, "06_edep_vs_conversion_z");
  canvas.Clear();
  hCentroidXY.Draw("colz");
  saveCanvas(canvas, outDir, "07_shower_centroid_xy");
  canvas.Clear();
  hTransverseRms.SetLineColor(kOrange+7);
  hTransverseRms.SetLineWidth(2);
  hTransverseRms.Draw("hist");
  saveCanvas(canvas, outDir, "08_transverse_width");
  canvas.Clear();
  hEnergyByIz.SetLineColor(kCyan+2);
  hEnergyByIz.SetLineWidth(2);
  hEnergyByIz.Draw("hist");
  saveCanvas(canvas, outDir, "09_longitudinal_profile");
  canvas.Clear();
  hPairEnergyShare.SetLineColor(kViolet+1);
  hPairEnergyShare.SetLineWidth(2);
  hPairEnergyShare.Draw("hist");
  saveCanvas(canvas, outDir, "10_pair_energy_share");

  std::ofstream summary(outDir + "/numeric_summary.txt");
  summary << std::setprecision(9)
          << "files=" << filesAdded << "\n"
          << "events=" << entries << "\n"
          << "converted=" << convertedCount << "\n"
          << "unconverted_final=" << unconvertedCount << "\n"
          << "zero_calo_edep=" << zeroCaloCount << "\n"
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
              hBaselineReco.GetRMS()/hBaselineReco.GetMean() << "\n"
          << "n_cells_1MeV_mean=" << hNCells1MeV.GetMean() << "\n"
          << "transverse_rms_mean_cells=" << hTransverseRms.GetMean() << "\n"
          << "longitudinal_rms_mean_layers=" << hLongitudinalRms.GetMean() << "\n"
          << "conversion_z_mean_cm=" << hConversionZ.GetMean() << "\n";

  std::cout << "ANALYSIS_SUCCESS\n"
            << "events=" << entries << "\n"
            << "converted=" << convertedCount << "\n"
            << "unconverted_final=" << unconvertedCount << "\n"
            << "test_relative_resolution=" <<
               hBaselineReco.GetRMS()/hBaselineReco.GetMean() << "\n"
            << "output=" << outDir << "\n";
}
