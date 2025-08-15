"""Signal adapter for RM3 decay tests (float and plate heave)."""
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
		# Heave (z) for body1
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
		raise KeyError("RM3: no suitable signal found")


def select_signals(h5_path: Path):
	"""Return multiple named signals for RM3: float and plate heave."""
	with h5py.File(h5_path, "r") as f:
		# time
		for key in ["/results/time/time", "/results/time", "/time"]:
			if key in f:
				t = np.asarray(f[key][:], dtype=float).reshape(-1)
				break
		else:
			raise KeyError("time vector not found")
		# body heave helper
		def heave_for(body: str):
			for key in [
				f"/results/model/bodies/{body}/position",
				f"/results/bodies/{body}/position",
				f"/results/bodies/{body}/z",
			]:
				if key in f:
					arr = np.asarray(f[key][:])
					if arr.ndim == 2 and arr.shape[1] >= 3:
						return arr[:, 2]
					if arr.ndim == 1:
						return arr
			raise KeyError(f"RM3: heave not found for {body}")
		float_z = heave_for("body1")
		plate_z = heave_for("body2")
		return {
			"float": (t, float_z, "Heave (m)"),
			"plate": (t, plate_z, "Heave (m)"),
		}


