#!/usr/bin/env python3
import sys
from pathlib import Path
import numpy as np

try:
    import h5py  # type: ignore
except Exception as e:
    print(f"h5py import failed: {e}", file=sys.stderr)
    sys.exit(2)


def find_files(model: str, test_type: str) -> tuple[Path, Path | None]:
    base = Path(__file__).resolve().parent / model / test_type
    out_h5 = (base / "outputs" / "results.still.h5").resolve()
    exp_dir = base / "expected"
    ref = None
    for p in exp_dir.glob("*.h5"):
        ref = p.resolve()
        break
    return out_h5, ref


def first5(arr: np.ndarray) -> str:
    return np.array2string(arr[:5], precision=6, separator=", ")


def probe_pitch(h5_path: Path, body: str) -> tuple[np.ndarray, np.ndarray, str]:
    with h5py.File(h5_path, "r") as f:  # type: ignore
        # time
        for key in ["/results/time/time", "/results/time", "/time"]:
            if key in f:
                t = np.asarray(f[key][:], dtype=float).reshape(-1)
                break
        else:
            raise KeyError("time vector not found")
        # pitch: prefer orientation_xyz Y; fallback quaternion
        for k in [
            f"/results/model/bodies/{body}/orientation_xyz",
            f"/results/bodies/{body}/orientation_xyz",
        ]:
            if k in f:
                arr = np.asarray(f[k][:])
                if arr.ndim == 2 and arr.shape[1] >= 2:
                    return t, arr[:, 1], k
        for k in [
            f"/results/model/bodies/{body}/orientation_quaternion",
            f"/results/bodies/{body}/orientation_quaternion",
        ]:
            if k in f:
                q = np.asarray(f[k][:])
                w, x, y, z = q[:, 0], q[:, 1], q[:, 2], q[:, 3]
                pitch = np.arctan2(2 * (w * y - x * z), 1 - 2 * (y * y + z * z))
                return t, pitch, k
        raise KeyError(f"pitch not found for {body}")


def main() -> int:
    model = sys.argv[1] if len(sys.argv) > 1 else "f3of"
    test_type = sys.argv[2] if len(sys.argv) > 2 else "decay_dt3"
    out_h5, ref_h5 = find_files(model, test_type)
    print("outputs:", out_h5)
    print("reference:", ref_h5)
    if not out_h5.exists():
        print("Missing outputs H5", file=sys.stderr)
        return 1
    t_out, y_out, k_out = probe_pitch(out_h5, "body1")
    print(f"outputs body1 pitch from {k_out} -> N={len(y_out)} first5={first5(y_out)}")
    if ref_h5 and ref_h5.exists():
        t_ref, y_ref, k_ref = probe_pitch(ref_h5, "body1")
        # align to out time for a quick visual sample
        y_ref_on_out = np.interp(t_out, t_ref, y_ref)
        print(f"reference body1 pitch from {k_ref} -> N={len(y_ref)} first5={first5(y_ref)}")
        print(f"reference on outputs timebase first5={first5(y_ref_on_out)}")
        # quick RMSrel
        ref_rms = float(np.sqrt(np.mean(np.square(y_ref_on_out))))
        rmsrel = float(np.sqrt(np.mean(np.square(y_out - y_ref_on_out))) / ref_rms) if ref_rms != 0 else float(np.sqrt(np.mean(np.square(y_out))))
        print(f"RMSrel vs ref: {rmsrel:.6f}")
    else:
        print("No reference H5 found")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


