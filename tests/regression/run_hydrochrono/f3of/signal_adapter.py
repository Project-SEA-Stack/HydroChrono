"""Signal adapter for F3OF tests (DT1/DT2/DT3).

Mappings (per current test requirements):
- DT1: base surge (body1 position X)
- DT2: base pitch (body2 orientation about Y)
- DT3: flap pitches (fore=body2 pitch, aft=body3 pitch)
"""
from pathlib import Path
import numpy as np
import h5py

def select_signals(h5_path: Path):
	"""Return named signal(s) for the F3OF tests.

	- DT1: {"surge": body1 X}
	- DT2: {"pitch": body2 Y-rotation}
	- DT3: {"fore": body2 pitch, "aft": body3 pitch}
	"""
	with h5py.File(h5_path, "r") as f:
		# infer test folder name (e.g., decay_dt1/decay_dt2/decay_dt3)
		test_folder = h5_path.parent.parent.name.lower()
		# time
		for key in ["/results/time/time", "/results/time", "/time"]:
			if key in f:
				t = np.asarray(f[key][:], dtype=float).reshape(-1)
				break
		else:
			raise KeyError("time vector not found")
		# helpers
		def pitch_for(body: str):
			for k in [
				f"/results/model/bodies/{body}/orientation_xyz",
				f"/results/bodies/{body}/orientation_xyz",
			]:
				if k in f:
					arr = np.asarray(f[k][:])
					if arr.ndim == 2 and arr.shape[1] >= 2:
						return arr[:, 1]
			for k in [
				f"/results/model/bodies/{body}/orientation_quaternion",
				f"/results/bodies/{body}/orientation_quaternion",
			]:
				if k in f:
					q = np.asarray(f[k][:])
					w, x, y, z = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
					return np.arctan2(2 * (w * y - x * z), 1 - 2 * (y * y + z * z))
			raise KeyError(f"pitch not found for {body}")
		def surge_for(body: str):
			for k in [
				f"/results/model/bodies/{body}/position",
				f"/results/bodies/{body}/position",
				f"/results/bodies/{body}/x",
			]:
				if k in f:
					arr = np.asarray(f[k][:])
					if arr.ndim == 2 and arr.shape[1] >= 1:
						return arr[:, 0]
					if arr.ndim == 1:
						return arr
			raise KeyError(f"surge not found for {body}")
		# Branch per test
		if "decay_dt3" in test_folder:
			return {
				"fore": (t, pitch_for("body2"), "Pitch (rad)"),
				"aft": (t, pitch_for("body3"), "Pitch (rad)"),
			}
		if "decay_dt2" in test_folder:
			return {
				"pitch": (t, pitch_for("body2"), "Pitch (rad)"),
			}
		# default to DT1 behavior (surge body1)
		return {
			"surge": (t, surge_for("body1"), "Surge (m)"),
		}


def select_signal(h5_path: Path):
	"""Return the primary signal for an F3OF test based on folder (DT1/DT2/DT3).

	Returns (time, value, y_label).
	"""
	# Choose signal based on test folder: DT1 -> surge (x), DT2 -> pitch (y-rotation)
	test_folder = h5_path.parent.parent.name.lower()
	with h5py.File(h5_path, "r") as f:
		# time
		for key in ["/results/time/time", "/results/time", "/time"]:
			if key in f:
				t = np.asarray(f[key][:], dtype=float).reshape(-1)
				break
		else:
			raise KeyError("time vector not found")
		if "decay_dt2" in test_folder:
			# body2 pitch from orientation_xyz (Y component)
			for key in [
				"/results/model/bodies/body2/orientation_xyz",
				"/results/bodies/body2/orientation_xyz",
			]:
				if key in f:
					arr = np.asarray(f[key][:])
					if arr.ndim == 2 and arr.shape[1] >= 2:
						return t, arr[:, 1], "Pitch (rad)"
			# fallback via quaternion if needed
			for key in [
				"/results/model/bodies/body2/orientation_quaternion",
				"/results/bodies/body2/orientation_quaternion",
			]:
				if key in f:
					q = np.asarray(f[key][:])
					# convert quaternion [w,x,y,z] then compute y-angle
					w, x, y, z = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
					pitch = np.arctan2(2 * (w * y - x * z), 1 - 2 * (y * y + z * z))
					return t, pitch, "Pitch (rad)"
			raise KeyError("F3OF DT2: base (body2) pitch signal not found")
		if "decay_dt3" in test_folder:
			# Primary: fore flap pitch (body2)
			for key in [
				"/results/model/bodies/body2/orientation_xyz",
				"/results/bodies/body2/orientation_xyz",
			]:
				if key in f:
					arr = np.asarray(f[key][:])
					if arr.ndim == 2 and arr.shape[1] >= 2:
						return t, arr[:, 1], "Pitch (rad)"
			# fallback via quaternion
			for key in [
				"/results/model/bodies/body2/orientation_quaternion",
				"/results/bodies/body2/orientation_quaternion",
			]:
				if key in f:
					q = np.asarray(f[key][:])
					w, x, y, z = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
					pitch = np.arctan2(2 * (w * y - x * z), 1 - 2 * (y * y + z * z))
					return t, pitch, "Pitch (rad)"
			raise KeyError("F3OF DT3: fore (body2) pitch not found")
		# DT1 or others: body1 position x (surge)
		for key in [
			"/results/model/bodies/body1/position",
			"/results/bodies/body1/position",
		]:
			if key in f:
				arr = np.asarray(f[key][:])
				if arr.ndim == 2 and arr.shape[1] >= 1:
					return t, arr[:, 0], "Surge (m)"
		raise KeyError("F3OF: base surge signal not found")


