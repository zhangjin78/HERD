"""PyTorch model interfaces for HERDOS gamma reconstruction."""


def require_torch():
    try:
        import torch
        import torch.nn as nn
    except ImportError as error:
        raise RuntimeError(
            "PyTorch is not installed. Create the documented analysis environment first."
        ) from error
    return torch, nn


def build_feature_multitask_model(input_features, hidden=(128, 128, 64)):
    torch, nn = require_torch()

    class FeatureMultiTaskModel(nn.Module):
        def __init__(self):
            super().__init__()
            layers = []
            width = input_features
            for next_width in hidden:
                layers.extend((nn.Linear(width, next_width), nn.ReLU()))
                width = next_width
            self.encoder = nn.Sequential(*layers)
            self.energy = nn.Linear(width, 1)
            self.conversion = nn.Linear(width, 1)
            self.vertex = nn.Linear(width, 3)
            self.direction = nn.Linear(width, 3)

        def forward(self, features):
            latent = self.encoder(features)
            direction = self.direction(latent)
            direction = direction / direction.norm(dim=1, keepdim=True).clamp_min(1e-8)
            return {
                "log_energy": self.energy(latent).squeeze(1),
                "conversion_logit": self.conversion(latent).squeeze(1),
                "vertex_cm": self.vertex(latent),
                "direction": direction,
            }

    return FeatureMultiTaskModel()


def build_dense_calo_multitask_model(grid_size=21):
    """Define the stage-three CALO-only 3D-CNN interface.

    Input shape is [batch, 1, iz, iy, ix]. Training is intentionally gated on
    production of validated per-cell tensors.
    """
    torch, nn = require_torch()

    class DenseCaloMultiTaskModel(nn.Module):
        def __init__(self):
            super().__init__()
            self.encoder = nn.Sequential(
                nn.Conv3d(1, 16, 3, padding=1),
                nn.ReLU(),
                nn.MaxPool3d(2),
                nn.Conv3d(16, 32, 3, padding=1),
                nn.ReLU(),
                nn.AdaptiveAvgPool3d(1),
                nn.Flatten(),
            )
            self.energy = nn.Linear(32, 1)
            self.conversion = nn.Linear(32, 1)
            self.vertex = nn.Linear(32, 3)
            self.direction = nn.Linear(32, 3)

        def forward(self, cells):
            if tuple(cells.shape[2:]) != (grid_size, grid_size, grid_size):
                raise ValueError(
                    f"expected {grid_size}^3 CALO grid, got {tuple(cells.shape[2:])}"
                )
            latent = self.encoder(cells)
            direction = self.direction(latent)
            direction = direction / direction.norm(dim=1, keepdim=True).clamp_min(1e-8)
            return {
                "log_energy": self.energy(latent).squeeze(1),
                "conversion_logit": self.conversion(latent).squeeze(1),
                "vertex_cm": self.vertex(latent),
                "direction": direction,
            }

    return DenseCaloMultiTaskModel()
