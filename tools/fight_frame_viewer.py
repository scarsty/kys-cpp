from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from PIL import Image, ImageDraw, ImageFont, ImageTk

from eft_viewer import ArchiveAccessor, load_image_variants, load_offsets


APP_TITLE = "KYS Fight Frame Viewer"
ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FIGHT_ROOT = ROOT / "work" / "game-dev" / "resource" / "fight"
DEFAULT_CONFIG_ROOT = ROOT / "work" / "game-dev" / "config"
FIGHT_STYLE_COUNT = 5
FIGHT_DIRECTION_COUNT = 4
DEFAULT_FIGHT_ID = 605
DEFAULT_INTERVAL_MS = 100
TEXT_NUMBER_PATTERN = re.compile(r"[+-]?\d+")
ROLE_LINE_PATTERN = re.compile(r"^\s*-\s*(\d+)(?:\s*#\s*(.+?))?\s*$")
FIGHT_PACK_PATTERN = re.compile(r"^fight(\d+)\.zip$", re.IGNORECASE)
RESAMPLING = getattr(Image, "Resampling", Image)
VIEW_MODE_SINGLE = "single"
VIEW_MODE_ACTION = "action"
VIEW_MODE_LABELS = [
    (VIEW_MODE_SINGLE, "單幀"),
    (VIEW_MODE_ACTION, "動作全幀"),
]
VIEW_MODE_BY_LABEL = {label: value for value, label in VIEW_MODE_LABELS}

STYLE_LABELS = [
    "動作 0",
    "動作 1",
    "動作 2",
    "動作 3",
    "動作 4",
]

DIRECTION_LABELS = [
    "0 右上",
    "1 右下",
    "2 左上",
    "3 左下",
]


@dataclass(frozen=True)
class RenderFrame:
    index: int
    dx: int
    dy: int
    width: int
    height: int


@dataclass(frozen=True)
class FramePlacement:
    frame_index: int
    x: int
    y: int


@dataclass(frozen=True)
class OffsetLayout:
    width: int
    height: int
    origin_x: int
    origin_y: int
    placements: list[FramePlacement]


@dataclass(frozen=True)
class RoleMetadata:
    role_id: int
    name: str


@dataclass(frozen=True)
class RoleOption:
    role_id: int
    name: str
    path: Path

    @property
    def label(self) -> str:
        return f"{self.role_id} {self.name}"


@dataclass(frozen=True)
class ActionFrameEntry:
    direction: int
    frame: int
    image_index: int


@dataclass
class LoadedFrame:
    index: int
    dx: int
    dy: int
    image: Image.Image

    def render_frame(self) -> RenderFrame:
        return RenderFrame(
            index=self.index,
            dx=self.dx,
            dy=self.dy,
            width=self.image.width,
            height=self.image.height,
        )


@dataclass
class FightPack:
    source_path: Path
    frame_counts: list[int]
    frames: dict[int, LoadedFrame]

    @property
    def available_styles(self) -> list[int]:
        return [style for style, count in enumerate(self.frame_counts) if count > 0]

    def frame_for(self, style: int, direction: int, frame: int) -> LoadedFrame | None:
        if self.frame_counts[style] <= 0:
            return None
        index = frame_index_for(self.frame_counts, style, direction, frame)
        return self.frames.get(index)


def parse_fightframe_text(text: str) -> list[int]:
    values = [int(number) for number in TEXT_NUMBER_PATTERN.findall(text)]
    frame_counts = [0] * FIGHT_STYLE_COUNT
    for offset in range(0, len(values) - 1, 2):
        style = values[offset]
        count = values[offset + 1]
        if 0 <= style < FIGHT_STYLE_COUNT:
            frame_counts[style] = max(0, count)
    return frame_counts


def parse_role_metadata(text: str) -> list[RoleMetadata]:
    metadata: list[RoleMetadata] = []
    for line in text.splitlines():
        match = ROLE_LINE_PATTERN.match(line)
        if match is None:
            continue
        role_id = int(match.group(1))
        name = (match.group(2) or "").strip() or f"角色 {role_id}"
        metadata.append(RoleMetadata(role_id=role_id, name=name))
    return metadata


def discover_role_options(config_root: Path = DEFAULT_CONFIG_ROOT, fight_root: Path = DEFAULT_FIGHT_ROOT) -> list[RoleOption]:
    options: list[RoleOption] = []
    seen: set[int] = set()
    pool_path = config_root / "chess_pool.yaml"
    if pool_path.exists():
        for metadata in parse_role_metadata(pool_path.read_text(encoding="utf-8-sig")):
            try:
                path = resolve_fight_path(str(metadata.role_id), fight_root)
            except FileNotFoundError:
                continue
            options.append(RoleOption(role_id=metadata.role_id, name=metadata.name, path=path))
            seen.add(metadata.role_id)

    if fight_root.exists():
        for path in sorted(fight_root.glob("fight*.zip")):
            match = FIGHT_PACK_PATTERN.match(path.name)
            if match is None:
                continue
            role_id = int(match.group(1))
            if role_id in seen:
                continue
            options.append(RoleOption(role_id=role_id, name=f"角色 {role_id}", path=path.resolve()))
            seen.add(role_id)
    return options


def frame_index_for(frame_counts: list[int], style: int, direction: int, frame: int) -> int:
    if not 0 <= style < FIGHT_STYLE_COUNT:
        raise ValueError(f"style must be 0-{FIGHT_STYLE_COUNT - 1}, got {style}")
    if not 0 <= direction < FIGHT_DIRECTION_COUNT:
        raise ValueError(f"direction must be 0-{FIGHT_DIRECTION_COUNT - 1}, got {direction}")
    frame_count = frame_counts[style]
    if frame_count <= 0:
        raise ValueError(f"style {style} has no frames")
    if not 0 <= frame < frame_count:
        raise ValueError(f"frame must be 0-{frame_count - 1}, got {frame}")

    return sum(count * FIGHT_DIRECTION_COUNT for count in frame_counts[:style]) + frame_count * direction + frame


def action_frame_entries(frame_counts: list[int], style: int) -> list[ActionFrameEntry]:
    if not 0 <= style < FIGHT_STYLE_COUNT:
        raise ValueError(f"style must be 0-{FIGHT_STYLE_COUNT - 1}, got {style}")
    frame_count = frame_counts[style]
    if frame_count <= 0:
        return []
    return [
        ActionFrameEntry(direction=direction, frame=frame, image_index=frame_index_for(frame_counts, style, direction, frame))
        for direction in range(FIGHT_DIRECTION_COUNT)
        for frame in range(frame_count)
    ]


def compute_offset_layout(frames: list[RenderFrame], margin: int = 24) -> OffsetLayout:
    if not frames:
        return OffsetLayout(width=max(1, margin * 2), height=max(1, margin * 2), origin_x=margin, origin_y=margin, placements=[])

    min_x = min(-frame.dx for frame in frames)
    min_y = min(-frame.dy for frame in frames)
    max_x = max(-frame.dx + frame.width for frame in frames)
    max_y = max(-frame.dy + frame.height for frame in frames)

    width = max(1, max_x - min_x + margin * 2)
    height = max(1, max_y - min_y + margin * 2)
    origin_x = margin - min_x
    origin_y = margin - min_y
    placements = [
        FramePlacement(
            frame_index=frame.index,
            x=origin_x - frame.dx,
            y=origin_y - frame.dy,
        )
        for frame in frames
    ]
    return OffsetLayout(width=width, height=height, origin_x=origin_x, origin_y=origin_y, placements=placements)


def resolve_fight_path(value: str | None, fight_root: Path = DEFAULT_FIGHT_ROOT) -> Path:
    if value is None:
        value = str(DEFAULT_FIGHT_ID)

    candidate = Path(value).expanduser()
    if candidate.exists():
        return candidate.resolve()

    if value.isdigit():
        fight_id = int(value)
        for name in (f"fight{fight_id}.zip", f"fight{fight_id:03d}.zip", f"fight{fight_id:04d}.zip"):
            path = fight_root / name
            if path.exists():
                return path.resolve()

    raise FileNotFoundError(f"Cannot find fight pack for {value!r}")


def load_fight_pack(source_path: Path) -> FightPack:
    accessor = ArchiveAccessor(source_path)
    try:
        raw_fightframe = accessor.read_bytes("fightframe.txt")
        if raw_fightframe is None:
            raise FileNotFoundError("fightframe.txt is missing")
        frame_counts = parse_fightframe_text(raw_fightframe.decode("utf-8-sig", errors="replace"))

        offsets = load_offsets(accessor)
        frames: dict[int, LoadedFrame] = {}
        for index, (dx, dy) in enumerate(offsets):
            variants = load_image_variants(accessor, index)
            if not variants:
                continue
            frames[index] = LoadedFrame(index=index, dx=dx, dy=dy, image=variants[0])

        return FightPack(source_path=source_path, frame_counts=frame_counts, frames=frames)
    finally:
        accessor.close()


def make_placeholder(text: str, size: tuple[int, int] = (320, 240)) -> Image.Image:
    image = Image.new("RGBA", size, (24, 28, 34, 255))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    box = draw.multiline_textbbox((0, 0), text, font=font, spacing=4)
    x = (size[0] - (box[2] - box[0])) // 2
    y = (size[1] - (box[3] - box[1])) // 2
    draw.rectangle((0, 0, size[0] - 1, size[1] - 1), outline=(90, 100, 115, 255))
    draw.multiline_text((x, y), text, font=font, fill=(220, 226, 232, 255), align="center", spacing=4)
    return image


def scaled_image(image: Image.Image, zoom: float) -> Image.Image:
    if zoom == 1.0:
        return image
    return image.resize(
        (
            max(1, round(image.width * zoom)),
            max(1, round(image.height * zoom)),
        ),
        RESAMPLING.NEAREST,
    )


def fit_image(image: Image.Image, max_width: int, max_height: int) -> Image.Image:
    scale = min(max_width / image.width, max_height / image.height)
    return scaled_image(image, scale)


def compose_offset_frame(frame: LoadedFrame, margin: int = 12, background: tuple[int, int, int, int] = (14, 19, 25, 255)) -> Image.Image:
    layout = compute_offset_layout([frame.render_frame()], margin=margin)
    canvas = Image.new("RGBA", (layout.width, layout.height), background)
    draw = ImageDraw.Draw(canvas)
    draw.line((0, layout.origin_y, layout.width, layout.origin_y), fill=(53, 80, 107, 255))
    draw.line((layout.origin_x, 0, layout.origin_x, layout.height), fill=(53, 80, 107, 255))
    placement = layout.placements[0]
    image = frame.image if frame.image.mode == "RGBA" else frame.image.convert("RGBA")
    canvas.alpha_composite(image, (placement.x, placement.y))
    return canvas


class FightFrameViewerApp:
    def __init__(self, root: tk.Tk, initial_source: Path | None, role_options: list[RoleOption] | None = None) -> None:
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1120x820")
        self.root.minsize(780, 560)

        self.pack: FightPack | None = None
        self.active_role: RoleOption | None = None
        self.role_options = role_options if role_options is not None else discover_role_options()
        self.role_var = tk.StringVar(value="")
        self.view_mode_var = tk.StringVar(value=VIEW_MODE_SINGLE)
        self.style_var = tk.IntVar(value=0)
        self.direction_var = tk.IntVar(value=0)
        self.frame_var = tk.IntVar(value=0)
        self.playing_var = tk.BooleanVar(value=True)
        self.loop_var = tk.BooleanVar(value=True)
        self.zoom_var = tk.DoubleVar(value=2.0)
        self.interval_var = tk.IntVar(value=DEFAULT_INTERVAL_MS)
        self.status_var = tk.StringVar(value="")
        self.source_var = tk.StringVar(value="")

        self.current_photo: ImageTk.PhotoImage | None = None
        self.strip_photos: list[ImageTk.PhotoImage] = []
        self.grid_photos: list[ImageTk.PhotoImage] = []
        self._after_id: str | None = None

        self._configure_style()
        self._build_layout()
        self.root.protocol("WM_DELETE_WINDOW", self.close)

        if initial_source is not None:
            self.load_source(initial_source)
        else:
            self.status_var.set("請開啟 fight zip，或用命令列傳入角色戰鬥圖編號。")

        self._schedule_tick()

    def _configure_style(self) -> None:
        self.root.configure(background="#10141a")
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("App.TFrame", background="#10141a")
        style.configure("Toolbar.TFrame", background="#171f2a")
        style.configure("Panel.TFrame", background="#10141a")
        style.configure("App.TLabel", background="#171f2a", foreground="#e8edf3", font=("Segoe UI", 10))
        style.configure("Meta.TLabel", background="#10141a", foreground="#b6c2cf", font=("Consolas", 10))
        style.configure("TButton", font=("Segoe UI", 10))
        style.configure("TCheckbutton", background="#171f2a", foreground="#e8edf3", font=("Segoe UI", 10))

    def _build_layout(self) -> None:
        outer = ttk.Frame(self.root, style="App.TFrame", padding=10)
        outer.pack(fill="both", expand=True)
        outer.rowconfigure(1, weight=1)
        outer.columnconfigure(0, weight=1)

        toolbar = ttk.Frame(outer, style="Toolbar.TFrame", padding=10)
        toolbar.grid(row=0, column=0, sticky="ew")
        toolbar.columnconfigure(4, weight=1)
        toolbar.columnconfigure(11, weight=1)

        ttk.Label(toolbar, text="角色", style="App.TLabel").grid(row=0, column=0, padx=(0, 4), pady=(0, 8))
        self.role_combo = ttk.Combobox(
            toolbar,
            state="readonly",
            width=20,
            textvariable=self.role_var,
            values=[option.label for option in self.role_options],
        )
        self.role_combo.grid(row=0, column=1, padx=(0, 10), pady=(0, 8), sticky="w")
        self.role_combo.bind("<<ComboboxSelected>>", lambda _event: self._select_role())

        ttk.Button(toolbar, text="開啟", command=self.choose_file).grid(row=0, column=2, padx=(0, 8), pady=(0, 8))
        ttk.Button(toolbar, text="重載", command=self.reload_current).grid(row=0, column=3, padx=(0, 12), pady=(0, 8))
        ttk.Label(toolbar, textvariable=self.source_var, style="App.TLabel").grid(row=0, column=4, columnspan=8, sticky="ew", pady=(0, 8))

        ttk.Label(toolbar, text="模式", style="App.TLabel").grid(row=1, column=0, padx=(0, 4))
        self.mode_combo = ttk.Combobox(toolbar, state="readonly", width=10, values=[label for _value, label in VIEW_MODE_LABELS])
        self.mode_combo.grid(row=1, column=1, padx=(0, 10), sticky="w")
        self.mode_combo.current(0)
        self.mode_combo.bind("<<ComboboxSelected>>", lambda _event: self._select_view_mode())

        ttk.Label(toolbar, text="動作", style="App.TLabel").grid(row=1, column=2, padx=(0, 4))
        self.style_combo = ttk.Combobox(toolbar, state="readonly", width=16)
        self.style_combo.grid(row=1, column=3, padx=(0, 10))
        self.style_combo.bind("<<ComboboxSelected>>", lambda _event: self._select_style())

        ttk.Label(toolbar, text="方向", style="App.TLabel").grid(row=1, column=4, padx=(0, 4))
        self.direction_combo = ttk.Combobox(toolbar, state="readonly", width=10, values=DIRECTION_LABELS)
        self.direction_combo.grid(row=1, column=5, padx=(0, 10))
        self.direction_combo.bind("<<ComboboxSelected>>", lambda _event: self._select_direction())

        self.play_button = ttk.Button(toolbar, text="暫停", command=self.toggle_play)
        self.play_button.grid(row=1, column=6, padx=(0, 8))
        ttk.Checkbutton(toolbar, text="循環", variable=self.loop_var).grid(row=1, column=7, padx=(0, 8))
        ttk.Button(toolbar, text="上一幀", command=lambda: self.step_frame(-1)).grid(row=1, column=8, padx=(0, 6))
        ttk.Button(toolbar, text="下一幀", command=lambda: self.step_frame(1)).grid(row=1, column=9, padx=(0, 12))

        ttk.Label(toolbar, text="縮放", style="App.TLabel").grid(row=1, column=10, padx=(0, 4))
        ttk.Scale(toolbar, from_=1.0, to=5.0, orient="horizontal", variable=self.zoom_var, command=lambda _value: self.render()).grid(row=1, column=11, sticky="ew")

        body = ttk.Frame(outer, style="Panel.TFrame")
        body.grid(row=1, column=0, sticky="nsew", pady=(10, 0))
        body.rowconfigure(0, weight=1)
        body.columnconfigure(0, weight=1)

        self.canvas = tk.Canvas(body, background="#0b0f14", highlightthickness=0)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        vertical_scroll = ttk.Scrollbar(body, orient="vertical", command=self.canvas.yview)
        vertical_scroll.grid(row=0, column=1, sticky="ns")
        horizontal_scroll = ttk.Scrollbar(body, orient="horizontal", command=self.canvas.xview)
        horizontal_scroll.grid(row=1, column=0, sticky="ew")
        self.canvas.configure(xscrollcommand=horizontal_scroll.set, yscrollcommand=vertical_scroll.set)
        self.canvas.bind("<MouseWheel>", self._on_mouse_wheel)
        self.canvas.bind("<Shift-MouseWheel>", self._on_shift_mouse_wheel)

        bottom = ttk.Frame(outer, style="Panel.TFrame")
        bottom.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        bottom.columnconfigure(0, weight=1)
        self.strip_canvas = tk.Canvas(bottom, height=150, background="#0b0f14", highlightthickness=0)
        self.strip_canvas.grid(row=0, column=0, sticky="ew")
        ttk.Label(bottom, textvariable=self.status_var, style="Meta.TLabel").grid(row=1, column=0, sticky="w", pady=(6, 0))

    def _on_mouse_wheel(self, event: tk.Event) -> None:
        self.canvas.yview_scroll(-int(event.delta / 120), "units")

    def _on_shift_mouse_wheel(self, event: tk.Event) -> None:
        self.canvas.xview_scroll(-int(event.delta / 120), "units")

    def close(self) -> None:
        if self._after_id is not None:
            self.root.after_cancel(self._after_id)
            self._after_id = None
        self.root.destroy()

    def choose_file(self) -> None:
        initial = str(DEFAULT_FIGHT_ROOT if DEFAULT_FIGHT_ROOT.exists() else ROOT)
        selected = filedialog.askopenfilename(
            title="開啟 fight zip",
            initialdir=initial,
            filetypes=[("Fight packs", "fight*.zip"), ("Zip files", "*.zip"), ("All files", "*.*")],
        )
        if selected:
            self.load_source(Path(selected))

    def _select_role(self) -> None:
        selected = self.role_combo.current()
        if 0 <= selected < len(self.role_options):
            option = self.role_options[selected]
            self.load_source(option.path, option)

    def _select_view_mode(self) -> None:
        self.view_mode_var.set(VIEW_MODE_BY_LABEL.get(self.mode_combo.get(), VIEW_MODE_SINGLE))
        self.render()

    def reload_current(self) -> None:
        if self.pack is None:
            self.choose_file()
            return
        self.load_source(self.pack.source_path, self.active_role)

    def load_source(self, source_path: Path, role: RoleOption | None = None) -> None:
        try:
            pack = load_fight_pack(source_path)
        except Exception as exc:
            messagebox.showerror(APP_TITLE, str(exc))
            self.status_var.set(f"載入失敗: {exc}")
            return

        self.pack = pack
        self.active_role = role or self._role_for_path(pack.source_path)
        self._sync_role_combo()
        self.source_var.set(self._source_label())
        available = pack.available_styles
        if not available:
            self.status_var.set(f"{pack.source_path.name}: fightframe.txt 沒有可播放動作。")
            return
        self.style_var.set(available[0])
        self.direction_var.set(0)
        self.frame_var.set(0)
        self._refresh_controls()
        self.render()

    def _role_for_path(self, path: Path) -> RoleOption | None:
        resolved = path.resolve()
        for option in self.role_options:
            if option.path == resolved:
                return option
        return None

    def _sync_role_combo(self) -> None:
        if self.active_role is None:
            self.role_var.set("")
            return
        for index, option in enumerate(self.role_options):
            if option.role_id == self.active_role.role_id:
                self.role_combo.current(index)
                return
        self.role_var.set(self.active_role.label)

    def _source_label(self) -> str:
        if self.pack is None:
            return ""
        if self.active_role is None:
            return str(self.pack.source_path)
        return f"{self.active_role.label}  |  {self.pack.source_path}"

    def _refresh_controls(self) -> None:
        if self.pack is None:
            return
        values = [
            f"{STYLE_LABELS[style]} ({self.pack.frame_counts[style]} 幀)"
            for style in range(FIGHT_STYLE_COUNT)
            if self.pack.frame_counts[style] > 0
        ]
        styles = self.pack.available_styles
        self.style_combo.configure(values=values)
        if self.style_var.get() in styles:
            self.style_combo.current(styles.index(self.style_var.get()))
        self.direction_combo.current(self.direction_var.get())
        for index, (mode, _label) in enumerate(VIEW_MODE_LABELS):
            if mode == self.view_mode_var.get():
                self.mode_combo.current(index)
                break

    def _select_style(self) -> None:
        if self.pack is None:
            return
        styles = self.pack.available_styles
        selected = self.style_combo.current()
        if 0 <= selected < len(styles):
            self.style_var.set(styles[selected])
            self.frame_var.set(0)
            self.render()

    def _select_direction(self) -> None:
        selected = self.direction_combo.current()
        if 0 <= selected < FIGHT_DIRECTION_COUNT:
            self.direction_var.set(selected)
            self.frame_var.set(0)
            self.render()

    def toggle_play(self) -> None:
        self.playing_var.set(not self.playing_var.get())
        self.play_button.configure(text="暫停" if self.playing_var.get() else "播放")

    def step_frame(self, delta: int, from_playback: bool = False) -> None:
        if self.pack is None:
            return
        style = self.style_var.get()
        count = self.pack.frame_counts[style]
        if count <= 0:
            return
        next_frame = self.frame_var.get() + delta
        if self.loop_var.get():
            next_frame %= count
        else:
            if next_frame < 0:
                next_frame = 0
            elif next_frame >= count:
                next_frame = count - 1
                if from_playback:
                    self.playing_var.set(False)
                    self.play_button.configure(text="播放")
        self.frame_var.set(next_frame)
        self.render()

    def _schedule_tick(self) -> None:
        self._after_id = self.root.after(max(20, self.interval_var.get()), self._tick)

    def _tick(self) -> None:
        if self.playing_var.get():
            self.step_frame(1, from_playback=True)
        self._schedule_tick()

    def render(self) -> None:
        self.canvas.delete("all")
        self.strip_canvas.delete("all")
        self.current_photo = None
        self.strip_photos.clear()
        self.grid_photos.clear()

        if self.pack is None:
            return

        style = self.style_var.get()
        if self.view_mode_var.get() == VIEW_MODE_ACTION:
            self._render_action_grid(style)
            return

        direction = self.direction_var.get()
        frame_number = self.frame_var.get()
        frame = self.pack.frame_for(style, direction, frame_number)
        if frame is None:
            self._draw_placeholder(f"缺少貼圖\n動作 {style} 方向 {direction} 幀 {frame_number}")
            return

        layout = compute_offset_layout([frame.render_frame()], margin=32)
        zoom = self.zoom_var.get()
        image = scaled_image(frame.image, zoom)
        x = layout.placements[0].x * zoom
        y = layout.placements[0].y * zoom
        origin_x = layout.origin_x * zoom
        origin_y = layout.origin_y * zoom
        canvas_width = max(int(layout.width * zoom), self.canvas.winfo_width())
        canvas_height = max(int(layout.height * zoom), self.canvas.winfo_height())

        self.canvas.configure(scrollregion=(0, 0, canvas_width, canvas_height))
        self._draw_origin(self.canvas, origin_x, origin_y, canvas_width, canvas_height)
        self.current_photo = ImageTk.PhotoImage(image)
        self.canvas.create_image(x, y, anchor="nw", image=self.current_photo)

        self.status_var.set(
            f"{self.pack.source_path.name} | {STYLE_LABELS[style]} | {DIRECTION_LABELS[direction]} | "
            f"幀 {frame_number + 1}/{self.pack.frame_counts[style]} | 圖號 {frame.index} | "
            f"offset=({frame.dx},{frame.dy}) | size={frame.image.width}x{frame.image.height}"
        )
        self._render_direction_strip(style, frame_number)

    def _draw_origin(self, canvas: tk.Canvas, origin_x: float, origin_y: float, width: int, height: int) -> None:
        canvas.create_line(0, origin_y, width, origin_y, fill="#35506b", dash=(4, 4))
        canvas.create_line(origin_x, 0, origin_x, height, fill="#35506b", dash=(4, 4))
        canvas.create_oval(origin_x - 4, origin_y - 4, origin_x + 4, origin_y + 4, outline="#e0bf59", width=2)

    def _draw_placeholder(self, text: str) -> None:
        image = make_placeholder(text)
        self.current_photo = ImageTk.PhotoImage(image)
        self.canvas.create_image(20, 20, anchor="nw", image=self.current_photo)

    def _render_action_grid(self, style: int) -> None:
        if self.pack is None:
            return
        frame_count = self.pack.frame_counts[style]
        if frame_count <= 0:
            self._draw_placeholder(f"{STYLE_LABELS[style]}\n沒有可顯示幀")
            return

        zoom = self.zoom_var.get()
        max_thumb_w = max(120, round(110 * zoom))
        max_thumb_h = max(90, round(85 * zoom))
        header_w = 96
        header_h = 34
        cell_w = max_thumb_w + 32
        cell_h = max_thumb_h + 54
        total_w = header_w + frame_count * cell_w + 20
        total_h = header_h + FIGHT_DIRECTION_COUNT * cell_h + 20
        canvas_width = max(total_w, self.canvas.winfo_width())
        canvas_height = max(total_h, self.canvas.winfo_height())
        self.canvas.configure(scrollregion=(0, 0, canvas_width, canvas_height))

        self.canvas.create_text(12, 10, anchor="nw", text=STYLE_LABELS[style], fill="#e8edf3", font=("Segoe UI", 10, "bold"))
        for frame_number in range(frame_count):
            x = header_w + frame_number * cell_w
            self.canvas.create_text(
                x + cell_w / 2,
                18,
                text=f"{frame_number + 1}",
                fill="#b6c2cf",
                font=("Consolas", 10),
            )

        current_direction = self.direction_var.get()
        current_frame = self.frame_var.get()
        for direction, direction_label in enumerate(DIRECTION_LABELS):
            y = header_h + direction * cell_h
            if direction == current_direction:
                self.canvas.create_rectangle(0, y, total_w, y + cell_h, fill="#111c28", outline="")
            self.canvas.create_text(12, y + cell_h / 2, anchor="w", text=direction_label, fill="#dce6f0", font=("Segoe UI", 10))

            for frame_number in range(frame_count):
                x = header_w + frame_number * cell_w
                tag = f"cell-{direction}-{frame_number}"
                is_current = frame_number == current_frame
                is_selected = is_current and direction == current_direction
                outline = "#e0bf59" if is_current else "#253242"
                fill = "#182536" if is_selected else "#0e141b"
                self.canvas.create_rectangle(
                    x + 4,
                    y + 4,
                    x + cell_w - 4,
                    y + cell_h - 4,
                    fill=fill,
                    outline=outline,
                    width=2 if is_current else 1,
                    tags=(tag,),
                )

                frame = self.pack.frame_for(style, direction, frame_number)
                if frame is None:
                    thumb = make_placeholder("缺圖", (max_thumb_w, max_thumb_h))
                    label = "缺圖"
                else:
                    thumb = fit_image(compose_offset_frame(frame, margin=10), max_thumb_w, max_thumb_h)
                    label = f"圖 {frame.index}  ({frame.dx},{frame.dy})"
                photo = ImageTk.PhotoImage(thumb)
                self.grid_photos.append(photo)
                image_x = x + (cell_w - thumb.width) / 2
                image_y = y + 12
                self.canvas.create_image(image_x, image_y, anchor="nw", image=photo, tags=(tag,))
                self.canvas.create_text(
                    x + cell_w / 2,
                    y + max_thumb_h + 34,
                    text=label,
                    fill="#aebcca",
                    font=("Consolas", 9),
                    tags=(tag,),
                )
                self.canvas.tag_bind(tag, "<Button-1>", lambda _event, d=direction, f=frame_number: self.select_cell(d, f))

        loop_text = "循環" if self.loop_var.get() else "不循環"
        role_text = self.active_role.label if self.active_role is not None else self.pack.source_path.name
        self.status_var.set(
            f"{role_text} | {self.pack.source_path.name} | {STYLE_LABELS[style]} 動作全幀 | "
            f"{frame_count} 幀 x {FIGHT_DIRECTION_COUNT} 方向 | 目前 {DIRECTION_LABELS[current_direction]} 第 {current_frame + 1} 幀 | {loop_text}"
        )

    def select_cell(self, direction: int, frame_number: int) -> None:
        self.direction_var.set(direction)
        self.frame_var.set(frame_number)
        self._refresh_controls()
        self.render()

    def _render_direction_strip(self, style: int, frame_number: int) -> None:
        if self.pack is None:
            return
        x = 12
        y = 12
        max_thumb_w = 140
        max_thumb_h = 110
        for direction, label in enumerate(DIRECTION_LABELS):
            frame = self.pack.frame_for(style, direction, min(frame_number, self.pack.frame_counts[style] - 1))
            if frame is None:
                thumb = make_placeholder("缺圖", (max_thumb_w, max_thumb_h))
            else:
                thumb = fit_image(compose_offset_frame(frame, margin=12), max_thumb_w, max_thumb_h)
            photo = ImageTk.PhotoImage(thumb)
            self.strip_photos.append(photo)
            self.strip_canvas.create_image(x, y, anchor="nw", image=photo)
            self.strip_canvas.create_text(x + max_thumb_w // 2, y + max_thumb_h + 18, text=label, fill="#dce6f0", font=("Segoe UI", 9))
            x += max_thumb_w + 24


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Preview KYS fight animation frames with offsets.")
    parser.add_argument("fight", nargs="?", help="Fight id, fight zip, or extracted fight directory. Defaults to 605.")
    parser.add_argument("--fight-root", default=str(DEFAULT_FIGHT_ROOT), help="Directory used when resolving numeric fight ids.")
    parser.add_argument("--config-root", default=str(DEFAULT_CONFIG_ROOT), help="Directory used for discovering named chess roles.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    fight_root = Path(args.fight_root)
    role_options = discover_role_options(Path(args.config_root), fight_root)
    source = resolve_fight_path(args.fight, fight_root)
    root = tk.Tk()
    FightFrameViewerApp(root, source, role_options=role_options)
    try:
        root.mainloop()
    except KeyboardInterrupt:
        try:
            root.destroy()
        except tk.TclError:
            pass
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
