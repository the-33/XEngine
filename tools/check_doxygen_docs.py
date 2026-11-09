#!/usr/bin/env python3
"""Validate descriptions, parameters and return values in both XEngine APIs.

Usage (from any directory):
    python tools/build_doxygen.py
    python tools/check_doxygen_docs.py

The game reference checks the public/protected API. The full engine reference
also checks private implementation members. Doxygen XML is authoritative, so
this catches overloads, inherited aliases and enum values the regex-based
checks in a C++ header would easily overlook.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_XML = ROOT / "build" / "docs" / "game" / "xml"
ENGINE_XML = ROOT / "build" / "docs" / "engine" / "xml"


def _description(element: ET.Element) -> str:
    return "".join(
        "".join(part.itertext())
        for tag in ("briefdescription", "detaileddescription")
        if (part := element.find(tag)) is not None
    ).strip()


def _return_required(member: ET.Element, owner: str) -> bool:
    """Ignore void methods, constructors, destructors and non-value qualifiers."""
    if member.get("kind") != "function":
        return False
    name = member.findtext("name") or ""
    class_name = owner.rsplit("::", 1)[-1]
    if name in (class_name, "~" + class_name):
        return False
    signature_type = member.find("type")
    result = "".join(signature_type.itertext()).strip() if signature_type is not None else ""
    return bool(result and result not in {"void", "constexpr", "explicit", "virtual", "static"})


def _contract_issues(member: ET.Element, owner: str) -> list[str]:
    """Report individual undocumented arguments and function return values."""
    if member.get("kind") != "function":
        return []
    name = member.findtext("name") or "?"
    qualified = f"{owner}::{name}"
    documented = {
        (node.text or "").strip()
        for node in member.findall(".//parameterlist[@kind='param']//parametername")
    }
    missing = []
    for parameter in member.findall("param"):
        arg = parameter.findtext("declname") or parameter.findtext("defname")
        if arg and arg not in documented:
            missing.append(f"{qualified}: @param {arg}")
    documented_templates = {
        (node.text or "").strip()
        for node in member.findall(".//parameterlist[@kind='templateparam']//parametername")
    }
    for parameter in member.findall("templateparamlist/param"):
        arg = parameter.findtext("declname") or parameter.findtext("defname")
        if arg and arg not in documented_templates:
            missing.append(f"{qualified}: @tparam {arg}")
    if _return_required(member, owner) and member.find(".//simplesect[@kind='return']") is None:
        missing.append(f"{qualified}: @return")
    return missing


def inspect(xml_dir: Path, *, include_private: bool = False,
            require_contracts: bool = False) -> tuple[int, int, list[str]]:
    """Return member and enum counts plus exact Doxygen documentation gaps.

    @param xml_dir Doxygen XML output directory containing index.xml.
    @param include_private Also audit implementation details in engine XML.
    @param require_contracts Check every named parameter and non-void return.
    @return Number of members, enumerators and list of missing descriptions/tags.
    """
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
            if (require_contracts and definition.get("kind") in
                    {"class", "struct", "union", "namespace"} and
                    not "".join(definition.find("briefdescription").itertext()
                                if definition.find("briefdescription") is not None else []).strip()):
                missing.append(f"{owner}: @brief (type or namespace)")
            for member in definition.findall(".//memberdef"):
                # A friend declaration grants C++ access; it is not a separate
                # callable API member. Doxygen lists it as an undocumented
                # member of the befriending class even when the referenced
                # class itself has complete public documentation.
                if member.get("kind") == "friend":
                    continue
                if not include_private and member.get("prot") not in ("public", "protected"):
                    continue
                members += 1
                member_name = member.findtext("name") or "?"
                if not _description(member):
                    missing.append(f"{owner}::{member_name}")
                elif (require_contracts and not "".join(
                    member.find("briefdescription").itertext()
                    if member.find("briefdescription") is not None else []).strip()):
                    missing.append(f"{owner}::{member_name}: @brief")
                if require_contracts:
                    missing.extend(_contract_issues(member, owner))
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
        "--xml", type=Path,
        help="Check only this XML directory instead of both game and full engine references",
    )
    args = parser.parse_args()
    targets = [(args.xml, args.xml == ENGINE_XML)] if args.xml else [
        (DEFAULT_XML, False), (ENGINE_XML, True)]
    errors = 0
    for xml, include_private in targets:
        try:
            members, enumerators, missing = inspect(
                xml, include_private=include_private, require_contracts=True)
        except (OSError, ET.ParseError) as error:
            print(f"Doxygen API check failed: {error}", file=sys.stderr)
            return 2

        for name in missing:
            print(f"Undocumented: {name}", file=sys.stderr)
        print(
            f"Doxygen {xml.parent.name}: {members} declarations, "
            f"{enumerators} enum values, {len(missing)} missing descriptions or contracts."
        )
        errors += len(missing)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
