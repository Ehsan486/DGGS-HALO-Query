#!/usr/bin/env python3
"""Reproducibly acquire and audit the official Calgary experiment datasets.

Downloads are streamed into an atomic temporary file, use stable Socrata
``:id`` ordering, and are accepted only when the feature count and geometry
checks pass.  Existing valid files are reused by default.  A lock file records
the remote update timestamp, byte size, feature count, and SHA-256 digest.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry


@dataclass(frozen=True)
class Dataset:
    name: str
    api_id: str
    catalog_id: str
    output_name: str
    official_page: str
    geometry_field: str


DATASETS = {
    "buildings": Dataset(
        "City of Calgary Buildings", "uc4c-6kbd", "uc4c-6kbd",
        "calgary_buildings.geojson",
        "https://data.calgary.ca/Base-Maps/Buildings/uc4c-6kbd",
        "multipolygon"),
    "streets": Dataset(
        "City of Calgary Street Centreline", "4dx8-rtm5", "4dx8-rtm5",
        "calgary_streets.geojson",
        "https://data.calgary.ca/Transportation-Transit/Street-Centreline/4dx8-rtm5",
        "line"),
    # 7t9h-2z9s is a visualization asset.  Its data source is erra-cqp9.
    "boundary": Dataset(
        "City of Calgary City Boundary", "erra-cqp9", "7t9h-2z9s",
        "calgary_boundary.geojson",
        "https://data.calgary.ca/Base-Maps/City-Boundary/7t9h-2z9s",
        "multipolygon"),
}

HOST = "https://data.calgary.ca"
LICENSE = "https://data.calgary.ca/d/Open-Data-Terms/u45n-7awa"


def session() -> requests.Session:
    retry = Retry(
        total=7, connect=7, read=7, backoff_factor=0.8,
        status_forcelist=(408, 429, 500, 502, 503, 504),
        allowed_methods=frozenset({"GET"}), raise_on_status=False)
    s = requests.Session()
    s.mount("https://", HTTPAdapter(max_retries=retry, pool_connections=4))
    token = os.getenv("SOCRATA_APP_TOKEN")
    if token:
        s.headers["X-App-Token"] = token
    s.headers["User-Agent"] = "DGGS-HALO-Query-reproducibility/1.0"
    return s


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def token_count(path: Path, tokens: tuple[bytes, ...]) -> int:
    """Count JSON tokens without loading a multi-hundred-MB file into RAM."""
    count = 0
    tail = b""
    keep = max(map(len, tokens)) - 1
    with path.open("rb") as stream:
        while True:
            chunk = stream.read(8 * 1024 * 1024)
            if not chunk:
                break
            data = tail + chunk
            cutoff = max(0, len(data) - keep)
            # Count matches by their starting position.  A token may start
            # before cutoff and finish inside the retained tail; counting only
            # data[:cutoff] would incorrectly lose that boundary-spanning
            # match.  Starts at/after cutoff are deferred to the next chunk.
            for token in tokens:
                start = 0
                while True:
                    position = data.find(token, start)
                    if position < 0 or position >= cutoff:
                        break
                    count += 1
                    start = position + len(token)
            tail = data[cutoff:]
    # Matches starting in the final retained tail have not yet been counted.
    count += sum(tail.count(token) for token in tokens)
    return count


def inspect_file(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {"exists": False, "valid": False}
    features = token_count(path, (b'"type":"Feature"', b'"type": "Feature"'))
    null_geometries = token_count(
        path, (b'"geometry":null', b'"geometry": null'))
    result = {
        "exists": True,
        "bytes": path.stat().st_size,
        "features": features,
        "null_geometries": null_geometries,
        "sha256": sha256_file(path),
    }
    result["valid"] = features > 0 and null_geometries == 0
    return result


def remote_info(s: requests.Session, dataset: Dataset) -> dict[str, Any]:
    count_response = s.get(
        f"{HOST}/resource/{dataset.api_id}.json",
        params={"$select": "count(*)"}, timeout=90)
    count_response.raise_for_status()
    expected = int(count_response.json()[0]["count"])
    metadata_response = s.get(
        f"{HOST}/api/views/{dataset.api_id}", timeout=90)
    metadata_response.raise_for_status()
    metadata = metadata_response.json()
    columns = {
        column.get("fieldName"): column.get("dataTypeName")
        for column in metadata.get("columns", [])
    }
    if dataset.geometry_field not in columns:
        raise RuntimeError(
            f"{dataset.api_id} does not expose expected geometry field "
            f"{dataset.geometry_field!r}; columns={sorted(columns)}")
    return {
        "expected_features": expected,
        "rows_updated_at_epoch": metadata.get("rowsUpdatedAt"),
        "rows_updated_at_utc": (
            datetime.fromtimestamp(metadata["rowsUpdatedAt"], timezone.utc)
            .isoformat() if metadata.get("rowsUpdatedAt") else None),
        "columns": columns,
    }


def write_streamed(
    s: requests.Session,
    dataset: Dataset,
    output: Path,
    expected: int,
    page_size: int,
) -> int:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".part")
    written = 0
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as stream:
            stream.write('{"type":"FeatureCollection","features":[')
            offset = 0
            first = True
            while offset < expected:
                response = s.get(
                    f"{HOST}/resource/{dataset.api_id}.geojson",
                    params={
                        "$limit": page_size,
                        "$offset": offset,
                        "$order": ":id",
                    },
                    timeout=240)
                response.raise_for_status()
                page = response.json().get("features", [])
                if not page:
                    break
                for feature in page:
                    if feature.get("geometry") is None:
                        raise RuntimeError(
                            f"{dataset.api_id} returned a null geometry at row {written}")
                    if not first:
                        stream.write(",")
                    stream.write(json.dumps(feature, separators=(",", ":")))
                    first = False
                    written += 1
                offset += len(page)
                print(
                    f"{dataset.api_id}: {written:,}/{expected:,} features",
                    flush=True)
                time.sleep(0.05)
            stream.write(
                '],"crs":{"type":"name","properties":'
                '{"name":"urn:ogc:def:crs:OGC:1.3:CRS84"}}}')
        if written != expected:
            raise RuntimeError(
                f"{dataset.api_id}: expected {expected:,} features, got {written:,}")
        os.replace(temporary, output)
    finally:
        if temporary.exists():
            temporary.unlink()
    return written


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", default="datasets/raw")
    parser.add_argument(
        "--datasets", nargs="*", choices=sorted(DATASETS),
        default=["buildings", "boundary", "streets"])
    parser.add_argument("--page-size", type=int, default=20_000)
    parser.add_argument("--force", action="store_true")
    parser.add_argument(
        "--offline", action="store_true",
        help="audit existing files without contacting the API")
    parser.add_argument(
        "--lock-file", default="datasets/download_lock.json")
    args = parser.parse_args()
    if not 1 <= args.page_size <= 50_000:
        parser.error("--page-size must be in [1, 50000]")

    s = session()
    output_dir = Path(args.output_dir)
    records: list[dict[str, Any]] = []
    for key in args.datasets:
        dataset = DATASETS[key]
        path = output_dir / dataset.output_name
        before = inspect_file(path)
        remote = {} if args.offline else remote_info(s, dataset)
        expected = remote.get("expected_features")
        reusable = bool(
            before.get("valid") and
            (expected is None or before.get("features") == expected))
        action = "reused"
        if args.force or not reusable:
            if args.offline:
                raise RuntimeError(f"{path} failed offline validation: {before}")
            action = "downloaded"
            write_streamed(s, dataset, path, expected, args.page_size)
        after = inspect_file(path)
        if not after.get("valid"):
            raise RuntimeError(f"{path} failed validation: {after}")
        if expected is not None and after["features"] != expected:
            raise RuntimeError(
                f"{path}: local count {after['features']} != remote count {expected}")
        record = {
            "key": key,
            **asdict(dataset),
            "license": LICENSE,
            "action": action,
            "local_path": path.as_posix(),
            **remote,
            **after,
        }
        records.append(record)
        print(
            f"{key}: {action}; {after['features']:,} features; "
            f"sha256={after['sha256'][:16]}...", flush=True)

    lock = {
        "schema": "bafe-calgary-download-lock-v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "stable_paging_order": ":id",
        "datasets": records,
    }
    lock_path = Path(args.lock_file)
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    lock_path.write_text(json.dumps(lock, indent=2), encoding="utf-8")
    print(f"wrote {lock_path}")


if __name__ == "__main__":
    main()
