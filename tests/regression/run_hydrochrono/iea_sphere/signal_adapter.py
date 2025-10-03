"""Signal adapter for IEA sphere decay tests (heave of body1)."""
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
		# heave from body1 position z
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
		raise KeyError("IEA Sphere: no suitable heave signal found")


