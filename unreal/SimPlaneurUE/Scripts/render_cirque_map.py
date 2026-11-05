"""Generate a review PNG; never overwrite the game's existing map.

Requires numpy and Pillow. The original 897-square game map has no surviving
canonical renderer. Its local altitude palette and directional hillshade are
therefore fitted to the old height data, with dark contour outliers rejected.
Only the cirque disk is repainted, with a soft edge; all outside pixels retain
their exact original RGB values. This is a reproducible style approximation,
not an assertion that the original cartographic shader has been recovered.

Example, from any working directory:
  python render_cirque_map.py

The result goes to work/site-atlas/cirque-map-preview.png for visual review.
The historical work/site-atlas/massif.bin must remain the pre-cirque baseline.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_EXPORT = ROOT / "work/site-atlas/cirque-export"


def basis(height, north_gradient, east_gradient):
    # Local normal and a continuous altitude palette. Quadratic normal terms
    # approximate the existing map's stronger contrast on steep mountain faces.
    knots = np.array([0.0, 100.0, 180.0, 260.0, 340.0, 460.0, 700.0])
    h = np.clip(height, knots[0], knots[-1])
    weights = np.zeros(h.shape + (len(knots),), dtype=np.float32)
    interval = np.clip(np.searchsorted(knots, h, side="right") - 1, 0, len(knots) - 2)
    fraction = (h - knots[interval]) / (knots[interval + 1] - knots[interval])
    for i in range(len(knots)):
        weights[..., i] = (interval == i) * (1.0 - fraction) + (interval + 1 == i) * fraction
    norm = np.sqrt(1.0 + north_gradient ** 2 + east_gradient ** 2)
    nx, ny, nz = north_gradient / norm, east_gradient / norm, 1.0 / norm
    light = np.stack((np.ones_like(h), nx, ny, nz, nx * ny, nx * nx, ny * ny), axis=-1)
    return (weights[..., :, None] * light[..., None, :]).reshape(h.shape + (-1,))


def fit_palette(before, rgb, north, east, distance, step):
    # Limit fitting to nearby dry terrain, preserving its existing greens and
    # warm limestone tones. Sparse sampling keeps memory and CPU use modest.
    row_gradient, east_gradient = np.gradient(before, step)
    valid = (distance > 1100.0) & (distance < 2900.0) & (before > 0.0)
    sample = np.zeros_like(valid)
    sample[::2, ::2] = True
    valid &= sample
    if np.count_nonzero(valid) < 1000:
        raise ValueError("Not enough unchanged dry terrain to calibrate the map palette")
    features = basis(before[valid], -row_gradient[valid], east_gradient[valid]).astype(np.float64)
    colours = rgb[valid].astype(np.float64)
    keep = np.ones(len(colours), dtype=bool)
    for _ in range(4):
        # Mild regularisation avoids extreme extrapolation on the new cliff.
        a, b = features[keep], colours[keep]
        coefficients = np.linalg.solve(a.T @ a + np.eye(a.shape[1]) * 0.05, a.T @ b)
        residual = colours - features @ coefficients
        luminance_residual = residual.mean(axis=1)
        error = np.sqrt(np.mean(residual ** 2, axis=1))
        keep = (error < np.quantile(error, 0.88)) & (luminance_residual > -12.0)
    error = np.sqrt(np.mean((colours[keep] - features[keep] @ coefficients) ** 2))
    return coefficients, float(error), int(np.count_nonzero(keep))


def contours(height, interval, scale=4):
    """Antialiased marching-squares contours in source-grid pixel coordinates."""
    rows, cols = height.shape
    canvas = Image.new("L", (cols * scale, rows * scale), 0)
    pen = ImageDraw.Draw(canvas)
    corners = ((0, 0), (1, 0), (1, 1), (0, 1))
    first = np.ceil(height.min() / interval) * interval
    for level in np.arange(first, height.max(), interval):
        above = height >= level
        hits = ((above[:-1, :-1] != above[1:, 1:])
                | (above[:-1, 1:] != above[1:, :-1])
                | (above[:-1, :-1] != above[:-1, 1:]))
        for row, col in zip(*np.nonzero(hits)):
            values = (height[row, col], height[row, col + 1],
                      height[row + 1, col + 1], height[row + 1, col])
            points = []
            for a in range(4):
                b = (a + 1) % 4
                if (values[a] >= level) == (values[b] >= level):
                    continue
                f = (level - values[a]) / (values[b] - values[a])
                p, q = corners[a], corners[b]
                points.append(((col + p[0] + f * (q[0] - p[0]) + 0.5) * scale,
                               (row + p[1] + f * (q[1] - p[1]) + 0.5) * scale))
            for a in range(0, len(points) - 1, 2):
                pen.line(points[a:a + 2], fill=115, width=1)
    return np.asarray(canvas.resize((cols, rows), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metadata", type=Path, default=DEFAULT_EXPORT / "metadata.json")
    parser.add_argument("--heights", type=Path, default=DEFAULT_EXPORT / "massif-after-cirque.f32")
    parser.add_argument("--previous-heights", type=Path, default=ROOT / "work/site-atlas/massif.bin")
    parser.add_argument("--source", type=Path, default=ROOT / "unreal/SimPlaneurUE/Content/Slate/Maps/massif.png")
    parser.add_argument("--output", type=Path, default=ROOT / "work/site-atlas/cirque-map-preview.png")
    parser.add_argument("--contour-interval", type=float, default=20.0)
    args = parser.parse_args()
    if args.output.resolve() == args.source.resolve():
        parser.error("Output must be a separate review image, not the original game asset")
    if args.contour_interval <= 0:
        parser.error("Contour interval must be positive")
    metadata = json.loads(args.metadata.read_text(encoding="utf-8"))
    size, step = int(metadata["size"]), float(metadata["step_m"])
    if (size, metadata["north_m"], metadata["west_m"], metadata["span_m"]) != (897, 5600, -5600, 11200):
        raise ValueError("Export bounds do not match the game's massif map")
    source = Image.open(args.source)
    if source.size != (size, size) or source.mode != "RGB":
        raise ValueError("Expected the original 897 x 897 RGB map")
    original = np.asarray(source).copy()
    before = np.fromfile(args.previous_heights, dtype="<f4").reshape(size, size)
    after = np.fromfile(args.heights, dtype=metadata["dtype"]).reshape(size, size)
    if not np.isfinite(before).all() or not np.isfinite(after).all():
        raise ValueError("Height export contains non-finite samples")
    row, col = np.indices((size, size), dtype=np.float32)
    north, east = metadata["north_m"] - row * step, metadata["west_m"] + col * step
    distance = np.hypot(north - metadata["center_x_m"], east - metadata["center_y_m"])
    radius = float(metadata["radius_m"])
    inside = distance < radius
    coefficients, fit_error, fit_samples = fit_palette(before, original, north, east, distance, step)
    rr, cc = np.nonzero(inside)
    bounds = (slice(max(0, rr.min() - 2), min(size, rr.max() + 3)),
              slice(max(0, cc.min() - 2), min(size, cc.max() + 3)))
    row_gradient, east_gradient = np.gradient(after, step)
    patch = basis(after[bounds], -row_gradient[bounds], east_gradient[bounds]) @ coefficients
    contour = contours(after[bounds], args.contour_interval)
    patch = patch * (1.0 - contour[..., None]) + np.array([56.0, 63.0, 45.0]) * contour[..., None]
    # Feather only the peripheral join, where the actual terrain deformation
    # already fades to zero. Outside the disk no operation touches source pixels.
    t = np.clip((distance[bounds] - radius * 0.90) / (radius * 0.10), 0.0, 1.0)
    alpha = (1.0 - t * t * (3.0 - 2.0 * t))[..., None]
    output = original.copy()
    output[bounds] = np.rint(original[bounds] * (1.0 - alpha) + np.clip(patch, 0, 255) * alpha).clip(0, 255).astype(np.uint8)
    output[~inside] = original[~inside]
    assert np.array_equal(output[~inside], original[~inside]), "Pixels outside cirque changed"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(output, "RGB").save(args.output)
    print(json.dumps({"output": str(args.output), "size": [size, size],
                      "palette_fit_rmse_rgb": round(fit_error, 2), "palette_fit_samples": fit_samples,
                      "changed_pixels": int(np.any(output != original, axis=2).sum()),
                      "outside_pixels_identical": True, "game_asset_modified": False}, indent=2))


if __name__ == "__main__":
    main()
