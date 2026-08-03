#include <TFile.h>
#include <TLeaf.h>
#include <TTree.h>

#include <fstream>
#include <iostream>
#include <map>
#include <tuple>

// Export one event's aggregated CALO energy deposits.  This is deliberately
// a small interchange file for plotting; raw ROOT data remain untouched.
void export_calo_event_csv(const char* inputFile, Long64_t entry, const char* outputFile) {
  TFile input(inputFile, "READ");
  auto* events = dynamic_cast<TTree*>(input.Get("events"));
  if (!events || entry < 0 || entry >= events->GetEntries()) {
    std::cerr << "ERROR: invalid events tree or entry\n"; return;
  }
  auto* ix = events->GetLeaf("calohits.ix");
  auto* iy = events->GetLeaf("calohits.iy");
  auto* iz = events->GetLeaf("calohits.iz");
  auto* edep = events->GetLeaf("calohits.edep");
  if (!ix || !iy || !iz || !edep) { std::cerr << "ERROR: CALO branches missing\n"; return; }
  events->GetEntry(entry);
  std::map<std::tuple<int,int,int>, double> cells;
  for (int i = 0; i < edep->GetNdata(); ++i) {
    if (edep->GetValue(i) <= 0.) continue;
    cells[{static_cast<int>(ix->GetValue(i)), static_cast<int>(iy->GetValue(i)),
           static_cast<int>(iz->GetValue(i))}] += edep->GetValue(i);
  }
  std::ofstream out(outputFile);
  out << "ix,iy,iz,edep_GeV\n";
  for (const auto& [key, energy] : cells) {
    const auto& [x, y, z] = key;
    out << x << ',' << y << ',' << z << ',' << energy << '\n';
  }
  std::cout << "Exported " << cells.size() << " fired cells to " << outputFile << '\n';
}
