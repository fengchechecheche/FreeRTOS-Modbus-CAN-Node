#!/usr/bin/env python3
"""Validate soak visualization outputs against the project acceptance scenarios."""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import datetime as dt
from typing import Any

import csv


PASS = "PASS"
REVIEW_REQUIRED = "REVIEW_REQUIRED"
FAIL = "FAIL"


def load_json(path: pathlib.Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_csv(path: pathlib.Path) -> list[dict[str, Any]]:
    if not path.exists():
        return []
    with path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        return list(reader)


def to_float(value: Any) -> float | None:
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        value = value.strip()
        if not value:
            return None
        try:
            return float(value)
        except ValueError:
            return None
    return None


def check_close(actual: float, expected: float, rel: float = 1e-6, abs_tol: float = 1e-9) -> bool:
    return math.isclose(actual, expected, rel_tol=rel, abs_tol=abs_tol)


def run_unit_checks(catalog_rows: list[dict[str, Any]], csv_rows: list[dict[str, Any]]) -> dict[str, Any]:
    cases = [
        ("bme280_temperature", "bme280_temperature_c", 0.01, "23.30"),
        ("bme280_humidity", "bme280_humidity_rh", 0.001, "33.0"),
        ("adxl345_acceleration_x", "adxl345_acceleration_x_g", 0.001, "9.80665*"),
    ]

    findings: list[str] = []
    ok = True
    for raw_key, converted_key, factor, sample_hint in cases:
        row_found = None
        for row in csv_rows:
            raw_value = to_float(row.get(raw_key))
            converted_value = to_float(row.get(converted_key))
            if raw_value is not None and converted_value is not None:
                row_found = (raw_value, converted_value)
                break
        if row_found is None:
            ok = False
            findings.append(f"{raw_key}/{converted_key} 没有可用样本对")
            continue
        raw_value, converted_value = row_found
        if not check_close(converted_value, raw_value * factor, rel=1e-6, abs_tol=1e-9):
            ok = False
            findings.append(
                f"{raw_key} -> {converted_key} 比例不一致: raw={raw_value} converted={converted_value}, "
                f"expected={raw_value * factor}"
            )
            continue
        findings.append(
            f"{raw_key}: {raw_value} -> {converted_key} {converted_value} (factor={factor}, 示例提示={sample_hint})"
        )

    status_counts = {
        "sample_rows": len(csv_rows),
        "session_rows": len(catalog_rows),
    }
    return {"ok": ok, "details": findings, "metrics": status_counts}


def run_group_split_checks(catalog_rows: list[dict[str, Any]]) -> dict[str, Any]:
    findings: list[str] = []
    ok = True

    for row in catalog_rows:
        status = str(row.get("status", "")).upper()
        group = str(row.get("group_key", ""))
        duration = row.get("duration_group")
        is_full_run = bool(row.get("is_full_run"))
        sample_count = row.get("sample_count", 0)
        expected = row.get("expected_sample_count")
        sid = row.get("session_id", "")

        if status == PASS:
            if duration in {"10m", "1h", "8h"} and not is_full_run:
                if group == "PASS":
                    ok = False
                    findings.append(f"{sid} 被标记为通过组但采样未满({sample_count}/{expected})")
            if duration == "8h" and group != "PASS" and is_full_run:
                ok = False
                findings.append(f"{sid} 是8小时PASS但不在通过组")
        if status in {FAIL, REVIEW_REQUIRED}:
            if group == "PASS":
                ok = False
                findings.append(f"{sid} 状态{status}却在通过组")
        if status == REVIEW_REQUIRED and group == "PASS":
            ok = False
            findings.append(f"{sid} status=REVIEW_REQUIRED 进入通过组，违反规则")

        expected_full = expected is not None and expected > 0 and sample_count >= expected
        if not expected_full and status == PASS and group == "PASS":
            ok = False
            findings.append(f"{sid} PASS 但 sample_count 未满足采样计划")

    pass_count = sum(1 for row in catalog_rows if row.get("group_key") == "PASS")
    problem_count = sum(1 for row in catalog_rows if row.get("group_key") == "PROBLEM")
    findings.append(f"PASS={pass_count}, PROBLEM={problem_count}（分组条目统计）")
    return {"ok": ok, "details": findings}


def run_boundary_checks(catalog_rows: list[dict[str, Any]]) -> dict[str, Any]:
    findings: list[str] = []
    ok = True

    default_10m_in_pass = [row for row in catalog_rows if str(row.get("session_dir", "")).startswith("p5_default_10m") and row.get("group_key") == "PASS"]
    if default_10m_in_pass:
        ok = False
        findings.append("default_10m 会话在通过组中出现")

    can_only_pass = [
        row for row in catalog_rows
        if row.get("source") == "log_only" and row.get("group_key") == "PASS"
    ]
    if can_only_pass:
        ok = False
        findings.append("日志-only 会话在通过组筛选中仍出现")

    problem_rows = [row for row in catalog_rows if row.get("group_key") == "PROBLEM"]
    if problem_rows:
        missing_reason = 0
        for row in problem_rows:
            if not row.get("failures") and not row.get("reviews"):
                missing_reason += 1
        if missing_reason:
            findings.append(f"问题组有 {missing_reason} 个条目缺失失败原因说明")
    else:
        findings.append("当前未发现问题组条目（验收时请确认输入目录内样本）")

    interupted = [
        row for row in catalog_rows
        if row.get("group_key") == "PROBLEM"
        and (not row.get("is_full_run") or row.get("status") in {FAIL, REVIEW_REQUIRED})
    ]
    findings.append(f"问题组候选可视化条目: {len(interupted)}")
    return {"ok": ok, "details": findings}


def run_consistency_checks(
    dashboard_path: pathlib.Path,
    sensor_path: pathlib.Path,
    catalog_rows: list[dict[str, Any]],
) -> dict[str, Any]:
    findings: list[str] = []
    ok = True

    dashboard_text = dashboard_path.read_text(encoding="utf-8") if dashboard_path.exists() else ""
    sensor_text = sensor_path.read_text(encoding="utf-8") if sensor_path.exists() else ""

    if "elapsed (min)" not in dashboard_text or "elapsed (min)" not in sensor_text:
        ok = False
        findings.append("未检测到统一横轴标签 elapsed (min)")

    if "x: valid.map((item) => item.x / 60)" not in sensor_text and "x / 60" not in sensor_text:
        ok = False
        findings.append("sensor_page 未出现统一秒/分钟归一化逻辑（可能横轴非 elapsed 秒/分钟）")

    if "elapsed (s)" not in dashboard_text and "hovertemplate" not in dashboard_text:
        findings.append("dashboard hover 与轴单位说明异常（仅提示，不判失败）")

    dashboard_contract = {
        "if (info.duration_group !== dur)": "dashboard 各时长面板未强制隔离自身时长",
        "compactSessionLabel(info)": "dashboard 未使用紧凑会话图例",
        'itemdoubleclick: "toggleothers"': "dashboard 图例不支持双击隔离曲线",
        'chartNode.style.height = "460px"': "dashboard 绘图区高度未达到清晰展示基线",
        "grid-template-columns: minmax(0, 1fr)": "dashboard 指标图未使用单列宽图布局",
    }
    for fragment, message in dashboard_contract.items():
        if fragment not in dashboard_text:
            ok = False
            findings.append(message)

    sensor_contract = {
        "compactSessionLabel(info)": "sensor_page 未使用紧凑会话图例",
        'itemdoubleclick: "toggleothers"': "sensor_page 图例不支持双击隔离曲线",
        'lineDiv.style.height = "460px"': "sensor_page 绘图区高度未达到清晰展示基线",
        "showlegend: false": "sensor_page 箱线图未关闭冗余图例",
    }
    for fragment, message in sensor_contract.items():
        if fragment not in sensor_text:
            ok = False
            findings.append(message)

    if ok:
        findings.append("时长面板隔离、紧凑图例、曲线隔离交互和宽图布局约束通过")

    pass_sessions = [
        row for row in catalog_rows
        if row.get("group_key") == "PASS"
    ]
    for duration in ("10m", "1h", "8h"):
        has = any(row.get("duration_group") == duration and row.get("group_key") == "PASS" for row in catalog_rows)
        if not has and pass_sessions:
            findings.append(f"注意：通过组内缺少 {duration} 的样本（如有预期请核对输入）")

    return {"ok": ok, "details": findings}


def summarize(ok: bool, name: str, details: list[str]) -> dict[str, Any]:
    return {
        "case": name,
        "ok": ok,
        "details": details,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Run project soak visualization acceptance checks.")
    parser.add_argument(
        "output_dir",
        nargs="?",
        default=".",
        help="Directory containing session_catalog.json and generated html/csv",
    )
    args = parser.parse_args()

    output_dir = pathlib.Path(args.output_dir).resolve()
    catalog_path = output_dir / "session_catalog.json"
    csv_path = output_dir / "sensor_timeseries_normalized.csv"
    dashboard_path = output_dir / "dashboard_pass_vs_problem.html"
    sensor_path = output_dir / "sensor_page.html"

    if not catalog_path.exists():
        print(f"缺少 {catalog_path}")
        return 1

    catalog_payload = load_json(catalog_path)
    catalog_rows = catalog_payload.get("sessions", catalog_payload if isinstance(catalog_payload, list) else [])
    csv_rows = load_csv(csv_path)

    unit = run_unit_checks(catalog_rows, csv_rows)
    group = run_group_split_checks(catalog_rows)
    boundary = run_boundary_checks(catalog_rows)
    consistency = run_consistency_checks(dashboard_path, sensor_path, catalog_rows)

    all_cases = [
        summarize(unit["ok"], "传感器单位转换", unit["details"]),
        summarize(group["ok"], "组别分流", group["details"]),
        summarize(boundary["ok"], "可视化边界", boundary["details"]),
        summarize(consistency["ok"], "跨组对比一致性", consistency["details"]),
    ]

    report = {
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "output_dir": str(output_dir),
        "session_catalog_path": str(catalog_path),
        "sensor_rows": len(csv_rows),
        "session_rows": len(catalog_rows),
        "cases": all_cases,
        "result": all(c["ok"] for c in all_cases),
    }

    report_path = output_dir / "visualization_acceptance_report.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"已生成 {report_path}")

    failed = [case for case in all_cases if not case["ok"]]
    if failed:
        for case in failed:
            print(f"FAIL: {case['case']}")
        return 1
    print("PASS: 全部验收场景通过")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
