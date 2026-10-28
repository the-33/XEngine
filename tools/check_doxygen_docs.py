#!/usr/bin/env python3
"""Check that Doxygen's generated XML documents all exported API members.

Usage (from any directory):
    python tools/build_doxygen.py
    python tools/check_doxygen_docs.py

Only the public/protected declarations and enumerator values *listed in the
generated XML index* are checked. Doxyfile_Games controls which implementation-only
headers and private nested types are outside the published API.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_XML = ROOT / "build" / "docs" / "game" / "xml"


def _description(element: ET.Element) -> str:
    return "".join(
        "".join(part.itertext())
        for tag in ("briefdescription", "detaileddescription")
        if (part := element.find(tag)) is not None
    ).strip()


def inspect(xml_dir: Path) -> tuple[int, int, list[str]]:
    """Return member and enum counts plus exact missing-documentation names."""
    index = xml_dir / "index.xml"
    if not index.is_file():
        raise FileNotFoundError(f"Missing {index}. Run `python tools/build_doxygen.py` first.")

    members = 0
    enumerators = 0
    missing: list[str] = []
    seen_refs: set[str] = set()
    root = ET.parse(index).getroot()
    for compound in root.findall("compound"):
        refid = compound.get("refid")
        if not refid or refid in seen_refs:
            continue
        seen_refs.add(refid)
        compound_file = xml_dir / f"{refid}.xml"
        if not compound_file.is_file():
            raise FileNotFoundError(f"Indexed Doxygen file is missing: {compound_file}")
        for definition in ET.parse(compound_file).getroot().findall("compounddef"):
            owner = definition.findtext("compoundname") or compound.get("kind") or "?"
            for member in definition.findall(".//memberdef"):
                if member.get("prot") not in ("public", "protected"):
                    continue
                members += 1
                member_name = member.findtext("name") or "?"
                if not _description(member):
                    missing.append(f"{owner}::{member_name}")
                for value in member.findall("enumvalue"):
                    enumerators += 1
                    if not _description(value):
                        value_name = value.findtext("name") or "?"
                        missing.append(f"{owner}::{member_name}::{value_name}")
    if not members:
        missing.append("No published API members found (bad or empty Doxygen output)")
    return members, enumerators, missing


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--xml", type=Path, default=DEFAULT_XML,
        help="XML directory (default: build/docs/game/xml; use --xml build/docs/engine/xml for internals)",
    )
    args = parser.parse_args()
    try:
        members, enumerators, missing = inspect(args.xml)
    except (OSError, ET.ParseError) as error:
        print(f"Doxygen API check failed: {error}", file=sys.stderr)
        return 2

    for name in missing:
        print(f"Undocumented: {name}", file=sys.stderr)
    print(
        f"Doxygen API: {members} public/protected declarations, "
        f"{enumerators} enumerator values, {len(missing)} missing descriptions."
    )
    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
