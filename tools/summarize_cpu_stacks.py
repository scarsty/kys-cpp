"""Turn an xperf CPU stack HTML report into JSON and Markdown for VS Code."""

import argparse
import html
import json
from pathlib import Path
import re


def table_rows(report, table_id):
    table = re.search(
        rf"<a id='{table_id}'>.*?<tbody>(.*?)</tbody>", report, re.DOTALL
    )
    if table is None:
        raise ValueError(f"Missing xperf table {table_id}")
    return [
        [
            # xperf writes C++ template angle brackets literally. Strip links,
            # preserving those brackets as part of the symbol name.
            html.unescape(re.sub(r"</?a\b[^>]*>", "", cell)).strip()
            for cell in re.findall(r"<td>(.*?)</td>", row, re.DOTALL)
        ]
        for row in re.findall(r"<tr[^>]*>(.*?)</tr>", table.group(1), re.DOTALL)
    ]


def summarize(report, process_name):
    processes = table_rows(report, "TblP")
    samples = sum(int(row[2]) for row in processes if row[0] == process_name)
    if samples == 0:
        raise ValueError(f"No CPU samples for {process_name}")
    functions = [
        {
            "function": row[0],
            "inclusiveSamples": int(row[1]),
            "selfSamples": int(row[3]),
            "inclusivePercent": 100 * int(row[1]) / samples,
            "selfPercent": 100 * int(row[3]) / samples,
        }
        for row in table_rows(report, "TblSI")
    ]
    return {"process": process_name, "samples": samples, "functions": functions}


def markdown(summary, source, limit):
    lines = [
        "# Sampled CPU hotspots",
        "",
        f"Process: `{summary['process']}`; CPU samples: **{summary['samples']:,}**.",
        f"Full call trees and caller/callee links: [{source.name}]({source.name}).",
        "",
        "Inclusive percentages include callees and overlap. Self percentages are leaf samples.",
        "Sampling an optimized Release executable does not recover every inlined function.",
        "Profiled duration must not be used as the replay benchmark.",
    ]
    for title, rows in (
        ("Largest inclusive paths", sorted(summary["functions"], key=lambda r: r["inclusiveSamples"], reverse=True)),
        ("Largest self costs", sorted(summary["functions"], key=lambda r: r["selfSamples"], reverse=True)),
    ):
        lines += ["", f"## {title}", "", "| Function | Inclusive | Self |", "|---|---:|---:|"]
        for row in rows[:limit]:
            name = row["function"].replace("|", "\\|").replace("`", "'")
            lines.append(f"| `{name}` | {row['inclusivePercent']:.2f}% | {row['selfPercent']:.2f}% |")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--process", default="kys_chess_cli.exe")
    parser.add_argument("--limit", type=int, default=30)
    args = parser.parse_args()
    summary = summarize(args.report.read_text(encoding="utf-8-sig"), args.process)
    args.report.with_suffix(".json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    output = args.report.with_suffix(".md")
    output.write_text(markdown(summary, args.report, args.limit), encoding="utf-8")
    print(f"{summary['samples']:,} samples; summary: {output}")


if __name__ == "__main__":
    main()
