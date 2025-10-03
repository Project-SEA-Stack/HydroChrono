"""
Utility to compute OSWEC flap CG position for a decay test at a given pitch angle
about the hinge (simple UI: angle only).

USAGE (command line)
  - python demos/oswec/compute_flap_pose.py --angle-deg 10

The script prints YAML-ready blocks for two equivalent methods to rotate about the hinge:

It prints:
  - Rotated CG (world) using fixed geometry
  - A minimal YAML block (CG-frame) you can paste into the model

Copy one of the printed blocks into your YAML (e.g., `demos/yaml/oswec/oswec_neutral.model.yaml`) under `model.bodies[0]`.
"""

import argparse
import math
from typing import Tuple


def rotate_vector_around_axis(v: Tuple[float, float, float],
                              axis: Tuple[float, float, float],
                              angle_deg: float) -> Tuple[float, float, float]:
    """Rotate v around 'axis' by angle (degrees) using Rodrigues' formula."""
    vx, vy, vz = v
    ax, ay, az = axis
    # normalize axis
    n = math.sqrt(ax * ax + ay * ay + az * az)
    if n == 0:
        raise ValueError("Axis vector must be non-zero")
    ax, ay, az = ax / n, ay / n, az / n
    theta = math.radians(angle_deg)
    c = math.cos(theta)
    s = math.sin(theta)
    # v_rot = v*c + (k x v)*s + k*(k·v)*(1-c)
    kxv = (
        ay * vz - az * vy,
        az * vx - ax * vz,
        ax * vy - ay * vx,
    )
    kdotv = ax * vx + ay * vy + az * vz
    v_rot = (
        vx * c + kxv[0] * s + ax * kdotv * (1 - c),
        vy * c + kxv[1] * s + ay * kdotv * (1 - c),
        vz * c + kxv[2] * s + az * kdotv * (1 - c),
    )
    return v_rot


def add(a: Tuple[float, float, float], b: Tuple[float, float, float]) -> Tuple[float, float, float]:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def fmt_vec(v: Tuple[float, float, float]) -> str:
    return "[%.6f, %.6f, %.6f]" % (v[0], v[1], v[2])


def main() -> None:
    ap = argparse.ArgumentParser(description="Compute OSWEC flap rotated CG for a given pitch angle.")
    ap.add_argument("--angle-deg", type=float, default=10.0,
                    help="Pitch angle in degrees about the hinge axis (default: 10)")
    args = ap.parse_args()

    # Fixed OSWEC geometry (matches demo inputs)
    hinge = (0.0, 0.0, -8.9)
    hinge_to_cg = (0.0, 0.0, 5.0)
    axis = (0.0, 1.0, 0.0)
    angle = args.angle_deg
    angle_rad = math.radians(angle)

    rotated_hinge_to_cg = rotate_vector_around_axis(hinge_to_cg, axis, angle)
    cg_world_rotated = add(hinge, rotated_hinge_to_cg)

    print()
    print("=== Rotated CG (world) ===")
    print(fmt_vec(cg_world_rotated))

    print()
    print("=== YAML (CG-frame minimal) ===")
    print("location: %s    # rotated COM (world)" % fmt_vec(cg_world_rotated))
    print("orientation: [0.0, %.6f, 0.0]    # radians" % angle_rad)
    print("com:")
    print("  location: [0.0, 0.0, 0.0]")
    print("  orientation: [0.0, 0.0, 0.0]")


if __name__ == "__main__":
    main()


