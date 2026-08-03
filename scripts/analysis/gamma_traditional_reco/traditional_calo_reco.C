#include <TCanvas.h>
#include <TChain.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLeaf.h>
#include <TSystem.h>
#include <TTree.h>

#include <algorithm>
#include <array>
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
constexpr int kFeatures = 6;

int jobFromFilename(const std::string& file) {
  std::smatch match;
  if (std::regex_search(file, match, std::regex("job([0-9]{6})")))
    return std::stoi(match[1]);
  return -1;
}

std::set<int> parseJobs(const char* text) {
  std::set<int> answer;
  std::stringstream input(text ? text : "");
  std::string token;
  while (std::getline(input, token, ',')) if (!token.empty()) answer.insert(std::stoi(token));
  return answer;
}

double angleDeg(double ax, double ay, double az, double bx, double by, double bz) {
  const double an = std::sqrt(ax*ax + ay*ay + az*az);
  const double bn = std::sqrt(bx*bx + by*by + bz*bz);
  if (an <= 0 || bn <= 0) return std::numeric_limits<double>::quiet_NaN();
  const double cosine = std::max(-1.0, std::min(1.0, (ax*bx + ay*by + az*bz)/(an*bn)));
  return std::acos(cosine) * 180.0 / M_PI;
}

// Solve normal equations with partial pivoting.  Returns false for a singular fit.
bool solve(std::array<std::array<double, kFeatures + 1>, kFeatures>& a,
           std::array<double, kFeatures>& x) {
  for (int col = 0; col < kFeatures; ++col) {
    int pivot = col;
    for (int row = col + 1; row < kFeatures; ++row)
      if (std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
    if (std::abs(a[pivot][col]) < 1e-12) return false;
    std::swap(a[pivot], a[col]);
    const double scale = a[col][col];
    for (int j = col; j <= kFeatures; ++j) a[col][j] /= scale;
    for (int row = 0; row < kFeatures; ++row) {
      if (row == col) continue;
      const double factor = a[row][col];
      for (int j = col; j <= kFeatures; ++j) a[row][j] -= factor*a[col][j];
    }
  }
  for (int i = 0; i < kFeatures; ++i) x[i] = a[i][kFeatures];
  return true;
}

struct Event {
  long long event = 0;
  int job = -1, split = -1;
  double eTrue = 0, eDep = 0, fLast = 0, centroidZ = 0, width = 0, edge = 0;
  double primaryPx = 0, primaryPy = 0, primaryPz = 0;
  double convX = std::numeric_limits<double>::quiet_NaN();
  double convY = std::numeric_limits<double>::quiet_NaN();
  double convZ = std::numeric_limits<double>::quiet_NaN();
  double pairPx = 0, pairPy = 0, pairPz = 0;
  int hasPair = 0, hasCaloAxis = 0;
  double caloAxisPx = 0, caloAxisPy = 0, caloAxisPz = 0;
};

std::array<double, kFeatures> featureVector(const Event& e) {
  return {1.0, std::log(std::max(e.eDep, 1e-8)), e.fLast,
          e.centroidZ / 20.0, e.width, e.edge / 11.0};
}
}

void traditional_calo_reco(const char* inputPattern, const char* outputDirectory,
                           int expectedFiles, long long expectedEntries,
                           const char* trainJobsText, const char* validationJobsText,
                           const char* testJobsText) {
  gSystem->mkdir(outputDirectory, true);
  const std::string out(outputDirectory);
  const auto train = parseJobs(trainJobsText);
  const auto validation = parseJobs(validationJobsText);
  const auto test = parseJobs(testJobsText);
  TChain events("events");
  const int files = events.Add(inputPattern);
  const long long entries = events.GetEntries();
  if (files != expectedFiles || entries != expectedEntries) {
    std::cerr << "INPUT_VALIDATION_FAILED files=" << files << " entries=" << entries << "\n";
    return;
  }
  std::vector<Event> data;
  data.reserve(entries);
  for (long long entry = 0; entry < entries; ++entry) {
    events.LoadTree(entry);
    events.GetEntry(entry);
    Event e;
    e.event = entry;
    e.job = jobFromFilename(events.GetCurrentFile()->GetName());
    e.split = train.count(e.job) ? 0 : validation.count(e.job) ? 1 : test.count(e.job) ? 2 : -1;
    auto* pdg = events.GetLeaf("mcparts.pdgID");
    auto* track = events.GetLeaf("mcparts.trackID");
    auto* parent = events.GetLeaf("mcparts.parentID");
    auto* state = events.GetLeaf("mcparts.simstat");
    auto* px = events.GetLeaf("mcparts.momentum.x");
    auto* py = events.GetLeaf("mcparts.momentum.y");
    auto* pz = events.GetLeaf("mcparts.momentum.z");
    auto* vx = events.GetLeaf("mcparts.vertex.x");
    auto* vy = events.GetLeaf("mcparts.vertex.y");
    auto* vz = events.GetLeaf("mcparts.vertex.z");
    int primaryTrack = -1;
    const int nmc = pdg ? pdg->GetNdata() : 0;
    for (int i = 0; i < nmc; ++i) {
      const int code = int(pdg->GetValue(i));
      const int id = int(track->GetValue(i));
      const int mother = int(parent->GetValue(i));
      const unsigned flags = unsigned(state->GetValue(i));
      const double x = px->GetValue(i), y = py->GetValue(i), z = pz->GetValue(i);
      if (code == 22 && (flags & 1u)) {
        primaryTrack = id; e.primaryPx = x; e.primaryPy = y; e.primaryPz = z;
        e.eTrue = std::sqrt(x*x + y*y + z*z);
      }
      if ((flags & 2u) && mother == primaryTrack && (code == 11 || code == -11)) {
        e.hasPair = 1; e.pairPx += x; e.pairPy += y; e.pairPz += z;
        if (std::isnan(e.convZ)) { e.convX = vx->GetValue(i); e.convY = vy->GetValue(i); e.convZ = vz->GetValue(i); }
      }
    }
    auto* ce = events.GetLeaf("calohits.edep");
    auto* cx = events.GetLeaf("calohits.ix");
    auto* cy = events.GetLeaf("calohits.iy");
    auto* cz = events.GetLeaf("calohits.iz");
    std::array<double, 21> layerE{}, layerX{}, layerY{};
    double sx=0, sy=0, sz=0, sx2=0, sy2=0, maxE=0;
    const int ncalo = ce ? ce->GetNdata() : 0;
    for (int i = 0; i < ncalo; ++i) {
      const double energy = ce->GetValue(i); if (energy <= 0) continue;
      const double x = cx->GetValue(i), y = cy->GetValue(i), z = cz->GetValue(i);
      e.eDep += energy; sx += energy*x; sy += energy*y; sz += energy*z;
      sx2 += energy*x*x; sy2 += energy*y*y; maxE = std::max(maxE, energy);
      const int iz = int(z);
      if (iz >= 0 && iz < 21) { layerE[iz] += energy; layerX[iz] += energy*x; layerY[iz] += energy*y; }
    }
    if (e.eDep > 0) {
      const double mx=sx/e.eDep, my=sy/e.eDep;
      e.centroidZ=sz/e.eDep;
      e.width=std::sqrt(std::max(0.0, sx2/e.eDep-mx*mx)+std::max(0.0, sy2/e.eDep-my*my));
      e.edge=std::min({mx, 22.0-mx, my, 22.0-my});
      e.fLast=layerE[0]/e.eDep;
      double sw=0, swz=0, swzz=0, swx=0, swy=0, swzx=0, swzy=0;
      for (int iz=0; iz<21; ++iz) if (layerE[iz] > 0) {
        const double w=layerE[iz], z=iz, x=layerX[iz]/w, y=layerY[iz]/w;
        sw+=w; swz+=w*z; swzz+=w*z*z; swx+=w*x; swy+=w*y; swzx+=w*z*x; swzy+=w*z*y;
      }
      const double den=sw*swzz-swz*swz;
      if (den > 1e-9) {
        const double dx=(sw*swzx-swz*swx)/den, dy=(sw*swzy-swz*swy)/den;
        e.caloAxisPx=-dx; e.caloAxisPy=-dy; e.caloAxisPz=-1.0; e.hasCaloAxis=1;
      }
    }
    data.push_back(e);
  }
  std::array<std::array<double, kFeatures + 1>, kFeatures> normal{};
  double baseN=0, baseX=0, baseXX=0, baseY=0, baseXY=0;
  long long trainActive = 0;
  for (const auto& e : data) if (e.split == 0 && e.eDep > 0 && e.eTrue > 0) {
    const auto f=featureVector(e); const double y=std::log(e.eTrue);
    for (int r=0;r<kFeatures;++r) { normal[r][kFeatures]+=f[r]*y; for (int c=0;c<kFeatures;++c) normal[r][c]+=f[r]*f[c]; }
    ++trainActive;
    baseN += 1.0; baseX += f[1]; baseXX += f[1]*f[1]; baseY += y; baseXY += f[1]*y;
  }
  for (int i=0;i<kFeatures;++i) normal[i][i]+=1e-9;
  std::array<double,kFeatures> coefficients{};
  if (!solve(normal, coefficients)) { std::cerr << "FIT_FAILED\n"; return; }
  const double baseDet = baseN*baseXX-baseX*baseX;
  if (std::abs(baseDet) < 1e-12) { std::cerr << "BASE_FIT_FAILED\n"; return; }
  const double baseA = (baseY*baseXX-baseX*baseXY)/baseDet;
  const double baseB = (baseN*baseXY-baseX*baseY)/baseDet;
  TH1D raw("raw_response_test", "Raw CALO response;E_{dep}/E_{true};Events", 160, 0, 1.6);
  TH1D calibrated("log_calibration_response_test", "Log-calibrated response;E_{reco}/E_{true};Events", 160, 0, 1.6);
  TH1D leakage("leakage_linear_response_test", "Leakage-corrected response;E_{reco}/E_{true};Events", 160, 0, 1.6);
  TH1D pairAngle("mc_pair_direction_residual", "MC pair-sum direction relative to primary gamma;#Delta#theta [deg];Events", 180, 0, 18);
  TH1D axisAngle("calo_axis_direction_residual", "CALO shower-axis direction relative to primary gamma;#Delta#theta [deg];Events", 180, 0, 18);
  TTree output("traditional_reco", "traditional reconstruction outputs");
  long long event; int job, split, active, has_pair, has_calo_axis; double etrue, edep, reco_log, reco_leak, conv_x, conv_y, conv_z, pair_angle, axis_angle;
  output.Branch("event",&event); output.Branch("job_id",&job); output.Branch("split",&split); output.Branch("calo_active",&active); output.Branch("has_mc_pair",&has_pair); output.Branch("has_calo_axis",&has_calo_axis);
  output.Branch("true_energy_GeV",&etrue); output.Branch("calo_edep_GeV",&edep); output.Branch("ereco_log_cal_GeV",&reco_log); output.Branch("ereco_leakage_GeV",&reco_leak); output.Branch("mc_conversion_x_cm",&conv_x); output.Branch("mc_conversion_y_cm",&conv_y); output.Branch("mc_conversion_z_cm",&conv_z); output.Branch("mc_pair_direction_delta_deg",&pair_angle); output.Branch("calo_axis_direction_delta_deg",&axis_angle);
  std::ofstream csv(out+"/traditional_reco.csv");
  csv << "event,job_id,split,calo_active,true_energy_GeV,calo_edep_GeV,ereco_log_cal_GeV,ereco_leakage_GeV,mc_conversion_x_cm,mc_conversion_y_cm,mc_conversion_z_cm,mc_pair_direction_delta_deg,calo_axis_direction_delta_deg\n";
  long long activeCount=0, pairCount=0, axisCount=0;
  for (const auto& e : data) {
    event=e.event; job=e.job; split=e.split; active=e.eDep>0; has_pair=e.hasPair; has_calo_axis=e.hasCaloAxis; etrue=e.eTrue; edep=e.eDep; conv_x=e.convX; conv_y=e.convY; conv_z=e.convZ;
    reco_log=0; reco_leak=0; pair_angle=std::numeric_limits<double>::quiet_NaN(); axis_angle=std::numeric_limits<double>::quiet_NaN();
    if (active) { const auto f=featureVector(e); reco_log=std::exp(baseA+baseB*f[1]); double loge=0; for(int i=0;i<kFeatures;++i) loge+=coefficients[i]*f[i]; reco_leak=std::exp(loge); ++activeCount; }
    if (e.hasPair) { pair_angle=angleDeg(e.pairPx,e.pairPy,e.pairPz,e.primaryPx,e.primaryPy,e.primaryPz); ++pairCount; }
    if (e.hasCaloAxis) { axis_angle=angleDeg(e.caloAxisPx,e.caloAxisPy,e.caloAxisPz,e.primaryPx,e.primaryPy,e.primaryPz); ++axisCount; }
    if (split==2 && active && etrue>0) { raw.Fill(edep/etrue); calibrated.Fill(reco_log/etrue); leakage.Fill(reco_leak/etrue); if(!std::isnan(pair_angle)) pairAngle.Fill(pair_angle); if(!std::isnan(axis_angle)) axisAngle.Fill(axis_angle); }
    output.Fill(); csv << event << ',' << job << ',' << split << ',' << active << ',' << etrue << ',' << edep << ',' << reco_log << ',' << reco_leak << ',' << conv_x << ',' << conv_y << ',' << conv_z << ',' << pair_angle << ',' << axis_angle << '\n';
  }
  TFile root((out+"/traditional_reco.root").c_str(), "RECREATE"); output.Write(); raw.Write(); calibrated.Write(); leakage.Write(); pairAngle.Write(); axisAngle.Write(); root.Close();
  TCanvas canvas("canvas","canvas",900,700); canvas.SetLogy(); raw.SetLineColor(kBlack); raw.Draw("hist"); canvas.SaveAs((out+"/01_raw_response_test.png").c_str()); canvas.Clear(); calibrated.SetLineColor(kBlue+1); calibrated.Draw("hist"); canvas.SaveAs((out+"/02_log_calibration_response_test.png").c_str()); canvas.Clear(); leakage.SetLineColor(kRed+1); leakage.Draw("hist"); canvas.SaveAs((out+"/03_leakage_linear_response_test.png").c_str()); canvas.SetLogy(false); canvas.Clear(); pairAngle.SetLineColor(kMagenta+1); pairAngle.Draw("hist"); canvas.SaveAs((out+"/04_mc_pair_direction_test.png").c_str()); canvas.Clear(); axisAngle.SetLineColor(kGreen+2); axisAngle.Draw("hist"); canvas.SaveAs((out+"/05_calo_axis_direction_test.png").c_str());
  std::ofstream summary(out+"/numeric_summary.txt"); summary << std::setprecision(12) << "events=" << entries << "\ntrain_active=" << trainActive << "\ncalo_active=" << activeCount << "\nmc_pair_available=" << pairCount << "\ncalo_axis_available=" << axisCount << "\nlog_calibration_a=" << baseA << "\nlog_calibration_b=" << baseB << "\nleakage_coefficients="; for (double c:coefficients) summary << c << ' '; summary << "\n";
  std::cout << "TRADITIONAL_RECO_SUCCESS events=" << entries << "\n";
}
