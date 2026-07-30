#include <TFile.h>
#include <TLeaf.h>
#include <TTree.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
double leafValue(TTree* tree, const std::string& name, int index) {
  auto* leaf = tree->GetLeaf(name.c_str());
  return leaf ? leaf->GetValue(index) : 0.0;
}

int leafCount(TTree* tree, const std::string& name) {
  auto* leaf = tree->GetLeaf(name.c_str());
  return leaf ? leaf->GetNdata() : 0;
}

struct Particle {
  int track = 0;
  int parent = 0;
  int pdg = 0;
  unsigned simstat = 0;
  double px = 0, py = 0, pz = 0;
  double x = 0, y = 0, z = 0;
};

void printDirection(const char* label, const Particle& p,
                    double gx, double gy, double gz) {
  const double pmag = std::sqrt(p.px*p.px + p.py*p.py + p.pz*p.pz);
  const double gmag = std::sqrt(gx*gx + gy*gy + gz*gz);
  const double ux = p.px / pmag;
  const double uy = p.py / pmag;
  const double uz = p.pz / pmag;
  double cosine = (p.px*gx + p.py*gy + p.pz*gz) / (pmag*gmag);
  if (cosine > 1) cosine = 1;
  if (cosine < -1) cosine = -1;
  const double angle = std::acos(cosine) * 180.0 / M_PI;
  const double phi = std::atan2(p.py, p.px) * 180.0 / M_PI;

  std::cout << label
            << " track=" << p.track
            << " parent=" << p.parent
            << " pdg=" << p.pdg
            << " p=(" << p.px << "," << p.py << "," << p.pz << ") GeV"
            << " |p|=" << pmag << " GeV"
            << " u=(" << ux << "," << uy << "," << uz << ")"
            << " angle_to_gamma=" << angle << " deg"
            << " phi=" << phi << " deg\n";
}
}  // namespace

void read_first_gamma_conversion(
    const char* filename =
        "/herdfs/user/zhangjin0101/HERD/results/v2025a-test/my_gamma_with_truth.root") {
  TFile file(filename, "READ");
  TTree* tree = nullptr;
  file.GetObject("events", tree);
  if (!tree) {
    std::cerr << "ERROR: events tree not found\n";
    return;
  }

  std::cout << std::setprecision(9);
  for (Long64_t event = 0; event < tree->GetEntries(); ++event) {
    tree->GetEntry(event);
    const int n = leafCount(tree, "mcparts.pdgID");
    std::vector<Particle> particles;
    std::map<int, Particle> byTrack;

    for (int i = 0; i < n; ++i) {
      Particle p;
      p.track = static_cast<int>(leafValue(tree, "mcparts.trackID", i));
      p.parent = static_cast<int>(leafValue(tree, "mcparts.parentID", i));
      p.pdg = static_cast<int>(leafValue(tree, "mcparts.pdgID", i));
      p.simstat = static_cast<unsigned>(leafValue(tree, "mcparts.simstat", i));
      p.px = leafValue(tree, "mcparts.momentum.x", i);
      p.py = leafValue(tree, "mcparts.momentum.y", i);
      p.pz = leafValue(tree, "mcparts.momentum.z", i);
      p.x = leafValue(tree, "mcparts.vertex.x", i);
      p.y = leafValue(tree, "mcparts.vertex.y", i);
      p.z = leafValue(tree, "mcparts.vertex.z", i);
      particles.push_back(p);
      byTrack[p.track] = p;
    }

    std::cout << "EVENT " << event << " mcparts=" << n << "\n";
    for (const auto& gamma : particles) {
      if (gamma.pdg != 22 || (gamma.simstat & 1u) == 0u) continue;

      std::vector<Particle> daughters;
      for (const auto& p : particles) {
        if ((p.simstat & 2u) != 0u && p.parent == gamma.track &&
            (p.pdg == 11 || p.pdg == -11)) {
          daughters.push_back(p);
        }
      }

      if (daughters.size() != 2) {
        std::cout << "PRIMARY_GAMMA track=" << gamma.track
                  << " direct_conversion_daughters=" << daughters.size() << "\n";
        continue;
      }

      std::cout << "FIRST_CONVERSION parent_gamma_track=" << gamma.track
                << " vertex=(" << daughters[0].x << ","
                << daughters[0].y << "," << daughters[0].z << ") cm\n";
      const double gmag =
          std::sqrt(gamma.px*gamma.px + gamma.py*gamma.py + gamma.pz*gamma.pz);
      std::cout << "gamma p=(" << gamma.px << "," << gamma.py << ","
                << gamma.pz << ") GeV |p|=" << gmag << " GeV\n";
      for (const auto& p : daughters) {
        printDirection(p.pdg == 11 ? "electron" : "positron",
                       p, gamma.px, gamma.py, gamma.pz);
      }
    }
  }
}
