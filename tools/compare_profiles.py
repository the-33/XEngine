#!/usr/bin/env python3
"""Compare XEngine frame-time captures generated with Engine.profileCsv=true.

Only Python's standard library is required. Prefer comparable gameplay workloads,
Release builds, and vsync=false when measuring raw throughput.
"""

import argparse
import csv
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROFILE_FILES = ("xengine_profile_single.csv", "xengine_profile_parallel.csv")

METRICS = (
    ("frame_ms", "Frame wall time"),
    ("input_ms", "Input"),
    ("fixed_ms", "Fixed updates"),
    ("physics_ms", "Physics"),
    ("collision_ms", "Collisions"),
    ("collision_snapshot_ms", "Collider snapshots"),
    ("collision_candidates_ms", "Collision candidates"),
    ("collision_merge_ms", "Collision merge"),
    ("update_ms", "Gameplay update"),
    ("render_present_ms", "Render + present"),
)

RENDER_METRICS = (
    ("render_record_ms", "Render recording"),
    ("render_prepare_ms", "Render CPU preparation"),
    ("render_submit_ms", "SDL command submission"),
    ("render_only_present_ms", "SDL present / VSync"),
    ("render_worker_wait_ms", "Render worker wait"),
    ("render_background_ms", "Background and clear"),
    ("render_world_ms", "Scene rendering incl. debug"),
    ("render_debug_ms", "Debug overlay rendering"),
    ("render_ui_ms", "UI rendering"),
    ("render_sprite_submit_ms", "World sprite SDL draws"),
)

OPTIONAL_RENDER_COUNTS = (
    "render_path_reason", "render_eligible_previous", "render_eligible_current",
    "render_recorded_total", "render_immediate_culled", "render_background_draws", "render_world_draws",
    "render_debug_draws", "render_ui_draws", "render_tint_cache_hits", "render_tint_updates",
    "render_prepared_commands", "render_culled_commands", "render_workers",
)
PATH_NAMES = {0: "disabled", 1: "no_workers", 2: "below_threshold", 3: "parallel"}


def read_profile(path: Path, warmup: int) -> list[dict[str, float]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        required = {key for key, _ in METRICS} | {"narrowphase_tests", "fixed_steps"}
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            missing = required - set(reader.fieldnames or ())
            raise ValueError(f"{path}: missing profile columns: {', '.join(sorted(missing))}")
        supported = required | {key for key, _ in RENDER_METRICS if key in reader.fieldnames} | {
            key for key in OPTIONAL_RENDER_COUNTS if key in reader.fieldnames
        }
        rows = [{name: float(value) for name, value in row.items()
                 if name in supported and value != ""} for row in reader]
    if len(rows) <= warmup:
        raise ValueError(f"{path}: {len(rows)} frames; warmup {warmup} leaves no measurements")
    return rows[warmup:]


def percentile(values: list[float], q: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("No samples")
    x = (len(ordered) - 1) * q
    lo = int(x)
    hi = min(lo + 1, len(ordered) - 1)
    return ordered[lo] + (ordered[hi] - ordered[lo]) * (x - lo)


def report(single: list[dict[str, float]], parallel: list[dict[str, float]]) -> str:
    result = ["XEngine frame-time comparison (milliseconds; lower is better)",
              f"Frames: single={len(single)}, parallel={len(parallel)}",
              "", f"{'Metric':<23} {'Single p50':>11} {'Parallel p50':>13} {'Single p95':>11} {'Parallel p95':>13} {'p50 change':>12}"]
    for key, label in METRICS + RENDER_METRICS:
        if key not in single[0] or key not in parallel[0]:
            continue
        left = [r[key] for r in single]
        right = [r[key] for r in parallel]
        p50a, p50b = percentile(left, .50), percentile(right, .50)
        p95a, p95b = percentile(left, .95), percentile(right, .95)
        change = ((p50a / p50b) - 1) * 100 if p50b > 0 else 0.0
        result.append(f"{label:<23} {p50a:>11.3f} {p50b:>13.3f} {p95a:>11.3f} {p95b:>13.3f} {change:>+11.1f}%")
    for field in ("narrowphase_tests", "fixed_steps"):
        a = statistics.mean(r[field] for r in single)
        b = statistics.mean(r[field] for r in parallel)
        result.append(f"Average {field}: single={a:.1f}, parallel={b:.1f}")
    if "render_path_reason" in parallel[0]:
        result.append("")
        result.append("Adaptive render route, parallel capture:")
        for code, label in PATH_NAMES.items():
            n = sum(int(row.get("render_path_reason", -1)) == code for row in parallel)
            result.append(f"  {label:<18}: {n:>7} frames ({100 * n / len(parallel):.1f}%)")
        if not any(int(row.get("render_path_reason", 0)) == 3 for row in parallel):
            result.append("WARNING: parallel render preparation never activated in this capture.")
    if all("render_background_draws" in sample[0] for sample in (single, parallel)):
        result.append("")
        result.append("Mean SDL draw submissions / frame (single | parallel):")
        for field, label in (("render_background_draws", "background"),
                             ("render_world_draws", "world"),
                             ("render_debug_draws", "debug"),
                             ("render_ui_draws", "UI"),
                             ("render_eligible_current", "eligible scene commands"),
                             ("render_culled_commands", "prepared CPU-culled commands"),
                             ("render_immediate_culled", "immediate CPU-culled commands"),
                             ("render_tint_cache_hits", "redundant tint sets avoided")):
            if field not in single[0] or field not in parallel[0]:
                continue
            a, b = (statistics.mean(row[field] for row in rows)
                    for rows in (single, parallel))
            result.append(f"  {label:<30} {a:>8.1f} | {b:>8.1f}")
    result.extend(("", "WARNING: VSync can cap both results; compare the same workload, resolution and hardware.",
                   "Parallel collision uses 192+ colliders; parallel render preparation uses the configured threshold (default 1536).",
                   "Render preparation includes worker waiting; do not add them as independent phases."))
    return "\n".join(result)



def resolve_game(game_arg: str) -> Path:
    """Resolve a game by folder name or by directory, independently of cwd."""
    supplied = Path(game_arg).expanduser()
    candidates = (
        supplied,
    ) if supplied.is_absolute() else (
        ROOT / "games" / supplied,
        ROOT / supplied,
        supplied,
    )
    for candidate in candidates:
        if candidate.is_dir() and (candidate / "config.json").is_file():
            return candidate.resolve()

    # Windows users frequently type a game name with different capitalization.
    # Only apply this fallback to simple names, never to arbitrary relative paths.
    if len(supplied.parts) == 1 and not supplied.is_absolute():
        games_dir = ROOT / "games"
        matches = [folder for folder in games_dir.iterdir()
                   if folder.is_dir() and folder.name.casefold() == game_arg.casefold()
                   and (folder / "config.json").is_file()] if games_dir.is_dir() else []
        if len(matches) == 1:
            return matches[0].resolve()

    available = sorted(p.name for p in (ROOT / "games").iterdir()
                       if p.is_dir() and (p / "config.json").is_file()) if (ROOT / "games").is_dir() else []
    raise ValueError(f"Game not found: {game_arg!r}. Expected a directory under games/ "
                     f"containing config.json. Available games: {', '.join(available) or '(none)'}")


def discover_profiles(game: Path) -> tuple[Path, Path, str | None]:
    """Find both captures only within this game's runtime output directories."""
    candidates: dict[str, list[Path]] = {name: [] for name in PROFILE_FILES}
    roots = (game, game / "assets")
    for folder in roots:
        for name in PROFILE_FILES:
            candidate = folder / name
            if candidate.is_file():
                candidates[name].append(candidate)

    # Release launches use dist/<platform>/<version> as their working directory.
    # The archive files themselves are not searched or unpacked.
    dist = game / "dist"
    if dist.is_dir():
        for name in PROFILE_FILES:
            for candidate in dist.glob(f"*/*/{name}"):
                if candidate.is_file():
                    candidates[name].append(candidate)

    missing = [name for name, paths in candidates.items() if not paths]
    if missing:
        checked = f"{game / 'assets'}, {game}, {dist / '<platform>' / '<version>'}"
        raise ValueError(f"{game.name}: missing {', '.join(missing)}. "
                         f"Enable Engine.profileCsv, run both modes and exit normally. "
                         f"Searched: {checked}")

    single_candidates = candidates[PROFILE_FILES[0]]
    parallel_candidates = candidates[PROFILE_FILES[1]]
    # Prefer a pair captured from the same output directory, so a recent run
    # on another OS/target does not silently mix incomparable captures.
    pairs = [(s, p) for s in single_candidates for p in parallel_candidates
             if s.parent == p.parent]
    if pairs:
        single, parallel = max(pairs, key=lambda pair: (
            min(pair[0].stat().st_mtime_ns, pair[1].stat().st_mtime_ns),
            max(pair[0].stat().st_mtime_ns, pair[1].stat().st_mtime_ns),
            str(pair[0].parent)))
        return single, parallel, None

    single = max(single_candidates, key=lambda p: (p.stat().st_mtime_ns, str(p)))
    parallel = max(parallel_candidates, key=lambda p: (p.stat().st_mtime_ns, str(p)))
    return single, parallel, ("WARNING: profiles are in different directories; "
                              "confirm they use the same platform, settings and workload.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", metavar="PATH", type=str,
                        help="Game name/path, or two explicit CSV paths (single parallel)")
    parser.add_argument("--game", metavar="NAME_OR_PATH",
                        help="Discover both profiles within games/<game>/ (or a game directory)")
    parser.add_argument("--warmup", type=int, default=120, help="Discard initial frames (default: 120)")
    args = parser.parse_args()
    if args.warmup < 0:
        parser.error("--warmup must not be negative")
    if args.game is not None and args.paths:
        parser.error("Specify either --game or positional arguments, not both")
    if args.game is None and len(args.paths) not in (1, 2):
        parser.error("Specify a game (name/path) or two explicit CSV paths (single parallel)")
    try:
        if args.game is not None or len(args.paths) == 1:
            game = resolve_game(args.game if args.game is not None else args.paths[0])
            single_path, parallel_path, warning = discover_profiles(game)
            print(f"[GAME] {game.name}")
            if warning:
                print(warning)
        else:
            single_path, parallel_path = map(Path, args.paths)
        print(f"[SINGLE]   {single_path}")
        print(f"[PARALLEL] {parallel_path}")
        print()
        print(report(read_profile(single_path, args.warmup),
                     read_profile(parallel_path, args.warmup)))
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
