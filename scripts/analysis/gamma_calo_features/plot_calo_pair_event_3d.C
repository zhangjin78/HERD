#include <TCanvas.h>
#include <TFile.h>
#include <TLeaf.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>
#include <TH3D.h>
#include <TH2D.h>
#include <TColor.h>
#include <TGraph.h>
#include <TLatex.h>
#include <TView.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
struct PairCandidate {
  Long64_t entry = -1;
  int eventNumber = -1;
  double angleDeg = -1.;
  double edep = 0.;
  int nCells = 0;
};

struct FiredCell {
  double x = 0., y = 0., z = 0.;
  double e = 0.;
};

double openingAngleDeg(double ax, double ay, double az,
                       double bx, double by, double bz) {
  const double amag = std::sqrt(ax*ax + ay*ay + az*az);
  const double bmag = std::sqrt(bx*bx + by*by + bz*bz);
  if (amag == 0. || bmag == 0.) return -1.;
  double cosine = (ax*bx + ay*by + az*bz) / (amag*bmag);
  cosine = std::max(-1., std::min(1., cosine));
  return std::acos(cosine) * 180. / M_PI;
}

double momentumMagnitude(double x, double y, double z) {
  return std::sqrt(x*x + y*y + z*z);
}
}

// Finds a truth-level primary-gamma -> e+e- event with the largest opening
// angle in one ROOT file, then draws its non-zero CALO crystals as a TH3D.
// Crystal coordinates are index/layer units, not cm.
void plot_calo_pair_event_3d(
    const char* inputFile,
    const char* outputDirectory,
    const char* plotName = "01_calo_3d_truth_pair_candidate") {
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
  gStyle->SetPalette(kViridis);
  gSystem->mkdir(outputDirectory, true);

  TFile input(inputFile, "READ");
  auto* events = dynamic_cast<TTree*>(input.Get("events"));
  if (!events) { std::cerr << "ERROR: events tree not found\n"; return; }

  auto* pdg = events->GetLeaf("mcparts.pdgID");
  auto* track = events->GetLeaf("mcparts.trackID");
  auto* parent = events->GetLeaf("mcparts.parentID");
  auto* simstat = events->GetLeaf("mcparts.simstat");
  auto* mpx = events->GetLeaf("mcparts.momentum.x");
  auto* mpy = events->GetLeaf("mcparts.momentum.y");
  auto* mpz = events->GetLeaf("mcparts.momentum.z");
  auto* ix = events->GetLeaf("calohits.ix");
  auto* iy = events->GetLeaf("calohits.iy");
  auto* iz = events->GetLeaf("calohits.iz");
  auto* edep = events->GetLeaf("calohits.edep");
  auto* eventID = events->GetLeaf("evinfo.event");
  if (!pdg || !track || !parent || !simstat || !mpx || !mpy || !mpz ||
      !ix || !iy || !iz || !edep) {
    std::cerr << "ERROR: required MC/CALO branches not found\n"; return;
  }

  PairCandidate best;
  const Long64_t nEntries = events->GetEntries();
  for (Long64_t entry = 0; entry < nEntries; ++entry) {
    events->GetEntry(entry);
    int gammaTrack = -1;
    for (int i = 0; i < pdg->GetNdata(); ++i) {
      if (static_cast<int>(pdg->GetValue(i)) == 22 &&
          (static_cast<unsigned>(simstat->GetValue(i)) & 1u)) {
        gammaTrack = static_cast<int>(track->GetValue(i));
        break;
      }
    }
    if (gammaTrack < 0) continue;

    int electron = -1, positron = -1;
    for (int i = 0; i < pdg->GetNdata(); ++i) {
      if (static_cast<int>(parent->GetValue(i)) != gammaTrack) continue;
      const int id = static_cast<int>(pdg->GetValue(i));
      if (id == 11 && electron < 0) electron = i;
      if (id == -11 && positron < 0) positron = i;
    }
    if (electron < 0 || positron < 0) continue;
    const double electronP = momentumMagnitude(mpx->GetValue(electron), mpy->GetValue(electron), mpz->GetValue(electron));
    const double positronP = momentumMagnitude(mpx->GetValue(positron), mpy->GetValue(positron), mpz->GetValue(positron));
    // Reject extremely soft partners: a large angle for an almost-stopped
    // secondary is not a useful representative pair-conversion topology.
    if (electronP < 0.05 || positronP < 0.05) continue;

    double totalEdep = 0.;
    int nCells = 0;
    for (int i = 0; i < edep->GetNdata(); ++i) {
      const double e = edep->GetValue(i);
      if (e > 0.) { totalEdep += e; ++nCells; }
    }
    if (nCells == 0) continue;
    const double angle = openingAngleDeg(
      mpx->GetValue(electron), mpy->GetValue(electron), mpz->GetValue(electron),
      mpx->GetValue(positron), mpy->GetValue(positron), mpz->GetValue(positron));
    if (angle > best.angleDeg) {
      best.entry = entry;
      best.eventNumber = eventID ? static_cast<int>(eventID->GetValue(0)) : static_cast<int>(entry);
      best.angleDeg = angle;
      best.edep = totalEdep;
      best.nCells = nCells;
    }
  }
  if (best.entry < 0) { std::cerr << "ERROR: no direct e+e- pair candidate\n"; return; }

  events->GetEntry(best.entry);
  int minX = std::numeric_limits<int>::max(), maxX = std::numeric_limits<int>::min();
  int minY = minX, maxY = maxX, minZ = minX, maxZ = maxX;
  for (int i = 0; i < edep->GetNdata(); ++i) {
    if (edep->GetValue(i) <= 0.) continue;
    minX = std::min(minX, static_cast<int>(ix->GetValue(i))); maxX = std::max(maxX, static_cast<int>(ix->GetValue(i)));
    minY = std::min(minY, static_cast<int>(iy->GetValue(i))); maxY = std::max(maxY, static_cast<int>(iy->GetValue(i)));
    minZ = std::min(minZ, static_cast<int>(iz->GetValue(i))); maxZ = std::max(maxZ, static_cast<int>(iz->GetValue(i)));
  }
  const int pad = 1;
  TH3D cells("calo_cells", ";crystal i_{x};crystal i_{y};CALO layer i_{z}",
             maxX-minX+1+2*pad, minX-pad-0.5, maxX+pad+0.5,
             maxY-minY+1+2*pad, minY-pad-0.5, maxY+pad+0.5,
             maxZ-minZ+1+2*pad, minZ-pad-0.5, maxZ+pad+0.5);
  std::vector<FiredCell> fired;
  double minE = std::numeric_limits<double>::infinity(), maxE = 0.;
  for (int i = 0; i < edep->GetNdata(); ++i) {
    if (edep->GetValue(i) > 0.) {
      cells.Fill(ix->GetValue(i), iy->GetValue(i), iz->GetValue(i), edep->GetValue(i));
      // v2025a CALO placement, transcribed from calogeo_sphere_v2.cc and
      // v2025a_cfg.cfgxml: indices 0..22, 0..22, 0..20 map to the actual
      // centres of 30 mm LYSO crystals.  The two y-direction structural gaps
      // are retained; z has no corresponding large gap in this geometry.
      const int xid = static_cast<int>(ix->GetValue(i));
      const int yid = static_cast<int>(iy->GetValue(i));
      const int zid = static_cast<int>(iz->GetValue(i));
      const int cx = xid - 11, cy = yid - 11, cz = zid - 10;
      const double x = 38.0 * cx;
      const double y = 33.0 * cy + (std::abs(cy) > 3 ? 16.5 * (cy > 0 ? 1.0 : -1.0) : 0.0);
      const double z = 33.0 * cz;
      fired.push_back({x, y, z, edep->GetValue(i)});
      minE = std::min(minE, edep->GetValue(i)); maxE = std::max(maxE, edep->GetValue(i));
    }
  }

  // Fixed-size isometric voxels: spatial box dimensions never encode energy.
  // Only their colour changes with Edep, so shower morphology is not distorted.
  const auto project = [](double x, double y, double z) {
    return std::pair<double, double>{0.8660254*(x-y), 0.50*(x+y) + z};
  };
  const double crystalSide = 30.0; // mm, from v2025a_cfg.cfgxml and calodet-sphere23z21-v3j2.xml
  const double half = crystalSide / 2.0;
  double xlo = std::numeric_limits<double>::infinity(), xhi = -xlo;
  double ylo = xlo, yhi = xhi;
  for (const auto& cell : fired) {
    for (const double dx : {-half, half}) for (const double dy : {-half, half}) for (const double dz : {-half, half}) {
      const auto p = project(cell.x+dx, cell.y+dy, cell.z+dz);
      xlo = std::min(xlo, p.first); xhi = std::max(xhi, p.first);
      ylo = std::min(ylo, p.second); yhi = std::max(yhi, p.second);
    }
  }
  xlo -= 35.; xhi += 35.; ylo -= 35.; yhi += 35.;
  TCanvas canvas("canvas", "canvas", 1200, 920);
  canvas.SetLeftMargin(0.08); canvas.SetRightMargin(0.05);
  canvas.SetBottomMargin(0.08); canvas.SetTopMargin(0.14);
  TH2D frame("frame", ";isometric transverse coordinate [mm];isometric depth coordinate [mm]",
             10, xlo, xhi, 10, ylo, yhi);
  frame.GetXaxis()->SetLabelSize(0.0); frame.GetYaxis()->SetLabelSize(0.0);
  frame.GetXaxis()->SetTickLength(0.0); frame.GetYaxis()->SetTickLength(0.0);
  frame.Draw();
  std::sort(fired.begin(), fired.end(), [](const FiredCell& a, const FiredCell& b) {
    return (a.x + a.y + a.z) < (b.x + b.y + b.z);
  });
  const double logMin = std::log10(std::max(minE, 1e-8));
  const double logMax = std::log10(std::max(maxE, minE * 1.001));
  for (const auto& cell : fired) {
    const double fraction = (std::log10(cell.e) - logMin) / (logMax - logMin);
    const int colour = TColor::GetColorPalette(std::max(0, std::min(255, static_cast<int>(255*fraction))));
    const auto a = project(cell.x-half, cell.y-half, cell.z-half);
    const auto b = project(cell.x+half, cell.y-half, cell.z-half);
    const auto c = project(cell.x+half, cell.y+half, cell.z-half);
    const auto d = project(cell.x-half, cell.y+half, cell.z-half);
    const auto e = project(cell.x-half, cell.y-half, cell.z+half);
    const auto f = project(cell.x+half, cell.y-half, cell.z+half);
    const auto g = project(cell.x+half, cell.y+half, cell.z+half);
    const auto h = project(cell.x-half, cell.y+half, cell.z+half);
    const std::vector<std::vector<std::pair<double,double>>> faces = {{e,f,g,h,e}, {b,c,g,f,b}, {d,c,g,h,d}};
    for (const auto& face : faces) {
      auto* polygon = new TGraph(static_cast<int>(face.size()));
      for (int j = 0; j < static_cast<int>(face.size()); ++j) polygon->SetPoint(j, face[j].first, face[j].second);
      polygon->SetFillColor(colour); polygon->SetLineColor(kBlack); polygon->SetLineWidth(1);
      polygon->Draw("FL");
    }
  }

  TLatex header;
  header.SetNDC(true); header.SetTextAlign(22); header.SetTextSize(0.032);
  header.DrawLatex(0.50, 0.965,
    Form("1 GeV primary #gamma, entry %lld (event %d): truth e^{+}e^{-} opening angle = %.2f^{#circ}",
         best.entry, best.eventNumber, best.angleDeg));
  header.SetTextSize(0.027);
  header.DrawLatex(0.50, 0.925,
    Form("CALO: %d v2025a 30#times30#times30 mm^{3} crystal voxels (true gaps retained),  #SigmaE_{dep} = %.3g GeV", best.nCells, best.edep));
  header.SetTextSize(0.025);
  header.DrawLatex(0.50, 0.890,
    "fixed crystal size; colour only: E_{dep} (blue low #rightarrow yellow high)  |  x pitch 38 mm, y/z pitch 33 mm");

  const std::string base = std::string(outputDirectory) + "/" + plotName;
  canvas.SaveAs((base + ".png").c_str());
  canvas.SaveAs((base + ".pdf").c_str());
  TFile output((base + ".root").c_str(), "RECREATE");
  cells.Write(); output.Close();
  std::cout << "Selected entry=" << best.entry << " event=" << best.eventNumber
            << " opening_angle_deg=" << best.angleDeg << " fired_cells=" << best.nCells
            << " edep_GeV=" << best.edep << "\n";
}
