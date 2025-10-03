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
		# Helper to read heave column from a body group name
		def heave_from_body(name: str):
			for key in [
				f"/results/model/bodies/{name}/position",
				f"/results/bodies/{name}/position",
				f"/results/bodies/{name}/z",
			]:
				if key in f:
					arr = np.asarray(f[key][:])
					if arr.ndim == 2 and arr.shape[1] >= 3:
						return arr[:, 2]
					if arr.ndim == 1:
						return arr
			return None
		# Prefer explicit plate/body2 naming
		for candidate in ["body2", "plate", "Plate", "PLATE"]:
			z = heave_from_body(candidate)
			if z is not None:
				return t, z, "Heave (m)"
		# Fallback: float (body1) heave if plate not present
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
		def heave_for(name_candidates):
			for name in name_candidates:
				for key in [
					f"/results/model/bodies/{name}/position",
					f"/results/bodies/{name}/position",
					f"/results/bodies/{name}/z",
				]:
					if key in f:
						arr = np.asarray(f[key][:])
						if arr.ndim == 2 and arr.shape[1] >= 3:
							return arr[:, 2]
						if arr.ndim == 1:
							return arr
			raise KeyError("RM3: heave not found for candidates: " + ",".join(name_candidates))
		float_z = heave_for(["body1", "float", "Float", "FLOAT"]) 
		plate_z = heave_for(["body2", "plate", "Plate", "PLATE"]) 
		return {
			"float": (t, float_z, "Heave (m)"),
			"plate": (t, plate_z, "Heave (m)"),
		}


