#!/usr/bin/env python3
"""Inspect and repair MIDI files with dangling notes.

The repository's music assets are standard SMF files. Some are format 0 with a
single track, while others contain multiple tracks. This tool scans those files,
reports unmatched note-on / note-off pairs, trims trailing junk after the
end-of-track marker, and can append synthetic note-off events so FluidSynth
stops warning about pending voices.

Usage examples:

    python tools/fix_midi_noteoffs.py work/game-dev/music
    python tools/fix_midi_noteoffs.py work/game-dev/music --fix --in-place
    python tools/fix_midi_noteoffs.py music/intro.mid --fix --suffix .repaired
"""

from __future__ import annotations

import argparse
import glob
import shutil
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


MIDI_SUFFIXES = {".mid", ".midi", ".kar"}
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
END_OF_TRACK = b"\x00\xff\x2f\x00"


def note_label(note: int) -> str:
    octave = note // 12 - 1
    return f"{NOTE_NAMES[note % 12]}{octave}"


def note_key_label(key: tuple[int, int]) -> str:
    channel, note = key
    return f"ch{channel + 1} {note_label(note)}"


def read_u16_be(data: bytes, offset: int) -> int:
    if offset + 2 > len(data):
        raise EOFError
    return (data[offset] << 8) | data[offset + 1]


def read_u32_be(data: bytes, offset: int) -> int:
    if offset + 4 > len(data):
        raise EOFError
    return (data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3]


def read_varlen(data: bytes, offset: int) -> tuple[int, int] | None:
    value = 0
    position = offset
    while position < len(data):
        byte = data[position]
        position += 1
        value = (value << 7) | (byte & 0x7F)
        if byte < 0x80:
            return value, position
    return None


def encode_note_offs(pending: Counter[tuple[int, int]]) -> bytes:
    encoded = bytearray()
    for (channel, note), count in sorted(pending.items()):
        for _ in range(count):
            encoded.extend(b"\x00")
            encoded.append(0x80 | (channel & 0x0F))
            encoded.append(note & 0x7F)
            encoded.append(0x00)
    return bytes(encoded)


@dataclass
class TrackInspection:
    track_length: int
    pending: Counter[tuple[int, int]]
    stray_offs: Counter[tuple[int, int]]
    eot_start: int | None
    eot_end: int | None
    parse_end: int
    repair_cut_end: int
    parse_error: str | None

    @property
    def trailing_junk_bytes(self) -> int:
        if self.eot_end is None:
            return 0
        return max(0, self.track_length - self.eot_end)

    @property
    def needs_repair(self) -> bool:
        if sum(self.pending.values()) > 0:
            return True
        if self.parse_error is not None:
            return True
        if self.eot_start is None:
            return True
        return self.trailing_junk_bytes > 0


@dataclass
class TrackChunk:
    index: int
    offset: int
    length_offset: int
    payload_offset: int
    declared_length: int
    payload: bytes
    inspection: TrackInspection


@dataclass
class MidiInspection:
    path: Path
    header_length: int
    format_type: int
    track_count: int
    division: int
    header_end: int
    tracks: list[TrackChunk]


def inspect_track_payload(payload: bytes) -> TrackInspection:
    pending: Counter[tuple[int, int]] = Counter()
    stray_offs: Counter[tuple[int, int]] = Counter()
    running_status: int | None = None
    position = 0
    last_safe_position = 0
    eot_start: int | None = None
    eot_end: int | None = None
    parse_error: str | None = None

    def close_note(channel: int, note: int) -> None:
        key = (channel, note)
        if pending[key] > 0:
            pending[key] -= 1
        else:
            stray_offs[key] += 1

    while position < len(payload):
        delta = read_varlen(payload, position)
        if delta is None:
            parse_error = "unexpected EOF while reading delta-time"
            break

        _, position = delta
        if position >= len(payload):
            parse_error = "unexpected EOF after delta-time"
            break

        status = payload[position]

        if status == 0xFF:
            if position + 2 > len(payload):
                parse_error = "unexpected EOF in meta event"
                break

            meta_type = payload[position + 1]
            meta_length = read_varlen(payload, position + 2)
            if meta_length is None:
                parse_error = "unexpected EOF in meta length"
                break

            length, meta_data_start = meta_length
            meta_end = meta_data_start + length
            if meta_end > len(payload):
                parse_error = "unexpected EOF in meta payload"
                break

            if meta_type == 0x2F:
                eot_start = position
                eot_end = meta_end
                position = meta_end
                last_safe_position = position
                break

            position = meta_end
            last_safe_position = position
            running_status = None
            continue

        if status in (0xF0, 0xF7):
            sysex_length = read_varlen(payload, position + 1)
            if sysex_length is None:
                parse_error = "unexpected EOF in sysex length"
                break

            length, sysex_data_start = sysex_length
            sysex_end = sysex_data_start + length
            if sysex_end > len(payload):
                parse_error = "unexpected EOF in sysex payload"
                break

            position = sysex_end
            last_safe_position = position
            running_status = None
            continue

        if status in (0xF1, 0xF3):
            if position + 2 > len(payload):
                parse_error = f"unexpected EOF in system message 0x{status:02X}"
                break
            position += 2
            last_safe_position = position
            running_status = None
            continue

        if status == 0xF2:
            if position + 3 > len(payload):
                parse_error = "unexpected EOF in song-position message"
                break
            position += 3
            last_safe_position = position
            running_status = None
            continue

        if status in (0xF6, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFE):
            position += 1
            last_safe_position = position
            running_status = None
            continue

        if status < 0x80:
            if running_status is None:
                parse_error = f"unexpected data byte 0x{status:02X} without running status"
                break
            message_status = running_status
            data_start = position
        else:
            message_status = status
            running_status = status
            data_start = position + 1

        message_type = message_status & 0xF0
        channel = message_status & 0x0F
        data_length = 1 if message_type in (0xC0, 0xD0) else 2
        if data_start + data_length > len(payload):
            parse_error = "unexpected EOF in channel message"
            break

        note = payload[data_start]
        velocity = payload[data_start + 1] if data_length == 2 else 0
        if message_type == 0x90:
            if velocity == 0:
                close_note(channel, note)
            else:
                pending[(channel, note)] += 1
        elif message_type == 0x80:
            close_note(channel, note)

        position = data_start + data_length
        last_safe_position = position

    pending = Counter({key: count for key, count in pending.items() if count > 0})
    stray_offs = Counter({key: count for key, count in stray_offs.items() if count > 0})
    return TrackInspection(len(payload), pending, stray_offs, eot_start, eot_end, position, last_safe_position, parse_error)


def inspect_midi_file(midi_path: Path) -> MidiInspection:
    data = midi_path.read_bytes()
    if len(data) < 22 or data[:4] != b"MThd":
        raise ValueError("not a standard MIDI file")

    header_length = read_u32_be(data, 4)
    if header_length < 6:
        raise ValueError(f"invalid MIDI header length: {header_length}")

    header_end = 8 + header_length
    if header_end + 8 > len(data):
        raise EOFError("truncated MIDI header")

    format_type = read_u16_be(data, 8)
    track_count = read_u16_be(data, 10)
    division = read_u16_be(data, 12)

    tracks: list[TrackChunk] = []
    offset = header_end
    for track_index in range(track_count):
        if offset + 8 > len(data):
            raise EOFError(f"truncated track chunk {track_index}")
        if data[offset : offset + 4] != b"MTrk":
            raise ValueError(f"missing MTrk chunk at track {track_index}")

        declared_length = read_u32_be(data, offset + 4)
        payload_offset = offset + 8
        payload_end = min(payload_offset + declared_length, len(data))
        payload = data[payload_offset:payload_end]
        inspection = inspect_track_payload(payload)
        tracks.append(
            TrackChunk(
                index=track_index,
                offset=offset,
                length_offset=offset + 4,
                payload_offset=payload_offset,
                declared_length=declared_length,
                payload=payload,
                inspection=inspection,
            )
        )
        offset = payload_offset + declared_length

    return MidiInspection(
        path=midi_path,
        header_length=header_length,
        format_type=format_type,
        track_count=track_count,
        division=division,
        header_end=header_end,
        tracks=tracks,
    )


def iter_midi_paths(paths: Iterable[str]) -> list[Path]:
    discovered: list[Path] = []
    seen: set[Path] = set()

    for raw_path in paths:
        path = Path(raw_path)
        if path.is_dir():
            for candidate in sorted(path.rglob("*")):
                if candidate.is_file() and candidate.suffix.lower() in MIDI_SUFFIXES:
                    resolved = candidate.resolve()
                    if resolved not in seen:
                        seen.add(resolved)
                        discovered.append(candidate)
            continue

        if path.is_file():
            resolved = path.resolve()
            if resolved not in seen:
                seen.add(resolved)
                discovered.append(path)
            continue

        if any(ch in raw_path for ch in "*?["):
            for match in sorted(glob.glob(raw_path, recursive=True)):
                candidate = Path(match)
                if candidate.is_file() and candidate.suffix.lower() in MIDI_SUFFIXES:
                    resolved = candidate.resolve()
                    if resolved not in seen:
                        seen.add(resolved)
                        discovered.append(candidate)
            continue

        raise FileNotFoundError(raw_path)

    return discovered


def format_track_issue(track: TrackChunk) -> str | None:
    inspection = track.inspection
    pending_total = sum(inspection.pending.values())
    stray_total = sum(inspection.stray_offs.values())

    parts = []
    if pending_total:
        parts.append(f"{pending_total} pending note-on(s)")
    if stray_total:
        parts.append(f"{stray_total} stray note-off(s)")
    if inspection.eot_start is None:
        parts.append("missing end-of-track marker")
    elif inspection.trailing_junk_bytes:
        parts.append(f"{inspection.trailing_junk_bytes} trailing junk byte(s)")
    if inspection.parse_error is not None:
        parts.append(f"parse error: {inspection.parse_error}")

    if not parts:
        return None
    return ", ".join(parts)


def print_report(report: MidiInspection) -> None:
    track_issues = [(track, format_track_issue(track)) for track in report.tracks]
    if all(issue is None for _, issue in track_issues):
        print(f"{report.path}: clean")
        return

    overall_parts = []
    pending_total = sum(sum(track.inspection.pending.values()) for track in report.tracks)
    stray_total = sum(sum(track.inspection.stray_offs.values()) for track in report.tracks)
    if pending_total:
        overall_parts.append(f"{pending_total} pending note-on(s)")
    if stray_total:
        overall_parts.append(f"{stray_total} stray note-off(s)")
    if any(track.inspection.eot_start is None for track in report.tracks):
        overall_parts.append("missing end-of-track marker")
    if any(track.inspection.trailing_junk_bytes for track in report.tracks):
        total_junk = sum(track.inspection.trailing_junk_bytes for track in report.tracks)
        overall_parts.append(f"{total_junk} trailing junk byte(s)")
    if any(track.inspection.parse_error is not None for track in report.tracks):
        overall_parts.append("parse errors present")

    print(f"{report.path}: {', '.join(overall_parts)}")
    for track, issue in track_issues:
        if issue is None:
            continue
        print(f"  track {track.index}: {issue}")
        if track.inspection.pending:
            for key, count in sorted(track.inspection.pending.items(), key=lambda item: (item[0][0], item[0][1])):
                print(f"    pending {note_key_label(key)} x{count}")
        if track.inspection.stray_offs:
            for key, count in sorted(track.inspection.stray_offs.items(), key=lambda item: (item[0][0], item[0][1])):
                print(f"    stray   {note_key_label(key)} x{count}")


def repair_track(track: TrackChunk) -> bytes:
    inspection = track.inspection
    if inspection.eot_start is not None:
        prefix = track.payload[: inspection.eot_start]
        suffix = track.payload[inspection.eot_start : inspection.eot_end]
    else:
        prefix = track.payload[: inspection.repair_cut_end]
        suffix = END_OF_TRACK

    return prefix + encode_note_offs(inspection.pending) + suffix


def repair_midi_file(report: MidiInspection, *, backup: bool, suffix: str, in_place: bool) -> Path:
    original_data = report.path.read_bytes()
    repaired_data = bytearray()
    repaired_data.extend(original_data[: report.header_end])

    for track in report.tracks:
        payload = repair_track(track) if track.inspection.needs_repair else track.payload
        repaired_data.extend(b"MTrk")
        repaired_data.extend(len(payload).to_bytes(4, byteorder="big"))
        repaired_data.extend(payload)

    if in_place:
        if backup:
            backup_path = report.path.with_suffix(report.path.suffix + ".bak")
            shutil.copy2(report.path, backup_path)
        output_path = report.path
    else:
        output_path = report.path.with_name(f"{report.path.stem}{suffix}{report.path.suffix}")

    output_path.write_bytes(repaired_data)
    return output_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect and repair MIDI files with dangling notes.")
    parser.add_argument("paths", nargs="+", help="MIDI files or directories to inspect")
    parser.add_argument("--fix", action="store_true", help="write repaired MIDI files")
    parser.add_argument("--in-place", action="store_true", help="overwrite the input file when used with --fix")
    parser.add_argument("--backup", action="store_true", help="create a .bak backup before in-place overwrite")
    parser.add_argument("--suffix", default=".fixed", help="suffix used for repaired output files")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        midi_paths = iter_midi_paths(args.paths)
    except FileNotFoundError as exc:
        print(f"error: path not found: {exc}", file=sys.stderr)
        return 2

    if not midi_paths:
        print("No MIDI files found.")
        return 0

    exit_code = 0
    repaired_any = False

    for midi_path in midi_paths:
        try:
            report = inspect_midi_file(midi_path)
        except Exception as exc:
            print(f"{midi_path}: failed to inspect MIDI file: {exc}", file=sys.stderr)
            exit_code = 1
            continue

        print_report(report)

        if not args.fix:
            continue

        if not any(track.inspection.needs_repair for track in report.tracks):
            continue

        try:
            output_path = repair_midi_file(report, backup=args.backup, suffix=args.suffix, in_place=args.in_place)
        except Exception as exc:
            print(f"{midi_path}: failed to repair MIDI file: {exc}", file=sys.stderr)
            exit_code = 1
            continue

        repaired_any = True
        print(f"  wrote {output_path}")

    if args.fix and not repaired_any and exit_code == 0:
        print("No repairs were necessary.")

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
