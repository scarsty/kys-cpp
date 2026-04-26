from __future__ import annotations

import argparse
import io
import json
import re
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from PIL import Image, ImageDraw, ImageFont, ImageTk


APP_TITLE = "KYS EFT Viewer"
STATE_PATH = Path.home() / ".kys_cpp_eft_viewer.json"
PREVIEW_WIDTH = 220
PREVIEW_HEIGHT = 180
TILE_WIDTH = 248
TILE_HEIGHT = 240
ANIMATION_INTERVAL_MS = 60
GROUP_PATTERN = re.compile(r"^eft(\d+)$", re.IGNORECASE)
INDEX_KA_NAME = "index.ka"
INDEX_TXT_NAME = "index.txt"
TEXT_NUMBER_PATTERN = re.compile(r"[+-]?\d+")
IMAGE_EXTENSIONS = (".webp", ".png")
RESAMPLING = getattr(Image, "Resampling", Image)


@dataclass
class RenderFrame:
    index: int
    dx: int
    dy: int
    image: Image.Image


@dataclass
class EftAnimation:
    eft_id: int
    source_path: Path
    slot_count: int
    preview_frames: list[Image.Image]
    populated_slots: int
    error: str | None = None

    @property
    def label(self) -> str:
        base = f"eft{self.eft_id:03d}"
        if self.error:
            return f"{base} | error"
        return f"{base} | {self.slot_count} slots | {self.populated_slots} populated"


class ArchiveAccessor:
    def __init__(self, source_path: Path) -> None:
        self.source_path = source_path
        self._zip_file: zipfile.ZipFile | None = None
        self._name_map: dict[str, str | Path] = {}
        self._root_names: list[str] = []

        if source_path.is_dir():
            for child in source_path.iterdir():
                if child.is_file():
                    self._root_names.append(child.name)
                    self._name_map[child.name.lower()] = child
        else:
            self._zip_file = zipfile.ZipFile(source_path)
            for info in self._zip_file.infolist():
                if info.is_dir():
                    continue
                normalized_name = info.filename.replace("\\", "/")
                if "/" not in normalized_name:
                    self._root_names.append(normalized_name)
                    self._name_map.setdefault(normalized_name.lower(), info.filename)

    def close(self) -> None:
        if self._zip_file is not None:
            self._zip_file.close()

    def root_names(self) -> list[str]:
        return list(self._root_names)

    def read_bytes(self, name: str) -> bytes | None:
        target = self._name_map.get(name.lower())
        if target is None:
            return None
        if isinstance(target, Path):
            return target.read_bytes()
        assert self._zip_file is not None
        return self._zip_file.read(target)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Browse and preview EFT animation groups from KYS resource assets.")
    parser.add_argument("directory", nargs="?", help="Game root, resource root, or eft directory to load.")
    return parser.parse_args()


def load_state() -> dict[str, str]:
    if not STATE_PATH.exists():
        return {}
    try:
        return json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def save_state(directory: Path) -> None:
    payload = {"directory": str(directory)}
    try:
        STATE_PATH.write_text(json.dumps(payload, ensure_ascii=True, indent=2), encoding="utf-8")
    except OSError:
        pass


def resolve_eft_root(selected: Path) -> Path:
    selected = selected.expanduser().resolve()
    direct_candidates = [
        selected,
        selected / "eft",
        selected / "resource" / "eft",
        selected / "game" / "resource" / "eft",
    ]
    for candidate in direct_candidates:
        if candidate.is_dir() and contains_eft_groups(candidate):
            return candidate

    for child in selected.iterdir() if selected.is_dir() else []:
        nested = child / "resource" / "eft"
        if nested.is_dir() and contains_eft_groups(nested):
            return nested

    raise FileNotFoundError(f"No eft directory found under {selected}")


def contains_eft_groups(directory: Path) -> bool:
    for child in directory.iterdir():
        if child.is_dir() and GROUP_PATTERN.match(child.name):
            return True
        if child.is_file() and child.suffix.lower() == ".zip" and GROUP_PATTERN.match(child.stem):
            return True
    return False


def discover_groups(eft_root: Path) -> list[tuple[int, Path]]:
    groups: list[tuple[int, Path]] = []
    for child in eft_root.iterdir():
        match = None
        if child.is_dir():
            match = GROUP_PATTERN.match(child.name)
        elif child.is_file() and child.suffix.lower() == ".zip":
            match = GROUP_PATTERN.match(child.stem)
        if match is None:
            continue
        groups.append((int(match.group(1)), child))
    groups.sort(key=lambda item: item[0])
    return groups


def find_max_texture_index(accessor: ArchiveAccessor) -> int:
    max_index = -1
    for name in accessor.root_names():
        if Path(name).suffix.lower() not in {".png", ".webp"}:
            continue
        numbers = TEXT_NUMBER_PATTERN.findall(Path(name).stem)
        if numbers:
            max_index = max(max_index, int(numbers[0]))
    return max_index


def parse_index_txt(payload: bytes) -> list[tuple[int, int, int]]:
    entries: list[tuple[int, int, int]] = []
    for line in payload.decode("utf-8-sig", errors="replace").splitlines():
        numbers = [int(number) for number in TEXT_NUMBER_PATTERN.findall(line)]
        if len(numbers) < 3:
            continue
        entries.append((numbers[0], numbers[1], numbers[2]))
    return entries


def load_offsets(accessor: ArchiveAccessor) -> list[tuple[int, int]]:
    max_texture_index = find_max_texture_index(accessor)

    raw_txt = accessor.read_bytes(INDEX_TXT_NAME)
    if raw_txt is not None:
        entries = parse_index_txt(raw_txt)
        max_index = max((index for index, _, _ in entries), default=-1)
        slot_count = max(max_texture_index, max_index) + 1
        offsets = [(0, 0)] * max(slot_count, 0)
        for index, dx, dy in entries:
            if 0 <= index < slot_count:
                offsets[index] = (dx, dy)
        return offsets

    raw_ka = accessor.read_bytes(INDEX_KA_NAME)
    if raw_ka is None:
        raise FileNotFoundError("index.txt or index.ka is missing")
    if len(raw_ka) % 4 != 0:
        raise ValueError(f"index.ka size {len(raw_ka)} is not divisible by 4")

    count = len(raw_ka) // 4
    offsets = [struct.unpack_from("<hh", raw_ka, index * 4) for index in range(count)]
    slot_count = max(max_texture_index, count - 1) + 1
    if len(offsets) < slot_count:
        offsets.extend([(0, 0)] * (slot_count - len(offsets)))
    return offsets


def load_image_variants(accessor: ArchiveAccessor, index: int) -> list[Image.Image]:
    for extension in IMAGE_EXTENSIONS:
        direct = accessor.read_bytes(f"{index}{extension}")
        if direct is not None:
            return [decode_image(direct)]

    for extension in IMAGE_EXTENSIONS:
        variants: list[Image.Image] = []
        sub_index = 0
        while True:
            payload = accessor.read_bytes(f"{index}_{sub_index}{extension}")
            if payload is None:
                break
            variants.append(decode_image(payload))
            sub_index += 1
        if variants:
            return variants

    return []


def decode_image(payload: bytes) -> Image.Image:
    with Image.open(io.BytesIO(payload)) as image:
        return image.convert("RGBA")


def build_preview_frames(render_frames: list[RenderFrame]) -> list[Image.Image]:
    if not render_frames:
        return [make_placeholder_image("No image frames")]

    min_x = min(-frame.dx for frame in render_frames)
    min_y = min(-frame.dy for frame in render_frames)
    max_x = max(-frame.dx + frame.image.width for frame in render_frames)
    max_y = max(-frame.dy + frame.image.height for frame in render_frames)

    width = max(1, max_x - min_x)
    height = max(1, max_y - min_y)
    scale = min(PREVIEW_WIDTH / width, PREVIEW_HEIGHT / height, 1.0)

    previews: list[Image.Image] = []
    for frame in render_frames:
        canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        canvas.alpha_composite(frame.image, (-frame.dx - min_x, -frame.dy - min_y))
        if scale != 1.0:
            new_size = (
                max(1, round(width * scale)),
                max(1, round(height * scale)),
            )
            canvas = canvas.resize(new_size, RESAMPLING.NEAREST)
        previews.append(canvas)
    return previews


def make_placeholder_image(text: str) -> Image.Image:
    image = Image.new("RGBA", (PREVIEW_WIDTH, PREVIEW_HEIGHT), (28, 32, 38, 255))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    text_box = draw.multiline_textbbox((0, 0), text, font=font, spacing=4)
    text_width = text_box[2] - text_box[0]
    text_height = text_box[3] - text_box[1]
    x = (PREVIEW_WIDTH - text_width) // 2
    y = (PREVIEW_HEIGHT - text_height) // 2
    draw.rounded_rectangle((10, 10, PREVIEW_WIDTH - 10, PREVIEW_HEIGHT - 10), radius=14, outline=(87, 97, 110, 255), width=2)
    draw.multiline_text((x, y), text, font=font, fill=(220, 226, 232, 255), align="center", spacing=4)
    return image


def load_animation(eft_id: int, source_path: Path) -> EftAnimation:
    accessor = ArchiveAccessor(source_path)
    try:
        offsets = load_offsets(accessor)
        render_frames: list[RenderFrame] = []
        populated_slots = 0
        for index, (dx, dy) in enumerate(offsets):
            variants = load_image_variants(accessor, index)
            if not variants:
                continue
            populated_slots += 1
            for image in variants:
                render_frames.append(RenderFrame(index=index, dx=dx, dy=dy, image=image))
        return EftAnimation(
            eft_id=eft_id,
            source_path=source_path,
            slot_count=len(offsets),
            preview_frames=build_preview_frames(render_frames),
            populated_slots=populated_slots,
        )
    finally:
        accessor.close()


class EftTile:
    def __init__(self, parent: ttk.Frame, animation: EftAnimation) -> None:
        self.animation = animation
        self.frame = ttk.Frame(parent, padding=10, style="Tile.TFrame")
        self.preview = tk.Canvas(
            self.frame,
            width=PREVIEW_WIDTH,
            height=PREVIEW_HEIGHT,
            background="#11161c",
            highlightthickness=0,
            bd=0,
        )
        self.preview.grid(row=0, column=0, sticky="nsew")
        self.title = ttk.Label(self.frame, text=self.animation.label, style="TileTitle.TLabel", anchor="center", justify="center")
        self.title.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        self.source = ttk.Label(
            self.frame,
            text=self.animation.source_path.name,
            style="TileMeta.TLabel",
            anchor="center",
            justify="center",
        )
        self.source.grid(row=2, column=0, sticky="ew", pady=(2, 0))

        self.photos = [ImageTk.PhotoImage(frame) for frame in animation.preview_frames]
        self.photo_index = 0
        initial = self.photos[0]
        self.image_id = self.preview.create_image(PREVIEW_WIDTH // 2, PREVIEW_HEIGHT // 2, image=initial)

    def grid(self, row: int, column: int) -> None:
        self.frame.grid(row=row, column=column, padx=10, pady=10, sticky="n")

    def advance(self) -> None:
        if len(self.photos) <= 1:
            return
        self.photo_index = (self.photo_index + 1) % len(self.photos)
        self.preview.itemconfigure(self.image_id, image=self.photos[self.photo_index])

    def destroy(self) -> None:
        self.frame.destroy()


class EftViewerApp:
    def __init__(self, root: tk.Tk, initial_directory: Path | None) -> None:
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1320x860")
        self.root.minsize(900, 640)

        self.selected_directory: Path | None = None
        self.eft_root: Path | None = None
        self.tiles: list[EftTile] = []
        self.column_count = 1

        self.path_var = tk.StringVar(value="No directory selected")
        self.status_var = tk.StringVar(value="Choose a directory to load EFT groups.")

        self._configure_style()
        self._build_layout()
        self._bind_scroll()

        if initial_directory is not None:
            self.load_directory(initial_directory, show_errors=True)

        self.root.after(ANIMATION_INTERVAL_MS, self._tick_animation)

    def _configure_style(self) -> None:
        self.root.configure(background="#0d1117")
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("App.TFrame", background="#0d1117")
        style.configure("Toolbar.TFrame", background="#111827")
        style.configure("Tile.TFrame", background="#16202b", relief="flat")
        style.configure("Path.TLabel", background="#111827", foreground="#e5edf5", font=("Segoe UI", 10, "bold"))
        style.configure("Status.TLabel", background="#111827", foreground="#a7b4c3", font=("Segoe UI", 9))
        style.configure("TileTitle.TLabel", background="#16202b", foreground="#f3f6fb", font=("Segoe UI", 10, "bold"))
        style.configure("TileMeta.TLabel", background="#16202b", foreground="#9fb0c0", font=("Consolas", 9))
        style.configure("Action.TButton", font=("Segoe UI", 10, "bold"))

    def _build_layout(self) -> None:
        outer = ttk.Frame(self.root, style="App.TFrame", padding=12)
        outer.pack(fill="both", expand=True)

        toolbar = ttk.Frame(outer, style="Toolbar.TFrame", padding=(14, 12))
        toolbar.pack(fill="x")
        toolbar.columnconfigure(1, weight=1)

        choose_button = ttk.Button(toolbar, text="Choose Directory", style="Action.TButton", command=self.choose_directory)
        choose_button.grid(row=0, column=0, padx=(0, 10), sticky="w")

        path_label = ttk.Label(toolbar, textvariable=self.path_var, style="Path.TLabel")
        path_label.grid(row=0, column=1, sticky="ew")

        reload_button = ttk.Button(toolbar, text="Reload", command=self.reload_current)
        reload_button.grid(row=0, column=2, padx=(10, 0), sticky="e")

        status_label = ttk.Label(toolbar, textvariable=self.status_var, style="Status.TLabel")
        status_label.grid(row=1, column=0, columnspan=3, sticky="w", pady=(6, 0))

        body = ttk.Frame(outer, style="App.TFrame")
        body.pack(fill="both", expand=True, pady=(12, 0))
        body.columnconfigure(0, weight=1)
        body.rowconfigure(0, weight=1)

        self.canvas = tk.Canvas(body, background="#0d1117", highlightthickness=0, bd=0)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        scrollbar = ttk.Scrollbar(body, orient="vertical", command=self.canvas.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.canvas.configure(yscrollcommand=scrollbar.set)

        self.content = ttk.Frame(self.canvas, style="App.TFrame")
        self.window_id = self.canvas.create_window((0, 0), window=self.content, anchor="nw")
        self.content.bind("<Configure>", self._on_content_configure)
        self.canvas.bind("<Configure>", self._on_canvas_configure)

    def _bind_scroll(self) -> None:
        self.root.bind_all("<MouseWheel>", self._on_mouse_wheel, add=True)

    def _on_mouse_wheel(self, event: tk.Event) -> None:
        if event.delta == 0:
            return
        self.canvas.yview_scroll(int(-event.delta / 120), "units")

    def _on_content_configure(self, _event: tk.Event) -> None:
        self.canvas.configure(scrollregion=self.canvas.bbox("all"))

    def _on_canvas_configure(self, event: tk.Event) -> None:
        self.canvas.itemconfigure(self.window_id, width=event.width)
        new_columns = max(1, event.width // TILE_WIDTH)
        if new_columns != self.column_count:
            self.column_count = new_columns
            self._layout_tiles()

    def _layout_tiles(self) -> None:
        for index, tile in enumerate(self.tiles):
            row = index // self.column_count
            column = index % self.column_count
            tile.grid(row=row, column=column)

    def _clear_tiles(self) -> None:
        for tile in self.tiles:
            tile.destroy()
        self.tiles.clear()

    def choose_directory(self) -> None:
        initial = self.selected_directory or Path.cwd()
        selected = filedialog.askdirectory(title="Choose a game/resource/eft directory", initialdir=str(initial))
        if not selected:
            return
        self.load_directory(Path(selected), show_errors=True)

    def reload_current(self) -> None:
        if self.selected_directory is None:
            self.choose_directory()
            return
        self.load_directory(self.selected_directory, show_errors=True)

    def load_directory(self, selected_directory: Path, show_errors: bool) -> None:
        try:
            eft_root = resolve_eft_root(selected_directory)
            groups = discover_groups(eft_root)
            if not groups:
                raise FileNotFoundError(f"No EFT groups found in {eft_root}")
        except Exception as exc:
            self.status_var.set(f"Load failed: {exc}")
            if show_errors:
                messagebox.showerror(APP_TITLE, str(exc))
            return

        self.selected_directory = selected_directory.expanduser().resolve()
        self.eft_root = eft_root
        save_state(self.selected_directory)
        self.path_var.set(str(self.selected_directory))
        self.status_var.set(f"Loading {len(groups)} EFT groups from {eft_root}...")
        self.root.update_idletasks()

        self._clear_tiles()
        failed = 0
        for eft_id, group_path in groups:
            try:
                animation = load_animation(eft_id, group_path)
            except Exception as exc:
                failed += 1
                animation = EftAnimation(
                    eft_id=eft_id,
                    source_path=group_path,
                    slot_count=0,
                    populated_slots=0,
                    preview_frames=[make_placeholder_image(str(exc))],
                    error=str(exc),
                )
            self.tiles.append(EftTile(self.content, animation))

        self._layout_tiles()
        self.canvas.yview_moveto(0.0)
        success_count = len(groups) - failed
        if failed:
            self.status_var.set(f"Loaded {success_count} EFT groups from {eft_root}. {failed} failed to decode.")
        else:
            self.status_var.set(f"Loaded {success_count} EFT groups from {eft_root}.")

    def _tick_animation(self) -> None:
        for tile in self.tiles:
            tile.advance()
        self.root.after(ANIMATION_INTERVAL_MS, self._tick_animation)


def determine_initial_directory(argument: str | None) -> Path | None:
    if argument:
        return Path(argument)

    state = load_state()
    directory = state.get("directory")
    if not directory:
        return None
    path = Path(directory)
    if not path.exists():
        return None
    return path


def main() -> int:
    args = parse_args()
    root = tk.Tk()
    app = EftViewerApp(root, determine_initial_directory(args.directory))
    if app.selected_directory is None:
        app.choose_directory()
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())