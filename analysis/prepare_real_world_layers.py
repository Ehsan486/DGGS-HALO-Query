#!/usr/bin/env python3
"""Prepare deterministic real-world point distributions for halo experiments.

This module does not claim polygon coverage.  It produces three clearly named
spatial distributions from the official Calgary layers: building interior
representative points, samples along street centrelines, and samples along the
municipal boundary.  These inputs test traversal under clustered, network-like,
and long-boundary distributions while keeping mapping semantics identical.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

import geopandas as gpd
import numpy as np
import pandas as pd
from shapely.geometry import LineString, MultiLineString


METRIC_CRS = "EPSG:3776"
WGS84 = "EPSG:4326"
PROFILES = {
    "quick": {"building_features": 10_000, "street_points": 20_000},
    "paper": {"building_features": 100_000, "street_points": 250_000},
    "full": {"building_features": 0, "street_points": 1_000_000},
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def select_evenly(frame: gpd.GeoDataFrame, maximum: int) -> gpd.GeoDataFrame:
    if maximum <= 0 or len(frame) <= maximum:
        return frame
    indices = np.linspace(0, len(frame) - 1, maximum, dtype=np.int64)
    return frame.iloc[np.unique(indices)]


def repair(frame: gpd.GeoDataFrame) -> tuple[gpd.GeoDataFrame, int]:
    frame = frame[frame.geometry.notna() & ~frame.geometry.is_empty].copy()
    invalid = ~frame.geometry.is_valid
    count = int(invalid.sum())
    if count:
        frame.loc[invalid, "geometry"] = frame.loc[invalid].geometry.make_valid()
    frame = frame[frame.geometry.notna() & ~frame.geometry.is_empty]
    return frame, count


def iter_lines(geometry) -> Iterable[LineString]:
    if isinstance(geometry, LineString):
        yield geometry
    elif isinstance(geometry, MultiLineString):
        yield from geometry.geoms
    elif hasattr(geometry, "geoms"):
        for part in geometry.geoms:
            yield from iter_lines(part)


def sample_lines(
    geometries: Iterable,
    spacing: float,
    maximum: int = 0,
) -> list:
    points = []
    for geometry in geometries:
        for line in iter_lines(geometry):
            if line.is_empty:
                continue
            samples = max(1, int(math.ceil(line.length / spacing)))
            for index in range(samples + 1):
                points.append(line.interpolate(min(line.length, index * spacing)))
                if maximum and len(points) >= maximum:
                    return points
    return points


def write_points(points, crs: str, output: Path) -> dict:
    series = gpd.GeoSeries(points, crs=crs).to_crs(WGS84)
    frame = pd.DataFrame({
        "lon": np.round(series.x.to_numpy(), 10),
        "lat": np.round(series.y.to_numpy(), 10),
    })
    frame = (
        frame.drop_duplicates()
        .sort_values(["lon", "lat"], kind="mergesort")
        .reset_index(drop=True))
    output.parent.mkdir(parents=True, exist_ok=True)
    frame.to_csv(output, index=False, lineterminator="\n")
    return {
        "path": output.as_posix(),
        "points": int(len(frame)),
        "sha256": sha256_file(output),
        "bounds_wgs84": {
            "min_lon": float(frame.lon.min()), "min_lat": float(frame.lat.min()),
            "max_lon": float(frame.lon.max()), "max_lat": float(frame.lat.max()),
        } if len(frame) else None,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw-dir", default="datasets/raw")
    parser.add_argument(
        "--output-dir", default="datasets/processed/real_world_layers")
    parser.add_argument("--profile", choices=PROFILES, default="paper")
    parser.add_argument("--street-spacing-m", type=float, default=20.0)
    parser.add_argument("--boundary-spacing-m", type=float, default=10.0)
    args = parser.parse_args()
    if args.street_spacing_m <= 0 or args.boundary_spacing_m <= 0:
        parser.error("sampling spacings must be positive")

    raw = Path(args.raw_dir)
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    settings = PROFILES[args.profile]
    paths = {
        "buildings": raw / "calgary_buildings.geojson",
        "streets": raw / "calgary_streets.geojson",
        "boundary": raw / "calgary_boundary.geojson",
    }
    for path in paths.values():
        if not path.exists():
            raise FileNotFoundError(path)

    results = {}
    validation = {}

    buildings, repaired = repair(gpd.read_file(paths["buildings"]))
    validation["buildings"] = {
        "source_features": int(len(buildings)), "repaired_geometries": repaired}
    buildings = select_evenly(buildings, settings["building_features"])
    buildings_metric = buildings.to_crs(METRIC_CRS)
    # representative_point is guaranteed to lie within each valid footprint.
    results["buildings"] = write_points(
        buildings_metric.geometry.representative_point(), METRIC_CRS,
        out / "calgary_building_representative_points.csv")

    streets, repaired = repair(gpd.read_file(paths["streets"]))
    validation["streets"] = {
        "source_features": int(len(streets)), "repaired_geometries": repaired}
    streets_metric = streets.to_crs(METRIC_CRS)
    street_points = sample_lines(
        streets_metric.geometry, args.street_spacing_m,
        settings["street_points"])
    results["streets"] = write_points(
        street_points, METRIC_CRS, out / "calgary_street_samples.csv")

    boundary, repaired = repair(gpd.read_file(paths["boundary"]))
    validation["boundary"] = {
        "source_features": int(len(boundary)), "repaired_geometries": repaired}
    boundary_metric = boundary.to_crs(METRIC_CRS)
    boundary_points = sample_lines(
        boundary_metric.geometry.boundary, args.boundary_spacing_m)
    results["boundary"] = write_points(
        boundary_points, METRIC_CRS, out / "calgary_boundary_samples.csv")

    metadata = {
        "schema": "bafe-real-world-layer-samples-v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "profile": args.profile,
        "coverage_semantics": (
            "point distributions for traversal experiments; not polygon covers"),
        "source_crs": WGS84,
        "metric_processing_crs": METRIC_CRS,
        "street_spacing_m": args.street_spacing_m,
        "boundary_spacing_m": args.boundary_spacing_m,
        "source_files": {
            key: {"path": path.as_posix(), "sha256": sha256_file(path)}
            for key, path in paths.items()
        },
        "validation": validation,
        "outputs": results,
    }
    metadata_path = out / "metadata.json"
    metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
