"""Signal adapter for OSWEC decay tests (flap pitch preferred, heave fallback)."""
from pathlib import Path
import numpy as np
import h5py


def select_signal(h5_path: Path):
	# Return (time, signal, y_label)
	with h5py.File(h5_path, "r") as f:
		# time
		for key in ["/results/time/time", "/results/time", "/time"]:
			if key in f:
				t = np.asarray(f[key][:], dtype=float).reshape(-1)
				break
		else:
			raise KeyError("time vector not found")
		# pitch preferred
		if "/results/model/bodies/body1/orientation_xyz" in f:
			arr = np.asarray(f["/results/model/bodies/body1/orientation_xyz"][:])
			if arr.ndim == 2 and arr.shape[1] >= 2:
				return t, arr[:, 1], "Pitch (rad)"
		# quaternion fallback
		if "/results/model/bodies/body1/orientation" in f:
			q = np.asarray(f["/results/model/bodies/body1/orientation"][:])
			if q.ndim == 2 and q.shape[1] == 4:
				w, x, y, z = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
				r20 = 2 * (x * z - w * y)
				r00 = 1 - 2 * (y * y + z * z)
				r10 = 2 * (x * y + w * z)
				theta_y = np.arctan2(-r20, np.hypot(r00, r10))
				return t, theta_y, "Pitch (rad)"
		# heave fallback
		for key in [
			"/results/model/bodies/body1/position",
			"/results/bodies/body1/position",
			"/results/bodies/body1/z",
		]:
			if key in f:
				arr = np.asarray(f[key][:])
				if arr.ndim == 2 and arr.shape[1] >= 3:
					return t, arr[:, 2], "Heave (m)"
				if arr.ndim == 1:
					return t, arr, "Heave (m)"
		raise KeyError("OSWEC: no suitable signal found")


