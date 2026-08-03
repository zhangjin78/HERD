#include <TCanvas.h>
#include <TChain.h>
#include <TFile.h>
#include <TH2D.h>
#include <TLatex.h>
#include <TLeaf.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TBox.h>
#include <TPad.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {
struct Summary {
  Long64_t generated = 0;
  Long64_t active = 0;
  double xmin = std::numeric_limits<double>::infinity();
  double xmax = -std::numeric_limits<double>::infinity();
  double ymin = std::numeric_limits<double>::infinity();
  double ymax = -std::numeric_limits<double>::infinity();
};

bool readPrimaryGamma(TChain& events, double& x, double& y, double& energy) {
  auto* pdg = events.GetLeaf("mcparts.pdgID");
  auto* status = events.GetLeaf("mcparts.simstat");
  auto* vx = events.GetLeaf("mcparts.vertex.x");
  auto* vy = events.GetLeaf("mcparts.vertex.y");
  auto* px = events.GetLeaf("mcparts.momentum.x");
  auto* py = events.GetLeaf("mcparts.momentum.y");
  auto* pz = events.GetLeaf("mcparts.momentum.z");
  if (!pdg || !status || !vx || !vy || !px || !py || !pz) return false;
  const int n = pdg->GetNdata();
  for (int i = 0; i < n; ++i) {
    const auto simstat = static_cast<unsigned>(status->GetValue(i));
    if (static_cast<int>(pdg->GetValue(i)) != 22 || !(simstat & 1u)) continue;
    x = vx->GetValue(i);
    y = vy->GetValue(i);
    const double p0 = px->GetValue(i);
    const double p1 = py->GetValue(i);
    const double p2 = pz->GetValue(i);
    energy = std::sqrt(p0*p0 + p1*p1 + p2*p2);
    return true;
  }
  return false;
}

double caloEdep(TChain& events) {
  auto* edep = events.GetLeaf("calohits.edep");
  if (!edep) return 0.0;
  double total = 0.0;
  for (int i = 0; i < edep->GetNdata(); ++i)
    if (edep->GetValue(i) > 0.0) total += edep->GetValue(i);
  return total;
}

void annotate(const Summary& summary, const char* selection) {
  TLatex label;
  label.SetNDC(true);
  label.SetTextAlign(31);
  label.SetTextSize(0.028);
  label.DrawLatex(0.87, 0.94, Form("N_{gen} = %lld", summary.generated));
  label.DrawLatex(0.87, 0.895, Form("N_{active} = %lld", summary.active));
  const double efficiency = summary.generated > 0
      ? static_cast<double>(summary.active) / summary.generated : 0.0;
  label.DrawLatex(0.87, 0.85, Form("#bar{#varepsilon}_{active} = %.4f", efficiency));
  label.SetTextSize(0.024);
  label.DrawLatex(0.87, 0.807, selection);
}
}

// Plot primary-gamma entry coordinates.  Only Edep>0 events appear in the
// left panel; the right panel is the corresponding bin-by-bin efficiency.
// The source boundary is read from the generated primary vertices themselves.
void plot_primary_xy_active(const char* inputPattern,
                            const char* outputDirectory,
                            int expectedFiles,
                            Long64_t expectedEntries,
                            double energyMinGeV,
                            double energyMaxGeV,
                            const char* plotName = "31_primary_xy_calo_active",
                            bool presentationSinglePanel = false) {
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
  gStyle->SetPalette(kViridis);
  gSystem->mkdir(outputDirectory, true);

  TChain events("events");
  const int filesAdded = events.Add(inputPattern);
  if (filesAdded != expectedFiles) {
    std::cerr << "ERROR: expected " << expectedFiles << " files, added "
              << filesAdded << "\n";
    return;
  }
  const Long64_t entries = events.GetEntries();
  if (entries != expectedEntries) {
    std::cerr << "ERROR: expected " << expectedEntries << " entries, got "
              << entries << "\n";
    return;
  }

  Summary summary;
  for (Long64_t entry = 0; entry < entries; ++entry) {
    events.LoadTree(entry);
    events.GetEntry(entry);
    double x = 0, y = 0, energy = 0;
    if (!readPrimaryGamma(events, x, y, energy)) continue;
    if (energy < energyMinGeV || energy > energyMaxGeV) continue;
    ++summary.generated;
    summary.xmin = std::min(summary.xmin, x);
    summary.xmax = std::max(summary.xmax, x);
    summary.ymin = std::min(summary.ymin, y);
    summary.ymax = std::max(summary.ymax, y);
    if (caloEdep(events) > 0.0) ++summary.active;
  }
  if (summary.generated == 0) {
    std::cerr << "ERROR: no primary gamma in selected energy range\n";
    return;
  }

  const double span = std::max(summary.xmax-summary.xmin, summary.ymax-summary.ymin);
  const double margin = std::max((presentationSinglePanel ? 0.20 : 0.03)*span,
                                 presentationSinglePanel ? 0.20 : 0.02);
  const double xlow = summary.xmin-margin, xhigh = summary.xmax+margin;
  const double ylow = summary.ymin-margin, yhigh = summary.ymax+margin;
  TH2D hGenerated("hGenerated", "", 40, xlow, xhigh, 40, ylow, yhigh);
  TH2D hActive("hActive", ";x_{0} [cm];y_{0} [cm]",
               40, xlow, xhigh, 40, ylow, yhigh);

  for (Long64_t entry = 0; entry < entries; ++entry) {
    events.LoadTree(entry);
    events.GetEntry(entry);
    double x = 0, y = 0, energy = 0;
    if (!readPrimaryGamma(events, x, y, energy)) continue;
    if (energy < energyMinGeV || energy > energyMaxGeV) continue;
    hGenerated.Fill(x, y);
    if (caloEdep(events) > 0.0) hActive.Fill(x, y);
  }
  TH2D hEfficiency(hActive);
  hEfficiency.SetName("hPrimaryXYCaloActiveEfficiency");
  hEfficiency.SetTitle(";x_{0} [cm];y_{0} [cm]");
  hEfficiency.Divide(&hActive, &hGenerated, 1.0, 1.0, "B");
  hEfficiency.SetMinimum(0.0);
  hEfficiency.SetMaximum(1.0);

  if (presentationSinglePanel) {
    TCanvas canvas("canvas", "canvas", 1000, 850);
    gPad->SetLeftMargin(0.13);
    gPad->SetRightMargin(0.18);
    gPad->SetTopMargin(0.14);
    gPad->SetBottomMargin(0.13);
    hActive.GetXaxis()->SetTitleSize(0.052);
    hActive.GetYaxis()->SetTitleSize(0.052);
    hActive.GetXaxis()->SetLabelSize(0.042);
    hActive.GetYaxis()->SetLabelSize(0.042);
    hActive.GetZaxis()->SetLabelSize(0.038);
    hActive.GetZaxis()->SetTitle("CALO-active events / bin");
    hActive.GetZaxis()->SetTitleSize(0.044);
    hActive.GetZaxis()->SetTitleOffset(1.25);
    hActive.Draw("colz");
    TBox sourceBox(summary.xmin, summary.ymin, summary.xmax, summary.ymax);
    sourceBox.SetFillStyle(0);
    sourceBox.SetLineColor(kRed+1);
    sourceBox.SetLineStyle(2);
    sourceBox.SetLineWidth(4);
    sourceBox.Draw("same");
    TLatex summaryText;
    summaryText.SetNDC(true);
    summaryText.SetTextAlign(22);
    summaryText.SetTextSize(0.033);
    const double activeFraction = static_cast<double>(summary.active) / summary.generated;
    summaryText.DrawLatex(0.50, 0.955,
      Form("N_{gen} = %lld   |   N_{CALO-active} = %lld   |   f_{active} = %.2f%%",
           summary.generated, summary.active, 100.0 * activeFraction));
    const std::string base = std::string(outputDirectory) + "/" + plotName;
    canvas.SaveAs((base + ".png").c_str());
    canvas.SaveAs((base + ".pdf").c_str());
    TFile output((base + ".root").c_str(), "RECREATE");
    hGenerated.Write(); hActive.Write(); output.Close();
    std::cout << "N_generated=" << summary.generated << " N_active=" << summary.active
              << " active_fraction=" << static_cast<double>(summary.active)/summary.generated << "\n";
    return;
  }

  const std::string energyLabel = std::abs(energyMaxGeV-energyMinGeV) < 1e-5
      ? Form("E_{true} = %.3g GeV", 0.5*(energyMinGeV+energyMaxGeV))
      : Form("%.3g #leq E_{true} #leq %.3g GeV", energyMinGeV, energyMaxGeV);
  TCanvas canvas("canvas", "canvas", 1500, 680);
  canvas.Divide(2, 1, 0.002, 0.0);
  for (int panel = 1; panel <= 2; ++panel) {
    canvas.cd(panel);
    gPad->SetRightMargin(0.16);
    gPad->SetLeftMargin(0.13);
    gPad->SetBottomMargin(0.13);
  }
  canvas.cd(1);
  hActive.Draw("colz");
  TBox sourceBox(summary.xmin, summary.ymin, summary.xmax, summary.ymax);
  sourceBox.SetFillStyle(0);
  sourceBox.SetLineColor(kRed+1);
  sourceBox.SetLineStyle(2);
  sourceBox.SetLineWidth(3);
  sourceBox.Draw("same");
  annotate(summary, energyLabel.c_str());
  TLatex leftText;
  leftText.SetNDC(true); leftText.SetTextSize(0.031); leftText.SetTextColor(kBlack);
  leftText.DrawLatex(0.15, 0.94, "CALO-active primary entry density");
  leftText.SetTextSize(0.022); leftText.SetTextColor(kRed+1);
  leftText.DrawLatex(0.15, 0.90, "red dashed: generated-source envelope");

  canvas.cd(2);
  hEfficiency.Draw("colz");
  sourceBox.Draw("same");
  annotate(summary, energyLabel.c_str());
  TLatex rightText;
  rightText.SetNDC(true); rightText.SetTextSize(0.031);
  rightText.DrawLatex(0.15, 0.94, "CALO-active fraction at primary entry");
  rightText.SetTextSize(0.022);
  rightText.DrawLatex(0.15, 0.90, "#varepsilon_{active}(x_{0},y_{0}) = N_{Edep>0}/N_{gen}");

  const std::string base = std::string(outputDirectory) + "/" + plotName;
  canvas.SaveAs((base + ".png").c_str());
  canvas.SaveAs((base + ".pdf").c_str());
  TFile output((base + ".root").c_str(), "RECREATE");
  hGenerated.Write(); hActive.Write(); hEfficiency.Write(); output.Close();
  std::cout << "N_generated=" << summary.generated << " N_active=" << summary.active
            << " active_fraction=" << static_cast<double>(summary.active)/summary.generated << "\n";
}

// Presentation-oriented single-panel version.  It intentionally omits the
// efficiency map and all in-plot statistics; those belong in the slide text.
// The expanded axis range exposes the complete generated source envelope.
void plot_primary_xy_active_density(const char* inputPattern,
                                    const char* outputDirectory,
                                    int expectedFiles,
                                    Long64_t expectedEntries,
                                    double energyMinGeV,
                                    double energyMaxGeV,
                                    const char* plotName = "34_primary_xy_calo_active_density") {
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
  gStyle->SetPalette(kViridis);
  gSystem->mkdir(outputDirectory, true);

  TChain events("events");
  const int filesAdded = events.Add(inputPattern);
  if (filesAdded != expectedFiles) {
    std::cerr << "ERROR: expected " << expectedFiles << " files, added "
              << filesAdded << "\n";
    return;
  }
  const Long64_t entries = events.GetEntries();
  if (entries != expectedEntries) {
    std::cerr << "ERROR: expected " << expectedEntries << " entries, got "
              << entries << "\n";
    return;
  }

  Summary summary;
  for (Long64_t entry = 0; entry < entries; ++entry) {
    events.LoadTree(entry);
    events.GetEntry(entry);
    double x = 0, y = 0, energy = 0;
    if (!readPrimaryGamma(events, x, y, energy)) continue;
    if (energy < energyMinGeV || energy > energyMaxGeV) continue;
    ++summary.generated;
    summary.xmin = std::min(summary.xmin, x);
    summary.xmax = std::max(summary.xmax, x);
    summary.ymin = std::min(summary.ymin, y);
    summary.ymax = std::max(summary.ymax, y);
    if (caloEdep(events) > 0.0) ++summary.active;
  }
  if (summary.generated == 0) {
    std::cerr << "ERROR: no primary gamma in selected energy range\n";
    return;
  }

  const double span = std::max(summary.xmax-summary.xmin, summary.ymax-summary.ymin);
  const double margin = std::max(0.20*span, 0.20);
  const double xlow = summary.xmin-margin, xhigh = summary.xmax+margin;
  const double ylow = summary.ymin-margin, yhigh = summary.ymax+margin;
  TH2D hActive("hPrimaryXYActiveDensity", ";x_{0} [cm];y_{0} [cm]",
               40, xlow, xhigh, 40, ylow, yhigh);
  for (Long64_t entry = 0; entry < entries; ++entry) {
    events.LoadTree(entry);
    events.GetEntry(entry);
    double x = 0, y = 0, energy = 0;
    if (!readPrimaryGamma(events, x, y, energy)) continue;
    if (energy < energyMinGeV || energy > energyMaxGeV) continue;
    if (caloEdep(events) > 0.0) hActive.Fill(x, y);
  }

  TCanvas canvas("canvasDensity", "canvasDensity", 1000, 850);
  gPad->SetLeftMargin(0.13);
  gPad->SetRightMargin(0.18);
  gPad->SetTopMargin(0.07);
  gPad->SetBottomMargin(0.13);
  hActive.GetXaxis()->SetTitleSize(0.052);
  hActive.GetYaxis()->SetTitleSize(0.052);
  hActive.GetXaxis()->SetLabelSize(0.042);
  hActive.GetYaxis()->SetLabelSize(0.042);
  hActive.GetZaxis()->SetLabelSize(0.038);
  hActive.GetZaxis()->SetTitle("CALO-active events / bin");
  hActive.GetZaxis()->SetTitleSize(0.044);
  hActive.GetZaxis()->SetTitleOffset(1.25);
  hActive.Draw("colz");
  TBox sourceBox(summary.xmin, summary.ymin, summary.xmax, summary.ymax);
  sourceBox.SetFillStyle(0);
  sourceBox.SetLineColor(kRed+1);
  sourceBox.SetLineStyle(2);
  sourceBox.SetLineWidth(4);
  sourceBox.Draw("same");

  const std::string base = std::string(outputDirectory) + "/" + plotName;
  canvas.SaveAs((base + ".png").c_str());
  canvas.SaveAs((base + ".pdf").c_str());
  TFile output((base + ".root").c_str(), "RECREATE");
  hActive.Write(); output.Close();
  std::cout << "N_generated=" << summary.generated << " N_active=" << summary.active
            << " active_fraction=" << static_cast<double>(summary.active)/summary.generated << "\n";
}
