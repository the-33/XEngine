#!/usr/bin/env python3
"""Create gameplay C++ files from the engine's canonical, editable templates."""

import argparse
import re
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SCAFFOLDS = ROOT / "tools" / "templates" / "AssetScaffolds"
SPRITE_SOURCES = ROOT / "tools" / "templates" / "BlankGame" / "assets" / "textures"
CPP_KEYWORDS = frozenset("""
alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t
char16_t char32_t class compl concept const consteval constexpr constinit const_cast
continue co_await co_return co_yield decltype default delete do double dynamic_cast
else enum explicit export extern false float for friend goto if inline int long
mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected
public register reinterpret_cast requires return short signed sizeof static
static_assert static_cast struct switch template this thread_local throw true try
typedef typeid typename union unsigned using virtual void volatile wchar_t while xor
xor_eq
""".split())
WINDOWS_RESERVED = frozenset({"CON", "PRN", "AUX", "NUL"}
                           | {f"COM{i}" for i in range(1, 10)}
                           | {f"LPT{i}" for i in range(1, 10)})
KIND_TEMPLATES = {
    "behaviour": ("Behaviour.hpp.tpl", "scripts"),
    "object": ("Object.hpp.tpl", "gameobjects"),
    "scene": ("Scene.hpp.tpl", "scenes"),
    "inputmap": ("InputMap.json.tpl", "input"),
    "inputscheme": ("InputScheme.json.tpl", "input"),
    "inputprofile": ("InputProfile.json.tpl", "input"),
}
KIND_ALIASES = {"behavior": "behaviour", "script": "behaviour"}


class GenerationError(Exception):
    """A user-correctable input or filesystem problem."""


def available_sprites() -> tuple[str, ...]:
    """Discover available PNGs from BlankGame, not the current game's assets."""
    return tuple(sorted(p.stem for p in SPRITE_SOURCES.glob("*.png") if p.is_file()))


def check_identifier(name: str) -> str:
    if (not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name)
            or name in CPP_KEYWORDS or name.upper() in WINDOWS_RESERVED):
        raise GenerationError(
            f"Invalid C++ name: {name!r}. Use letters, digits and underscores, "
            "starting with a letter; reserved keywords are not allowed."
        )
    return name


def locate_game(project_dir: str | None, game: str | None) -> Path:
    if project_dir and game:
        raise GenerationError("Use either --project-dir or --game, not both")
    if game:
        if (not game or game in {".", ".."} or "/" in game
                or "\\" in game or ":" in game):
            raise GenerationError("--game must be the name of a folder in games/")
        directory = (ROOT / "games" / game).resolve()
        if directory.parent != (ROOT / "games").resolve():
            raise GenerationError("--game must refer to a direct child of games/")
    elif project_dir:
        directory = Path(project_dir).resolve()
    else:
        directory = Path.cwd().resolve()
        for candidate in (directory, *directory.parents):
            if (candidate / "config.json").is_file() and (candidate / "assets").is_dir():
                directory = candidate
                break
    if not (directory / "config.json").is_file() or not (directory / "assets").is_dir():
        raise GenerationError(
            f"Not an XEngine game: {directory}. Run from the game folder "
            "or specify --project-dir / --game."
        )
    return directory


def asset_folder(assets: Path, relative: str) -> Path:
    """Keep all output inside assets, including when subdirectories are symlinks."""
    path = relative.replace("\\", "/")
    if (not path or path.startswith("/") or ":" in path
            or any(part == ".." for part in path.split("/"))):
        raise GenerationError("--dir must be a relative path inside assets/")
    destination = (assets / path).resolve()
    if not destination.is_relative_to(assets):
        raise GenerationError("--dir must not escape the game's assets/ folder")
    return destination


def find_behaviour(assets: Path, name: str) -> str:
    matches = [path for path in assets.rglob(f"{name}.hpp")
               if path.is_file() and path.resolve().is_relative_to(assets)]
    if not matches:
        raise GenerationError(
            f"Behaviour {name}.hpp not found under assets/. "
            f"Create it first: new_asset behaviour {name}"
        )
    if len(matches) > 1:
        raise GenerationError(
            f"Multiple {name}.hpp files under assets/; rename or remove the ambiguity"
        )
    return matches[0].relative_to(assets).as_posix()


def render_file(kind: str, name: str, sprite: str | None,
                collider: str, behaviour_include: str | None) -> str:
    template = SCAFFOLDS / KIND_TEMPLATES[kind][0]
    if not template.is_file():
        raise GenerationError(f"Canonical template missing: {template}")
    content = template.read_text(encoding="utf-8")
    if kind.startswith("input"):
        return content.replace("{{NAME}}", name)
    includes = (f'#include "{behaviour_include}"\n' if behaviour_include else "")
    sprite_code = ""
    if sprite:
        sprite_code = (
            f'    if (const Texture* texture = Assets->LoadTexture("textures/{sprite}.png")) {{\n'
            "        auto* renderer = object->AddComponent<SpriteRenderer>();\n"
            "        renderer->sprite = texture;\n"
            "    }\n"
        )
    collider_code = ""
    if collider != "none":
        shape = {"box": "Box", "circle": "Circle", "capsule": "Capsule"}[collider]
        collider_code = (
            "    auto* collider = object->AddComponent<Collider2D>();\n"
            f"    collider->shape = Collider2D::Shape::{shape};\n"
        )
    behaviour_code = (f"    object->AddComponent<{Path(behaviour_include).stem}>();\n"
                      if behaviour_include else "")
    substitutions = {
        "{{NAME}}": name,
        "{{EXTRA_INCLUDES}}": includes,
        "{{SPRITE_SETUP}}": sprite_code,
        "{{COLLIDER_SETUP}}": collider_code,
        "{{BEHAVIOUR_SETUP}}": behaviour_code,
    }
    for key, value in substitutions.items():
        content = content.replace(key, value)
    return content


def create_asset(project: Path, kind: str, name: str, *, directory: str = ".",
                 sprite: str | None = None, collider: str = "none",
                 behaviour: str | None = None) -> tuple[Path, Path | None]:
    kind = KIND_ALIASES.get(kind, kind)
    if kind not in KIND_TEMPLATES:
        raise GenerationError(f"Unknown asset type: {kind}")
    check_identifier(name)
    if kind != "object" and (sprite or collider != "none" or behaviour):
        raise GenerationError("--sprite, --collider and --behaviour only apply to objects")
    if collider not in {"none", "box", "circle", "capsule"}:
        raise GenerationError("--collider must be none, box, circle or capsule")
    if sprite and sprite not in available_sprites():
        raise GenerationError(
            f"Unknown default sprite {sprite!r}. Available: {', '.join(available_sprites())}"
        )
    if behaviour:
        check_identifier(behaviour)

    assets = (project / "assets").resolve(strict=True)
    if directory == "." and kind.startswith("input"):
        directory = KIND_TEMPLATES[kind][1]
    destination = asset_folder(assets, directory)
    extension = ".json" if kind.startswith("input") else ".hpp"
    target = destination / f"{name}{extension}"
    if target.exists() or target.is_symlink():
        raise GenerationError(f"File already exists (not overwritten): {target}")

    behaviour_include = find_behaviour(assets, behaviour) if behaviour else None
    code = render_file(kind, name, sprite, collider, behaviour_include)

    copied_sprite = None
    sprite_target = None
    if sprite:
        sprite_source = SPRITE_SOURCES / f"{sprite}.png"
        if not sprite_source.is_file():
            raise GenerationError(f"Default sprite missing from BlankGame: {sprite_source}")
        textures = (assets / "textures").resolve()
        if not textures.is_relative_to(assets):
            raise GenerationError("The textures directory points outside assets/")
        sprite_target = textures / f"{sprite}.png"
        if sprite_target.is_symlink():
            raise GenerationError(f"Refusing to use symlinked texture: {sprite_target}")

    destination.mkdir(parents=True, exist_ok=True)
    if sprite_target is not None and not sprite_target.exists():
        sprite_target.parent.mkdir(parents=True, exist_ok=True)
        with sprite_target.open("xb") as dst, (SPRITE_SOURCES / f"{sprite}.png").open("rb") as src:
            shutil.copyfileobj(src, dst)
        copied_sprite = sprite_target
    with target.open("x", encoding="utf-8", newline="\n") as dst:
        dst.write(code)
    return target, copied_sprite


def prompt_creation(options: argparse.Namespace) -> None:
    if options.kind is None:
        value = input("Type [object / scene / behaviour / inputmap / inputscheme / inputprofile] (object): ").strip().lower()
        options.kind = value or "object"
    if options.name is None:
        options.name = input("C++ name (e.g. Player): ").strip()
    kind = KIND_ALIASES.get(options.kind, options.kind)
    if kind == "object" and not options.sprite:
        sprites = ", ".join(available_sprites())
        selected = input(f"Default sprite [{sprites}] (Enter for none): ").strip()
        options.sprite = selected or None
    if kind == "object" and options.collider == "none":
        options.collider = input("Collider [box / circle / capsule] (Enter for none): ").strip() or "none"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Create a Scene, GameObject, Behaviour or JSON input asset in assets/."
    )
    parser.add_argument("kind", nargs="?", help="object | scene | behaviour | inputmap | inputscheme | inputprofile")
    parser.add_argument("name", nargs="?", help="C++ identifier and file basename")
    parser.add_argument("--project-dir", help="Path to an XEngine game (default: current game)")
    parser.add_argument("--game", help="Game directory name under games/")
    parser.add_argument("--dir", default=".", help="Directory relative to assets/ (default: root)")
    parser.add_argument("--sprite", help="Attach a SpriteRenderer using a BlankGame PNG")
    parser.add_argument("--collider", choices=("none", "box", "circle", "capsule"), default="none")
    parser.add_argument("--behaviour", help="Attach an existing Behaviour class")
    parser.add_argument("--list-sprites", action="store_true", help="List canonical BlankGame PNGs")
    options = parser.parse_args(argv)
    if options.list_sprites:
        print("Default sprites: " + ", ".join(available_sprites()))
        return 0
    if not options.kind or not options.name:
        if not sys.stdin.isatty():
            parser.error("kind and name are required outside interactive mode")
        prompt_creation(options)
    try:
        game = locate_game(options.project_dir, options.game)
        created, copied = create_asset(
            game, options.kind.lower(), options.name, directory=options.dir,
            sprite=options.sprite, collider=options.collider, behaviour=options.behaviour,
        )
    except (GenerationError, OSError) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 1
    print(f"[OK] Created {created.relative_to(game)}")
    if copied:
        print(f"[ASSET] Restored {copied.relative_to(game)} from BlankGame template")
    if options.kind.lower() in {"scene"}:
        print("[NEXT] Register the new builder in src/InitCallback.hpp")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
