# CALO traditional reconstruction

This module is the stage-02, CALO-only reference reconstruction for the
ideal vertical gamma samples.  Energy methods use no conversion/pair flag:
every event with positive CALO deposited energy is eligible.

The MC primary gamma, first-pair vertex, and first-pair electron/positron
momenta are retained for two explicitly truth-assisted direction references:

* `mc_pair_direction`: vector sum of the first e-/e+ momenta;
* `mc_conversion_vertex`: first-pair production vertex.

They are not detector-only observables and must never be presented as a
standalone CALO direction/vertex reconstruction.  `calo_axis_direction` is
the detector-only comparator, fitted from per-layer CALO energy centroids.

Energy baselines, fitted on train jobs only:

1. `raw`: `Ereco = Edep`;
2. `log_calibration`: `log(Etrue)=a+b log(Edep)`;
3. `leakage_linear`: adds last-layer fraction, shower depth, transverse width,
   and CALO boundary distance to the log-energy fit.

Validation jobs are reserved for selecting a later model revision; test jobs
are written out for the final independent evaluation.
