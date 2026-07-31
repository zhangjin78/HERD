#include <TChain.h>
#include <TFile.h>
#include <TLeaf.h>
#include <TTree.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
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

TLeaf* leaf(TTree& tree, const char* name) {
  auto* result = tree.GetLeaf(name);
  if (!result) throw std::runtime_error(std::string("missing leaf: ") + name);
  return result;
}
}

void extract_sparse_detector_tensors(
    const char* inputPattern,
    const char* outputFile,
    int expectedFiles,
    long long expectedEntries,
    const char* trainJobsText,
    const char* validationJobsText,
    const char* testJobsText) {
  TChain input("events");
  const int files = input.Add(inputPattern);
  if (files != expectedFiles || input.GetEntries() != expectedEntries) {
    std::cerr << "ERROR: input count mismatch files=" << files
              << " events=" << input.GetEntries() << "\n";
    return;
  }
  const auto trainJobs = parseJobs(trainJobsText);
  const auto validationJobs = parseJobs(validationJobsText);
  const auto testJobs = parseJobs(testJobsText);

  TFile output(outputFile, "RECREATE", "HERDOS sparse detector tensors", 6);
  TTree events("events", "One row per simulated gamma event");

  Long64_t event = 0;
  int jobId = -1;
  int split = -1;
  float trueEnergyGeV = 0;
  bool converted = false;
  bool caloActive = false;
  float conversionXcm = std::numeric_limits<float>::quiet_NaN();
  float conversionYcm = std::numeric_limits<float>::quiet_NaN();
  float conversionZcm = std::numeric_limits<float>::quiet_NaN();
  std::vector<unsigned short> caloIx, caloIy, caloIz;
  std::vector<float> caloEdepGeV;
  std::vector<unsigned int> stkCellCode;
  std::vector<float> stkXcm, stkYcm, stkZcm, stkEdepGeV;

  events.Branch("event", &event);
  events.Branch("job_id", &jobId);
  events.Branch("split", &split);
  events.Branch("true_energy_GeV", &trueEnergyGeV);
  events.Branch("converted", &converted);
  events.Branch("calo_active", &caloActive);
  events.Branch("conversion_x_cm", &conversionXcm);
  events.Branch("conversion_y_cm", &conversionYcm);
  events.Branch("conversion_z_cm", &conversionZcm);
  events.Branch("calo_ix", &caloIx);
  events.Branch("calo_iy", &caloIy);
  events.Branch("calo_iz", &caloIz);
  events.Branch("calo_edep_GeV", &caloEdepGeV);
  events.Branch("stk_cell_code", &stkCellCode);
  events.Branch("stk_x_cm", &stkXcm);
  events.Branch("stk_y_cm", &stkYcm);
  events.Branch("stk_z_cm", &stkZcm);
  events.Branch("stk_edep_GeV", &stkEdepGeV);

  int currentTree = -1;
  for (event = 0; event < input.GetEntries(); ++event) {
    input.LoadTree(event);
    input.GetEntry(event);
    if (input.GetTreeNumber() != currentTree) {
      currentTree = input.GetTreeNumber();
      jobId = jobFromFilename(input.GetCurrentFile()->GetName());
      if (trainJobs.count(jobId)) split = 0;
      else if (validationJobs.count(jobId)) split = 1;
      else if (testJobs.count(jobId)) split = 2;
      else split = -1;
    }

    trueEnergyGeV = 0;
    converted = false;
    caloActive = false;
    conversionXcm = conversionYcm = conversionZcm =
        std::numeric_limits<float>::quiet_NaN();
    caloIx.clear(); caloIy.clear(); caloIz.clear(); caloEdepGeV.clear();
    stkCellCode.clear(); stkXcm.clear(); stkYcm.clear(); stkZcm.clear();
    stkEdepGeV.clear();

    auto* mcPdg = leaf(input, "mcparts.pdgID");
    auto* mcTrack = leaf(input, "mcparts.trackID");
    auto* mcParent = leaf(input, "mcparts.parentID");
    auto* mcStatus = leaf(input, "mcparts.simstat");
    auto* mcPx = leaf(input, "mcparts.momentum.x");
    auto* mcPy = leaf(input, "mcparts.momentum.y");
    auto* mcPz = leaf(input, "mcparts.momentum.z");
    auto* mcVx = leaf(input, "mcparts.vertex.x");
    auto* mcVy = leaf(input, "mcparts.vertex.y");
    auto* mcVz = leaf(input, "mcparts.vertex.z");
    int primaryTrack = -1;
    for (int index = 0; index < mcPdg->GetNdata(); ++index) {
      if (static_cast<int>(mcPdg->GetValue(index)) == 22 &&
          (static_cast<unsigned>(mcStatus->GetValue(index)) & 1u)) {
        primaryTrack = static_cast<int>(mcTrack->GetValue(index));
        const double px = mcPx->GetValue(index);
        const double py = mcPy->GetValue(index);
        const double pz = mcPz->GetValue(index);
        trueEnergyGeV = std::sqrt(px*px + py*py + pz*pz);
        break;
      }
    }
    for (int index = 0; index < mcPdg->GetNdata(); ++index) {
      const int pdg = static_cast<int>(mcPdg->GetValue(index));
      const int parent = static_cast<int>(mcParent->GetValue(index));
      const unsigned status = static_cast<unsigned>(mcStatus->GetValue(index));
      if (parent == primaryTrack && (status & 2u) &&
          (pdg == 11 || pdg == -11)) {
        converted = true;
        if (!std::isfinite(conversionZcm)) {
          conversionXcm = mcVx->GetValue(index);
          conversionYcm = mcVy->GetValue(index);
          conversionZcm = mcVz->GetValue(index);
        }
      }
    }

    auto* cIx = leaf(input, "calohits.ix");
    auto* cIy = leaf(input, "calohits.iy");
    auto* cIz = leaf(input, "calohits.iz");
    auto* cEdep = leaf(input, "calohits.edep");
    for (int index = 0; index < cEdep->GetNdata(); ++index) {
      const float energy = cEdep->GetValue(index);
      if (energy <= 0) continue;
      caloActive = true;
      caloIx.push_back(static_cast<unsigned short>(cIx->GetValue(index)));
      caloIy.push_back(static_cast<unsigned short>(cIy->GetValue(index)));
      caloIz.push_back(static_cast<unsigned short>(cIz->GetValue(index)));
      caloEdepGeV.push_back(energy);
    }

    auto* sCode = leaf(input, "stkhits.cellCode");
    auto* sX = leaf(input, "stkhits.pos.x");
    auto* sY = leaf(input, "stkhits.pos.y");
    auto* sZ = leaf(input, "stkhits.pos.z");
    auto* sEdep = leaf(input, "stkhits.edep");
    for (int index = 0; index < sCode->GetNdata(); ++index) {
      stkCellCode.push_back(static_cast<unsigned int>(sCode->GetValue(index)));
      stkXcm.push_back(sX->GetValue(index));
      stkYcm.push_back(sY->GetValue(index));
      stkZcm.push_back(sZ->GetValue(index));
      stkEdepGeV.push_back(sEdep->GetValue(index));
    }
    events.Fill();
  }

  events.Write();
  output.Close();
  std::cout << "SPARSE_TENSOR_EXTRACTION_SUCCESS events=" << expectedEntries
            << " output=" << outputFile << "\n";
}
