#!/usr/bin/env python3
"""Project Five soak result visualisation (pass / problem / unclassified).

This script generates:
- session_catalog.json
- sensor_timeseries_normalized.csv
- dashboard_pass_vs_problem.html
- sensor_page.html
"""

from __future__ import annotations

import argparse
import csv
import datetime as _dt
import json
import math
import pathlib
import re
from collections import defaultdict
from typing import Any

KNOWN_DURATIONS = {
    600: "10m",
    3600: "1h",
    28800: "8h",
}

GROUP_LABELS = {
    "PASS": "通过组",
    "PROBLEM": "问题组",
    "UNCLASSIFIED": "未分类",
}

GROUP_FILTERS = ("PASS", "PROBLEM", "UNCLASSIFIED", "ALL")

SENSOR_CONVERSIONS = [
    {
        "raw": "bme280_temperature",
        "converted": "bme280_temperature_c",
        "raw_unit": "c",
        "display_unit": "°C",
        "factor": 0.01,
        "label": "温度",
        "group": "climate",
    },
    {
        "raw": "bme280_pressure",
        "converted": "bme280_pressure_hpa",
        "raw_unit": "pa",
        "display_unit": "hPa",
        "factor": 0.01,
        "label": "压力",
        "group": "climate",
    },
    {
        "raw": "bme280_humidity",
        "converted": "bme280_humidity_rh",
        "raw_unit": "%",
        "display_unit": "%RH",
        "factor": 0.001,
        "label": "湿度",
        "group": "climate",
    },
    {
        "raw": "veml7700_illuminance",
        "converted": "veml7700_illuminance_lux",
        "raw_unit": "millilux",
        "display_unit": "lux",
        "factor": 0.001,
        "label": "光照",
        "group": "climate",
    },
    {
        "raw": "adxl345_acceleration_x",
        "converted": "adxl345_acceleration_x_g",
        "raw_unit": "mg",
        "display_unit": "g",
        "factor": 0.001,
        "label": "加速度X",
        "group": "vibration",
        "alt": {
            "raw": "adxl345_acceleration_x_g_m_s2",
            "display_unit": "m/s²",
            "factor": 0.00980665,
        },
    },
    {
        "raw": "adxl345_acceleration_y",
        "converted": "adxl345_acceleration_y_g",
        "raw_unit": "mg",
        "display_unit": "g",
        "factor": 0.001,
        "label": "加速度Y",
        "group": "vibration",
        "alt": {
            "raw": "adxl345_acceleration_y_g_m_s2",
            "display_unit": "m/s²",
            "factor": 0.00980665,
        },
    },
    {
        "raw": "adxl345_acceleration_z",
        "converted": "adxl345_acceleration_z_g",
        "raw_unit": "mg",
        "display_unit": "g",
        "factor": 0.001,
        "label": "加速度Z",
        "group": "vibration",
        "alt": {
            "raw": "adxl345_acceleration_z_g_m_s2",
            "display_unit": "m/s²",
            "factor": 0.00980665,
        },
    },
    {
        "raw": "adxl345_resultant_rms",
        "converted": "adxl345_resultant_rms_g",
        "raw_unit": "mg",
        "display_unit": "g",
        "factor": 0.001,
        "label": "合成RMS",
        "group": "vibration",
    },
]

DISPLAY_METRICS = [
    ("bme280_temperature_c", "温度 (°C)", "温度"),
    ("bme280_pressure_hpa", "压力 (hPa)", "压力"),
    ("bme280_humidity_rh", "湿度 (%RH)", "湿度"),
    ("veml7700_illuminance_lux", "光照 (lux)", "光照"),
    ("adxl345_acceleration_x_g", "加速度X (g)", "加速度"),
    ("adxl345_acceleration_y_g", "加速度Y (g)", "加速度"),
    ("adxl345_acceleration_z_g", "加速度Z (g)", "加速度"),
    ("adxl345_resultant_rms_g", "合成加速度RMS (g)", "加速度"),
]

DISPLAY_UNITS = {
    "bme280_temperature_c": "°C",
    "bme280_pressure_hpa": "hPa",
    "bme280_humidity_rh": "%RH",
    "veml7700_illuminance_lux": "lux",
    "adxl345_acceleration_x_g": "g",
    "adxl345_acceleration_y_g": "g",
    "adxl345_acceleration_z_g": "g",
    "adxl345_resultant_rms_g": "g",
}


def to_float(value: Any) -> float | None:
    if isinstance(value, bool):
        return None
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)
    try:
        return float(str(value).strip())
    except (TypeError, ValueError):
        return None


def parse_dir_phase(path: pathlib.Path) -> str:
    tokens = path.name.split("_")
    for token in tokens:
        if token in ("smoke", "prerun", "formal", "self-test"):
            return token
    return "unknown"


def parse_session_manifest(path: pathlib.Path) -> dict[str, Any]:
    raw = path.read_text(encoding="utf-8", errors="ignore")
    text = raw.strip()
    if not text:
        return {}
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        pass

    result: dict[str, Any] = {}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
        elif ":" in line:
            key, value = line.split(":", 1)
        else:
            continue
        key = key.strip()
        value = value.strip()
        if not key:
            continue
        for converter in (int, float, str):
            try:
                converted = converter(value)
                result[key] = converted
                if converter is int:
                    break
            except Exception:
                continue
        else:
            result[key] = value
    return result


def parse_summary_file(path: pathlib.Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_samples_file(path: pathlib.Path) -> tuple[dict[str, Any], int, dict[str, Any]]:
    records = path.read_text(encoding="utf-8").splitlines()
    if not records:
        return {}, 0, {}
    first = json.loads(records[0].strip())
    sample_count = max(0, len(records) - 1)
    sample_meta: dict[str, Any] = {}
    first_sample: dict[str, Any] | None = None
    last_sample: dict[str, Any] | None = None
    if sample_count:
        for line in records[1:]:
            if not line.strip():
                continue
            sample = json.loads(line)
            if first_sample is None:
                first_sample = sample
            last_sample = sample
    if first_sample is not None:
        sample_meta["first"] = first_sample
    if last_sample is not None:
        sample_meta["last"] = last_sample
    return first, sample_count, sample_meta


def parse_sample_lines(path: pathlib.Path) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    line_count = 0
    first: dict[str, Any] = {}
    last: dict[str, Any] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        line_count += 1
        sample = json.loads(line)
        if not first:
            first = sample
        last = sample
        rows.append(sample)
    if not rows:
        return rows, {"sample_count": 0, "first": {}, "last": {}}
    return rows, {"sample_count": line_count, "first": first, "last": last}


def classify_duration(duration_s: int | None) -> str:
    if not duration_s:
        return "UNCLASSIFIED"
    return KNOWN_DURATIONS.get(duration_s, "UNCLASSIFIED")


def safe_string(value: Any, default: str = "") -> str:
    if isinstance(value, str):
        return value
    if value is None:
        return default
    return str(value)


def normalize_status(value: Any) -> str:
    if not value:
        return "UNKNOWN"
    return safe_string(value).upper().strip()


def build_session_entry(session_dir: pathlib.Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    summary_path = session_dir / "summary.json"
    samples_path = session_dir / "samples.jsonl"
    manifest_path = session_dir / "session_manifest.txt"
    manifest_json_path = session_dir / "session_manifest.json"
    raw_dir = session_dir / "raw"
    sensor_timeseries_path = raw_dir / "sensor_timeseries.jsonl"
    sample_path = raw_dir / "sensor_timeseries.csv"

    summary_data: dict[str, Any] = {}
    samples_metadata: dict[str, Any] = {}
    sample_count_samples = 0
    sample_meta: dict[str, Any] = {}
    failures: list[str] = []
    reviews: list[str] = []
    source = "unstructured"
    started = None
    phase = parse_dir_phase(session_dir)
    planned_duration_s: int | None = None
    sample_period_s: int | None = None
    status = "UNKNOWN"
    session_id = session_dir.name
    has_sensor_timeseries = False
    has_raw_only = False

    if summary_path.exists():
        summary_data = parse_summary_file(summary_path)
        status = normalize_status(summary_data.get("status"))
        failures = [safe_string(item) for item in summary_data.get("failures", []) if safe_string(item)]
        reviews = [safe_string(item) for item in summary_data.get("reviews", []) if safe_string(item)]
        session_payload = summary_data.get("session", {})
        if isinstance(session_payload, dict):
            planned_duration_s = session_payload.get("planned_duration_s")
            sample_period_s = session_payload.get("sample_period_s")
            started = safe_string(session_payload.get("started_at_utc"), "")
            phase = safe_string(session_payload.get("phase"), phase)
            session_id = safe_string(session_payload.get("session_id"), session_id)
        if samples_path.exists() is False:
            source = "summary_only"
        else:
            source = "structured"
    if samples_path.exists():
        samples_metadata, sample_count_samples, sample_meta = parse_samples_file(samples_path)
        meta_session = safe_string(samples_metadata.get("session_id"))
        if meta_session:
            session_id = meta_session
        status = normalize_status(samples_metadata.get("status", status))
        if planned_duration_s is None:
            planned_duration_s = samples_metadata.get("planned_duration_s")
            if isinstance(planned_duration_s, bool) or planned_duration_s is None:
                planned_duration_s = None
        if sample_period_s is None:
            sample_period_s = samples_metadata.get("sample_period_s")
            if isinstance(sample_period_s, bool) or sample_period_s is None:
                sample_period_s = None
        if not started:
            started = safe_string(samples_metadata.get("started_at_utc"), "")
        if not phase or phase == "unknown":
            phase = safe_string(samples_metadata.get("phase"), phase)
        source = "structured"

    manifest = None
    if manifest_json_path.exists():
        manifest = parse_session_manifest(manifest_json_path)
    elif manifest_path.exists():
        manifest = parse_session_manifest(manifest_path)
    if manifest:
        if not started:
            started = safe_string(manifest.get("started_at_utc", started), "")
        if phase in ("unknown", ""):
            phase = safe_string(manifest.get("phase", phase))
        if planned_duration_s is None:
            planned_duration_s = manifest.get("planned_duration_s")
        if sample_period_s is None:
            sample_period_s = manifest.get("sample_period_s")
        source = source if source in ("structured", "summary_only") else "manifest_only"

    if not isinstance(planned_duration_s, int) or planned_duration_s <= 0:
        parsed = None
        match = re.search(r"(\\d+)m", session_dir.name)
        if match:
            parsed = int(match.group(1))
            if parsed in (10, 60, 480):
                planned_duration_s = parsed * 60
        if parsed is None and "8h" in session_dir.name.lower():
            planned_duration_s = 28800
        elif parsed is None and "1h" in session_dir.name.lower():
            planned_duration_s = 3600
        elif parsed is None and "10m" in session_dir.name.lower():
            planned_duration_s = 600

    if isinstance(sample_period_s, bool) or sample_period_s is None:
        sample_period_s = 60

    if not isinstance(sample_count_samples, int) or sample_count_samples < 0:
        sample_count_samples = 0

    expected_sample_count = None
    if planned_duration_s and sample_period_s and sample_period_s > 0:
        expected_sample_count = max(1, math.ceil(planned_duration_s / sample_period_s))
        if sample_count_samples < expected_sample_count:
            source = "structured" if source == "structured" else source
            if "session 未达到规划采样目标" not in failures:
                failures.append(f"session 未达到规划采样目标: 实测 {sample_count_samples} vs 目标 {expected_sample_count}")

    has_sensor_timeseries = sensor_timeseries_path.exists() and sensor_timeseries_path.is_file()
    has_raw_only = raw_dir.exists() and any(raw_dir.iterdir())

    if source in {"summary_only", "manifest_only", "log_only", "unstructured"} and has_raw_only:
        source = "log_only"
    if source == "structured":
        duration_group = classify_duration(planned_duration_s if isinstance(planned_duration_s, int) else None)
    elif session_dir.name.startswith("p5_default_10m"):
        duration_group = "default_10m"
    else:
        duration_group = "UNCLASSIFIED"

    if source == "structured":
        is_full_run = expected_sample_count is not None and sample_count_samples >= expected_sample_count
    else:
        is_full_run = False

    if source == "structured" and status == "PASS" and is_full_run and duration_group in KNOWN_DURATIONS.values():
        group = "PASS"
    elif (
        source == "structured"
        and (status in {"FAIL", "REVIEW_REQUIRED"} or (expected_sample_count is not None and sample_count_samples < expected_sample_count))
    ):
        group = "PROBLEM"
    else:
        group = "UNCLASSIFIED"

    if group != "PASS" and status not in {"PASS", "FAIL", "REVIEW_REQUIRED"}:
        status = "UNKNOWN" if status == "UNKNOWN" else status

    entry = {
        "session_id": session_id,
        "session_dir": session_dir.name,
        "phase": phase,
        "planned_duration_s": planned_duration_s,
        "sample_period_s": sample_period_s,
        "sample_count": sample_count_samples,
        "expected_sample_count": expected_sample_count,
        "started_at_utc": started,
        "status": status,
        "source": source,
        "is_full_run": is_full_run,
        "duration_group": duration_group,
        "group": GROUP_LABELS[group],
        "group_key": group,
        "has_sensor_timeseries": has_sensor_timeseries,
        "failures": failures,
        "reviews": reviews,
        "has_raw_logs": has_raw_only,
        "sample_meta": sample_meta,
    }

    sensor_rows = []
    if has_sensor_timeseries:
        raw_rows = parse_sample_lines(sensor_timeseries_path)[0]
        for raw_row in raw_rows:
            normalized = {
                "session_id": session_id,
                "session_dir": session_dir.name,
                "phase": phase,
                "duration_group": duration_group,
                "group_key": group,
                "group": GROUP_LABELS[group],
                "status": status,
                "sample_index": int(raw_row.get("sample_index")) if to_float(raw_row.get("sample_index")) is not None else None,
                "elapsed_ms": to_float(raw_row.get("elapsed_ms")),
            }
            normalized["elapsed_s"] = None if normalized["elapsed_ms"] is None else normalized["elapsed_ms"] / 1000.0
            for metric in SENSOR_CONVERSIONS:
                raw_name = metric["raw"]
                raw_value = to_float(raw_row.get("bme280_temperature") if raw_name == "bme280_temperature" else raw_row.get(raw_name))
                if raw_value is None:
                    continue
                normalized[raw_name] = raw_value
                normalized[metric["converted"]] = raw_value * float(metric["factor"])
                if "alt" in metric and metric["alt"]:
                    normalized[metric["alt"]["raw"]] = raw_value * float(metric["alt"]["factor"])
            sensor_rows.append(normalized)

    return entry, sensor_rows


def flatten_sensor_rows(sensor_rows: list[dict[str, Any]]) -> tuple[list[str], list[dict[str, Any]], dict[str, list[dict[str, Any]]]]:
    fieldnames = [
        "session_id",
        "session_dir",
        "phase",
        "duration_group",
        "group_key",
        "group",
        "status",
        "sample_index",
        "elapsed_ms",
        "elapsed_s",
    ]
    for metric in SENSOR_CONVERSIONS:
        fieldnames.append(metric["raw"])
        fieldnames.append(metric["converted"])
        if "alt" in metric and metric["alt"]:
            fieldnames.append(metric["alt"]["raw"])

    if not fieldnames:
        fieldnames = [
            "session_id",
            "session_dir",
            "phase",
            "duration_group",
            "group_key",
            "group",
            "status",
            "sample_index",
            "elapsed_ms",
            "elapsed_s",
        ]
    # Preserve ordering and make de-duplicate deterministic
    deduped = []
    seen = set()
    for name in fieldnames:
        if name not in seen:
            deduped.append(name)
            seen.add(name)
    fieldnames = deduped
    by_session: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in sensor_rows:
        sid = row.get("session_id", "")
        by_session[sid].append(row)
    return fieldnames, sensor_rows, by_session


def build_series_by_session(rows: list[dict[str, Any]]) -> tuple[dict[str, Any], list[str]]:
    sessions: dict[str, Any] = {}
    for row in rows:
        sid = safe_string(row.get("session_id"))
        if sid not in sessions:
            sessions[sid] = {
                "session_id": sid,
                "duration_group": row.get("duration_group"),
                "group_key": row.get("group_key"),
                "group": row.get("group"),
                "status": row.get("status"),
                "phase": row.get("phase"),
                "session_dir": row.get("session_dir"),
                "metrics": defaultdict(list),
            }
        for metric in SENSOR_CONVERSIONS:
            conv_key = metric["converted"]
            raw_key = metric["raw"]
            if conv_key not in row and raw_key not in row:
                continue
            converted = to_float(row.get(conv_key))
            if converted is None:
                continue
            raw_value = row.get(raw_key)
            sample_index = row.get("sample_index")
            point = {
                "elapsed_s": row.get("elapsed_s"),
                "y": converted,
                "x": row.get("elapsed_s"),
                "raw": raw_value,
                "sample_index": sample_index,
            }
            sessions[sid]["metrics"][conv_key].append(point)
    metric_keys = [m["converted"] for m in SENSOR_CONVERSIONS]
    return sessions, metric_keys


def write_catalog(path: pathlib.Path, catalog: list[dict[str, Any]]) -> None:
    payload = {
        "generated_at_utc": _dt.datetime.now(_dt.timezone.utc).isoformat(),
        "session_count": len(catalog),
        "sessions": catalog,
    }
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def write_csv(path: pathlib.Path, fieldnames: list[str], rows: list[dict[str, Any]]) -> None:
    if not rows:
        fieldnames = [
            "session_id",
            "session_dir",
            "phase",
            "duration_group",
            "group_key",
            "group",
            "status",
            "sample_index",
            "elapsed_ms",
            "elapsed_s",
        ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            clean = {key: row.get(key, "") for key in fieldnames}
            writer.writerow(clean)


def html_escape(value: Any) -> str:
    return (
        str(value)
        .replace("&", "&amp;")
        .replace("\"", "&quot;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
    )


def build_dashboard_html(path: pathlib.Path, catalog: list[dict[str, Any]], series_by_session: dict[str, Any], metric_keys: list[str]) -> None:
    catalog_json = json.dumps(catalog, ensure_ascii=False)
    series_json = json.dumps(series_by_session, ensure_ascii=False)
    metric_json = json.dumps(DISPLAY_METRICS, ensure_ascii=False)
    metric_lookup_js = ", ".join(
        [f'"{key}":["{html_escape(label)}","{html_escape(DISPLAY_UNITS[key])}","{html_escape(tag)}"]' for key, label, tag in DISPLAY_METRICS]
    )

    groups = ", ".join([f'"{key}":"{value}"' for key, value in GROUP_LABELS.items()])

    body = f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>项目五长稳结果总览（通过/问题分层）</title>
  <script src="https://cdn.plot.ly/plotly-2.29.1.min.js"></script>
  <style>
    body {{
      margin: 0;
      font-family: "Noto Sans SC", "Segoe UI", Arial, sans-serif;
      background: linear-gradient(180deg, #f8fbff 0%, #eff2ff 55%, #f7f7fa 100%);
      color: #1f2937;
    }}
    .header {{
      position: sticky;
      top: 0;
      z-index: 10;
      padding: 12px 18px;
      background: #0f172a;
      color: #f8fafc;
      border-bottom: 1px solid #1e293b;
    }}
    .container {{
      max-width: 1320px;
      margin: 0 auto;
      padding: 18px;
    }}
    .controls, .metric-card, .group-grid {{
      background: #ffffff;
      border-radius: 10px;
      border: 1px solid #dbeafe;
      box-shadow: 0 8px 24px rgba(15, 23, 42, 0.06);
      padding: 14px;
    }}
    .controls {{
      display: flex;
      flex-wrap: wrap;
      gap: 14px;
      margin-top: 14px;
      align-items: center;
    }}
    .kpi-grid {{
      display: grid;
      grid-template-columns: repeat(4, minmax(140px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }}
    .metric-card {{
      margin-top: 14px;
      margin-bottom: 10px;
    }}
    .metric-card > h3 {{
      margin-top: 0;
    }}
    .chart-grid {{
      display: grid;
      grid-template-columns: minmax(0, 1fr);
      gap: 16px;
    }}
    @media (max-width: 1000px) {{
      .chart-grid {{ grid-template-columns: 1fr; }}
      .kpi-grid {{ grid-template-columns: 1fr 1fr; }}
    }}
    .kpi {{
      padding: 12px;
      border: 1px solid #dbeafe;
      border-radius: 8px;
      background: #f8fafe;
    }}
    .kpi .value {{
      font-size: 24px;
      margin-top: 8px;
      font-weight: 700;
      color: #1d4ed8;
    }}
    .muted {{
      color: #64748b;
      font-size: 12px;
    }}
    ul {{
      margin-top: 6px;
      padding-left: 20px;
    }}
  </style>
</head>
<body>
  <div class="header">
    <h1 style="margin: 0;">项目五 Soak 长稳结果总览（可交互）</h1>
  </div>
  <div class="container">
    <div class="controls">
      <label>组别：
        <select id="groupFilter">
          <option value="PASS">通过组</option>
          <option value="PROBLEM">问题组</option>
          <option value="UNCLASSIFIED">未分类</option>
          <option value="ALL">全部</option>
        </select>
      </label>
      <label>时长：
        <select id="durationFilter">
          <option value="ALL">全部</option>
          <option value="10m">10分钟</option>
          <option value="1h">1小时</option>
          <option value="8h">8小时</option>
          <option value="UNCLASSIFIED">未分类/其它</option>
        </select>
      </label>
    </div>

    <div class="kpi-grid">
      <div class="kpi"><div class="muted">会话总数</div><div id="kpiTotal" class="value">0</div></div>
      <div class="kpi"><div class="muted">通过组会话</div><div id="kpiPass" class="value">0</div></div>
      <div class="kpi"><div class="muted">问题组会话</div><div id="kpiProblem" class="value">0</div></div>
      <div class="kpi"><div class="muted">未分类</div><div id="kpiUnclassified" class="value">0</div></div>
    </div>

    <div class="metric-card">
      <h3>长稳时长覆盖（通过 / 问题）</h3>
      <div id="durationCoverage"></div>
    </div>

    <div class="metric-card">
      <h3>图注：当前图层 = 组别 × 时长；默认仅展示当前过滤组的通过组对比线图（同一时长内）</h3>
      <div class="chart-grid" id="durationCharts"></div>
    </div>

    <div class="metric-card">
      <h3>问题组明细（含失败原因）</h3>
      <ul id="problemList"></ul>
    </div>
  </div>

<script>
const catalog = {catalog_json};
const seriesBySession = {series_json};
const metricInfo = {{ {metric_lookup_js} }};
const groupLabel = {{ {groups} }};
const durationOrder = ["10m", "1h", "8h"];
const metricDisplay = {metric_json};

function compactSessionLabel(info) {{
  const source = info.session_dir || info.session_id || "unknown";
  const stamp = source.slice(-5);
  const status = info.status === "REVIEW_REQUIRED" ? "REVIEW" : (info.status || "UNKNOWN");
  return `${{stamp}} · ${{info.phase || "unknown"}} · ${{status}}`;
}}

function fullSessionLabel(info) {{
  return `${{info.session_dir || "unknown"}}<br>${{info.session_id || "unknown"}}<br>status=${{info.status || "UNKNOWN"}}`;
}}

function getFilteredSessions() {{
  const selectedGroup = document.getElementById("groupFilter").value;
  const selectedDuration = document.getElementById("durationFilter").value;
  const sessions = catalog.filter((entry) => {{
    const groupMatch = selectedGroup === "ALL" || entry.group_key === selectedGroup;
    const durationMatch = selectedDuration === "ALL" || entry.duration_group === selectedDuration;
    return groupMatch && durationMatch;
  }});
  return sessions;
}}

function applyKPI() {{
  const total = catalog.length;
  const pass = catalog.filter((x) => x.group_key === "PASS").length;
  const problem = catalog.filter((x) => x.group_key === "PROBLEM").length;
  const unclass = catalog.filter((x) => x.group_key === "UNCLASSIFIED").length;
  document.getElementById("kpiTotal").textContent = total;
  document.getElementById("kpiPass").textContent = pass;
  document.getElementById("kpiProblem").textContent = problem;
  document.getElementById("kpiUnclassified").textContent = unclass;
}}

function buildDurationCoverage() {{
  const countPass = {{"10m": 0, "1h": 0, "8h": 0, "UNCLASSIFIED": 0}};
  const countProblem = {{"10m": 0, "1h": 0, "8h": 0, "UNCLASSIFIED": 0}};
  catalog.forEach((session) => {{
    const key = session.duration_group in countPass ? session.duration_group : "UNCLASSIFIED";
    if (session.group_key === "PASS") {{
      countPass[key] += 1;
    }} else if (session.group_key === "PROBLEM") {{
      countProblem[key] += 1;
    }}
  }});
  const labels = ["10m", "1h", "8h", "UNCLASSIFIED"];
  const passTrace = {{
    x: labels,
    y: labels.map((k) => countPass[k]),
    name: "通过组",
    type: "bar",
    marker: {{ color: "#16a34a" }},
  }};
  const problemTrace = {{
    x: labels,
    y: labels.map((k) => countProblem[k]),
    name: "问题组",
    type: "bar",
    marker: {{ color: "#dc2626" }},
  }};
  const fig = {{
    data: [passTrace, problemTrace],
    layout: {{
      barmode: "stack",
      margin: {{ t: 20 }},
      xaxis: {{ title: "规划时长" }},
      yaxis: {{ title: "会话数量" }},
      legend: {{ orientation: "h" }},
    }},
  }};
  Plotly.newPlot("durationCoverage", [passTrace, problemTrace], fig.layout, {{responsive: true}});
}}

function buildDashboardCharts() {{
  const group = document.getElementById("groupFilter").value;
  const duration = document.getElementById("durationFilter").value;
  const targetDurations = duration === "ALL" ? durationOrder : [duration];
  const container = document.getElementById("durationCharts");
  container.innerHTML = "";
  const hasNoDuration = ["10m","1h","8h"].filter((d) => targetDurations.includes(d));

  hasNoDuration.forEach((dur) => {{
    const metricCards = [];
    const header = document.createElement("div");
    header.style.gridColumn = "1 / -1";
    const title = document.createElement("h3");
    const totalPass = catalog.filter((s) => s.group_key === "PASS" && s.duration_group === dur).length;
    const totalProblem = catalog.filter((s) => s.group_key === "PROBLEM" && s.duration_group === dur).length;
    const totalUnclass = catalog.filter((s) => s.group_key === "UNCLASSIFIED" && s.duration_group === dur).length;
    const label = dur === "10m" ? "10分钟" : dur === "1h" ? "1小时" : "8小时";
    title.textContent = `${{label}}（通过组:${{totalPass}} 问题组:${{totalProblem}} 未分类:${{totalUnclass}}）`;
    if (totalPass === 0 && totalProblem > 0) {{
      title.textContent += "；无满足 PASS 与完整采样门的会话";
    }}
    header.appendChild(title);
    container.appendChild(header);

    metricDisplay.forEach((metric) => {{
      const [metricKey, yTitle, _metricGroup] = metric;
      const chartId = `chart_${{dur}}_${{metricKey}}`;
      const chartNode = document.createElement("div");
      chartNode.id = chartId;
      chartNode.style.height = "460px";
      container.appendChild(chartNode);
      metricCards.push({{metricKey, chartId}});

      const traces = [];
      Object.keys(seriesBySession).forEach((sid) => {{
        const info = seriesBySession[sid];
        if (info.duration_group !== dur) {{
          return;
        }}
        if (group !== "ALL" && info.group_key !== group) {{
          return;
        }}
        const points = info.metrics[metricKey] || [];
        const usable = points.filter((point) => point.y !== null && point.x !== null);
        if (!usable.length) {{
          return;
        }}
        traces.push({{
          x: usable.map((item) => item.x / 60),
          y: usable.map((item) => item.y),
          customdata: usable.map((item) => item.raw),
          mode: "lines",
          type: "scatter",
          name: compactSessionLabel(info),
          meta: fullSessionLabel(info),
          line: {{ width: 2 }},
          hovertemplate: `%{{meta}}<br>时间: %{{x:.1f}} min<br>值: %{{y:.6g}}<br>原始值: %{{customdata}}<extra></extra>`
        }});
      }});

      if (traces.length === 0) {{
        Plotly.newPlot(chartId, [], {{
          title: {{text: `${{metricInfo[metricKey] ? metricInfo[metricKey][0] : metricKey}}（无可用数据）`}},
          annotations: [{{
            x: 0.5,
            y: 0.5,
            xref: "paper",
            yref: "paper",
            text: "该过滤条件下无可视化数据",
            showarrow: false,
          }}],
        }}, {{responsive: true}});
        return;
      }}
      Plotly.newPlot(chartId, traces, {{
        margin: {{ t: 86, r: 24, b: 68, l: 76 }},
        title: {{text: `${{metricInfo[metricKey] ? metricInfo[metricKey][0] : metricKey}}（单位: ${{metricInfo[metricKey] ? metricInfo[metricKey][1] : ""}}）`}},
        xaxis: {{ title: "elapsed (min)" }},
        yaxis: {{ title: `${{metricInfo[metricKey] ? metricInfo[metricKey][1] : ""}}` }},
        legend: {{
          orientation: "h",
          x: 0,
          xanchor: "left",
          y: 1.04,
          yanchor: "bottom",
          font: {{ size: 10 }},
          itemclick: "toggle",
          itemdoubleclick: "toggleothers",
        }},
      }}, {{responsive: true}});
    }});
  }});
}}

function buildProblemList() {{
  const ul = document.getElementById("problemList");
  ul.innerHTML = "";
  const filtered = catalog.filter((session) => session.group_key === "PROBLEM");
  if (!filtered.length) {{
    const item = document.createElement("li");
    item.textContent = "当前不存在问题组会话。";
    ul.appendChild(item);
    return;
  }}
  filtered.forEach((session) => {{
    const li = document.createElement("li");
    const fail = (session.failures && session.failures.length) ? session.failures.join("; ") : "未给出失败明细";
    const review = (session.reviews && session.reviews.length) ? session.reviews.join("; ") : "";
    li.innerHTML = `<strong>${{session.session_id}}</strong>（${{session.duration_group}} / ${{session.phase}}）：${{fail}}`
      + (review ? `<br/><span style="color:#6b7280; font-size:12px;">REVIEW: ${{review}}</span>` : "");
    ul.appendChild(li);
  }});
}}

function refresh() {{
  applyKPI();
  buildDurationCoverage();
  buildDashboardCharts();
  buildProblemList();
}}

document.getElementById("groupFilter").addEventListener("change", refresh);
document.getElementById("durationFilter").addEventListener("change", refresh);
refresh();
</script>
</body>
</html>"""

    path.write_text(body, encoding="utf-8")


def build_sensor_page_html(path: pathlib.Path, catalog: list[dict[str, Any]], series_by_session: dict[str, Any], metric_keys: list[str]) -> None:
    catalog_json = json.dumps(catalog, ensure_ascii=False)
    series_json = json.dumps(series_by_session, ensure_ascii=False)
    metric_lookup_js = ", ".join(
        [f'"{key}":["{html_escape(label)}","{html_escape(DISPLAY_UNITS[key])}","{html_escape(tag)}"]' for key, label, tag in DISPLAY_METRICS]
    )
    groups = ", ".join([f'"{key}":"{value}"' for key, value in GROUP_LABELS.items()])
    metric_json = json.dumps(DISPLAY_METRICS, ensure_ascii=False)

    body = f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>项目五传感器页（原始值+展示值双轨）</title>
  <script src="https://cdn.plot.ly/plotly-2.29.1.min.js"></script>
  <style>
    body {{
      margin: 0;
      font-family: "Noto Sans SC", "Segoe UI", Arial, sans-serif;
      background: #f8fafc;
      color: #0f172a;
    }}
    .header {{
      background: #111827;
      color: #e5e7eb;
      padding: 12px 18px;
    }}
    .container {{
      max-width: 1600px;
      margin: 0 auto;
      padding: 16px;
    }}
    .topbar {{
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
      padding: 12px;
      background: #ffffff;
      border: 1px solid #d1d5db;
      border-radius: 8px;
    }}
    .section {{
      margin-top: 14px;
      background: #fff;
      border: 1px solid #dbeafe;
      border-radius: 10px;
      padding: 14px;
      box-shadow: 0 6px 18px rgba(15, 23, 42, 0.05);
    }}
    .metric-section {{
      margin-top: 18px;
    }}
    .metric-section h3 {{
      margin: 6px 0;
    }}
    .chart-grid {{
      display: grid;
      grid-template-columns: minmax(0, 3fr) minmax(300px, 1fr);
      gap: 14px;
    }}
    @media (max-width: 1100px) {{
      .chart-grid {{ grid-template-columns: 1fr; }}
    }}
  </style>
</head>
<body>
  <div class="header">
    <h1 style="margin: 0;">项目五传感器页</h1>
    <div style="font-size: 13px; color: #cbd5e1; margin-top: 4px;">
      支持原始值追溯（hover 显示），默认坐标为标准单位展示值。支持组别与时长过滤。
    </div>
  </div>
  <div class="container">
    <div class="topbar">
      <label>组别过滤:
        <select id="groupFilterSensor">
          <option value="PASS">通过组</option>
          <option value="PROBLEM">问题组</option>
          <option value="UNCLASSIFIED">未分类</option>
          <option value="ALL">全部</option>
        </select>
      </label>
      <label>时长过滤:
        <select id="durationFilterSensor">
          <option value="ALL">全部</option>
          <option value="10m">10分钟</option>
          <option value="1h">1小时</option>
          <option value="8h">8小时</option>
          <option value="UNCLASSIFIED">未分类/其它</option>
        </select>
      </label>
    </div>
    <div class="section">
      <p>说明：图层数据类型是“通过组 / 问题组 / 未分类”，横轴统一采用 elapsed (min)。</p>
    </div>
    <div id="metricContainer"></div>
  </div>
  <script>
const catalog = {catalog_json};
const seriesBySession = {series_json};
const metricInfo = {{ {metric_lookup_js} }};
const durationOrder = ["10m", "1h", "8h"];
const metricDisplay = {metric_json};
const groups = {{ {groups} }};

function compactSessionLabel(info) {{
  const source = info.session_dir || info.session_id || "unknown";
  const stamp = source.slice(-5);
  const status = info.status === "REVIEW_REQUIRED" ? "REVIEW" : (info.status || "UNKNOWN");
  return `${{stamp}} · ${{info.phase || "unknown"}} · ${{status}}`;
}}

function fullSessionLabel(info) {{
  return `${{info.session_dir || "unknown"}}<br>${{info.session_id || "unknown"}}<br>status=${{info.status || "UNKNOWN"}}`;
}}

function filteredSessions() {{
  const g = document.getElementById("groupFilterSensor").value;
  const d = document.getElementById("durationFilterSensor").value;
  return Object.entries(seriesBySession).filter(([_, info]) => {{
    const groupMatch = g === "ALL" || info.group_key === g;
    const durationMatch = d === "ALL" || info.duration_group === d || (d === "UNCLASSIFIED" && info.duration_group === "UNCLASSIFIED");
    return groupMatch && durationMatch;
  }});
}}

function formatDurationLabel(v) {{
  if (v === "10m") return "10分钟";
  if (v === "1h") return "1小时";
  if (v === "8h") return "8小时";
  return "未分类/其它";
}}

function buildCharts() {{
  const container = document.getElementById("metricContainer");
  container.innerHTML = "";
  const selected = filteredSessions();
  if (!selected.length) {{
    const section = document.createElement("div");
    section.className = "section";
    section.textContent = "当前筛选条件下无可用传感器数据。";
    container.appendChild(section);
    return;
  }}

  metricDisplay.forEach((metric) => {{
    const metricKey = metric[0];
    const metricTitle = metric[1];
    const metricTag = metric[2];
    const section = document.createElement("div");
    section.className = "section metric-section";
    const title = document.createElement("h3");
    title.textContent = `${{metricTitle}} ；hover 显示原始值`;
    section.appendChild(title);
    const grid = document.createElement("div");
    grid.className = "chart-grid";
    const lineDiv = document.createElement("div");
    const boxDiv = document.createElement("div");
    const gid = `${{metricKey}}_line`;
    const bid = `${{metricKey}}_box`;
    lineDiv.id = gid;
    boxDiv.id = bid;
    lineDiv.style.height = "460px";
    boxDiv.style.height = "460px";
    grid.appendChild(lineDiv);
    grid.appendChild(boxDiv);
    section.appendChild(grid);
    container.appendChild(section);

    const selectedSessions = [];
    const lineTraces = [];
    const boxValues = [];

    selected.forEach(([sid, info]) => {{
      const points = info.metrics[metricKey] || [];
      const valid = points.filter((point) => point.y !== null && point.x !== null);
      if (!valid.length) {{
        return;
      }}
      selectedSessions.push(sid);
      lineTraces.push({{
        name: compactSessionLabel(info),
        meta: fullSessionLabel(info),
        x: valid.map((p) => p.x / 60),
        y: valid.map((p) => p.y),
        customdata: valid.map((p) => p.raw),
        mode: "lines",
        type: "scatter",
        line: {{ width: 2 }},
        hovertemplate: "%{{meta}}<br>时间: %{{x:.1f}} min<br>值: %{{y:.6g}}<br>原始值: %{{customdata}}<extra></extra>"
      }});
      boxValues.push(valid.map((p) => p.y));
    }});

    Plotly.newPlot(gid, lineTraces, {{
      margin: {{ t: 86, r: 24, b: 68, l: 76 }},
      xaxis: {{ title: "elapsed (min)" }},
      yaxis: {{
        title: metricInfo[metricKey] ? metricInfo[metricKey][1] : "",
      }},
      title: {{ text: `${{metricTitle}}` }},
      legend: {{
        orientation: "h",
        x: 0,
        xanchor: "left",
        y: 1.04,
        yanchor: "bottom",
        font: {{ size: 10 }},
        itemclick: "toggle",
        itemdoubleclick: "toggleothers",
      }},
    }}, {{responsive: true}});

    if (!selectedSessions.length) {{
      Plotly.newPlot(bid, [], {{
        annotations: [{{
          x: 0.5,
          y: 0.5,
          text: "当前筛选条件下无箱线图数据",
          showarrow: false,
          xref: "paper",
          yref: "paper",
        }}]
      }}, {{responsive: true}});
      return;
    }}

    const boxTraces = selected.map(([_, info]) => {{
      const metricVals = info.metrics[metricKey] ? info.metrics[metricKey].map((point) => point.y).filter((v) => v !== null && v !== undefined) : [];
      return {{
        y: metricVals,
        type: "box",
        name: compactSessionLabel(info),
        boxpoints: false
      }};
    }}).filter((trace) => trace.y && trace.y.length);

    Plotly.newPlot(bid, boxTraces, {{
      margin: {{ t: 58, r: 20, b: 88, l: 64 }},
      yaxis: {{ title: metricInfo[metricKey] ? metricInfo[metricKey][1] : "" }},
      title: {{ text: `${{metricTag}} 带状图` }},
      showlegend: false,
    }}, {{responsive: true}});
  }});
}}

document.getElementById("groupFilterSensor").addEventListener("change", buildCharts);
document.getElementById("durationFilterSensor").addEventListener("change", buildCharts);
buildCharts();
</script>
</body>
</html>"""

    path.write_text(body, encoding="utf-8")


def collect_sessions(soak_root: pathlib.Path) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    catalog: list[dict[str, Any]] = []
    all_sensor_rows: list[dict[str, Any]] = []
    for item in sorted(soak_root.iterdir(), key=lambda x: x.name):
        if not item.is_dir():
            continue
        if item.name.startswith("."):
            continue
        if item.name.endswith(".tar.gz") or item.suffix == ".gz":
            continue
        if not item.name.startswith("p5_"):
            continue
        try:
            entry, sensor_rows = build_session_entry(item)
        except Exception:
            # keep pipeline robust when a single session is malformed
            continue
        catalog.append(entry)
        all_sensor_rows.extend(sensor_rows)
    return catalog, all_sensor_rows


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate interactive visualization artifacts for P5 soak sessions.")
    parser.add_argument("soak_root", nargs="?", default=".", help="Directory containing p5_* soak sessions")
    parser.add_argument("--output-dir", default=None, help="Output directory for generated artifacts")
    args = parser.parse_args()

    soak_root = pathlib.Path(args.soak_root).resolve()
    output_dir = pathlib.Path(args.output_dir).resolve() if args.output_dir else soak_root
    output_dir.mkdir(parents=True, exist_ok=True)

    catalog, sensor_rows = collect_sessions(soak_root)
    # deterministic order
    catalog.sort(key=lambda item: (item.get("session_dir", "")))
    sensor_rows.sort(key=lambda item: (item.get("session_id", ""), item.get("elapsed_ms", 0)))

    fieldnames, flat_rows, rows_by_session = flatten_sensor_rows(sensor_rows)
    write_catalog(output_dir / "session_catalog.json", catalog)
    write_csv(output_dir / "sensor_timeseries_normalized.csv", fieldnames, flat_rows)

    series_by_session, metric_keys = build_series_by_session(flat_rows)
    build_dashboard_html(output_dir / "dashboard_pass_vs_problem.html", catalog, series_by_session, metric_keys)
    build_sensor_page_html(output_dir / "sensor_page.html", catalog, series_by_session, metric_keys)

    print(f"已生成 {output_dir / 'session_catalog.json'}")
    print(f"已生成 {output_dir / 'sensor_timeseries_normalized.csv'}")
    print(f"已生成 {output_dir / 'dashboard_pass_vs_problem.html'}")
    print(f"已生成 {output_dir / 'sensor_page.html'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
