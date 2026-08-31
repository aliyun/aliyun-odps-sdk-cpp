#!/usr/bin/env python3
"""
Parse benchmark report.json and store results into MySQL for Grafana visualization.

Usage:
    python3 store_report_to_mysql.py <report.json> --db-host HOST --db-user USER --db-pass PASS \
        [--db-name NAME] [--db-port PORT] [--server-version V55.tunnel.RC4] [--test-env test-trunk]
"""

import argparse
import json
import os
import subprocess
import sys

import pymysql

# ---------------------------------------------------------------------------
# DB defaults (only for db-name and db-port)
# ---------------------------------------------------------------------------
_DB_DEFAULTS = {
    "database": "odps_tunnel_perf",
    "port": 3306,
}

# ---------------------------------------------------------------------------
# DDL
# ---------------------------------------------------------------------------
CREATE_REPORT_TABLE = """
CREATE TABLE IF NOT EXISTS benchmark_report (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    report_time     DATETIME        NOT NULL,
    sdk_branch      VARCHAR(128)    NOT NULL DEFAULT '',
    server_version  VARCHAR(64)     NOT NULL DEFAULT '',
    test_env        VARCHAR(64)     NOT NULL DEFAULT '',
    total_cases     INT             NOT NULL DEFAULT 0,
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_test_env (test_env)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
"""

CREATE_CASE_TABLE = """
CREATE TABLE IF NOT EXISTS benchmark_case (
    id                          BIGINT AUTO_INCREMENT PRIMARY KEY,
    report_id                   BIGINT          NOT NULL,
    report_time                 DATETIME        NOT NULL,
    suite_name                  VARCHAR(128)    NOT NULL DEFAULT '',
    case_type                   VARCHAR(32)     NOT NULL DEFAULT 'benchmark',
    mode                        VARCHAR(32)     NOT NULL DEFAULT '',
    project                     VARCHAR(128)    NOT NULL DEFAULT '',
    table_name                  VARCHAR(128)    NOT NULL DEFAULT '',
    threads                     INT             NOT NULL DEFAULT 1,
    rows_per_run                BIGINT          NOT NULL DEFAULT 0,
    -- performance
    total_ops                   BIGINT          NOT NULL DEFAULT 0,
    total_rows                  BIGINT          NOT NULL DEFAULT 0,
    total_memory_size_bytes     BIGINT          NOT NULL DEFAULT 0,
    total_wire_size_bytes       BIGINT          NOT NULL DEFAULT 0,
    wall_clock_time_ms          BIGINT          NOT NULL DEFAULT 0,
    qps                         DOUBLE          NOT NULL DEFAULT 0,
    rps                         DOUBLE          NOT NULL DEFAULT 0,
    memory_throughput_bps       DOUBLE          NOT NULL DEFAULT 0,
    wire_throughput_bps         DOUBLE          NOT NULL DEFAULT 0,
    error_count                 INT             NOT NULL DEFAULT 0,
    -- latency (ms)
    avg_ms                      DOUBLE          NOT NULL DEFAULT 0,
    min_ms                      DOUBLE          NOT NULL DEFAULT 0,
    max_ms                      DOUBLE          NOT NULL DEFAULT 0,
    std_dev_ms                  DOUBLE          NOT NULL DEFAULT 0,
    p50_ms                      DOUBLE          NOT NULL DEFAULT 0,
    p90_ms                      DOUBLE          NOT NULL DEFAULT 0,
    p95_ms                      DOUBLE          NOT NULL DEFAULT 0,
    p99_ms                      DOUBLE          NOT NULL DEFAULT 0,
    p999_ms                     DOUBLE          NOT NULL DEFAULT 0,
    -- time breakdown (nullable, varies by mode)
    avg_session_create_ms       DOUBLE          NULL,
    avg_split_get_ms            DOUBLE          NULL,
    avg_stream_create_ms        DOUBLE          NULL,
    avg_read_ms                 DOUBLE          NULL,
    avg_write_ms                DOUBLE          NULL,
    avg_data_gen_ms             DOUBLE          NULL,
    avg_commit_ms               DOUBLE          NULL,
    avg_close_ms                DOUBLE          NULL,
    -- system resources
    memory_used_kb              BIGINT          NULL,
    peak_memory_kb              BIGINT          NULL,
    avg_cpu_percent             DOUBLE          NULL,
    peak_cpu_percent            DOUBLE          NULL,
    -- coordinated omission
    co_total_omission_ms        DOUBLE          NULL,
    co_avg_omission_ms          DOUBLE          NULL,
    co_corrected_avg_latency_ms DOUBLE          NULL,
    -- index
    INDEX idx_report_id (report_id),
    INDEX idx_suite_time (suite_name, report_time),
    INDEX idx_report_time (report_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
"""


# ---------------------------------------------------------------------------
# Git helpers
# ---------------------------------------------------------------------------
def get_sdk_branch():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"], stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return ""


# ---------------------------------------------------------------------------
# Insert helpers
# ---------------------------------------------------------------------------
def insert_report(cursor, report_time, sdk_branch, server_version, test_env, total_cases):
    cursor.execute(
        "INSERT INTO benchmark_report (report_time, sdk_branch, server_version, test_env, total_cases) "
        "VALUES (%s, %s, %s, %s, %s)",
        (report_time, sdk_branch, server_version, test_env, total_cases),
    )
    return cursor.lastrowid


def insert_benchmark_case(cursor, report_id, report_time, case):
    tc = case.get("test_config", {})
    ps = case.get("performance_summary", {})
    ls = case.get("latency_stats", {})
    tb = case.get("time_breakdown", {})
    co = case.get("coordinated_omission", {})
    sr = case.get("system_resources", {})

    cursor.execute(
        """INSERT INTO benchmark_case (
            report_id, report_time, suite_name, case_type, mode, project, table_name,
            threads, rows_per_run,
            total_ops, total_rows, total_memory_size_bytes, total_wire_size_bytes,
            wall_clock_time_ms, qps, rps, memory_throughput_bps, wire_throughput_bps, error_count,
            avg_ms, min_ms, max_ms, std_dev_ms,
            p50_ms, p90_ms, p95_ms, p99_ms, p999_ms,
            avg_session_create_ms, avg_split_get_ms, avg_stream_create_ms,
            avg_read_ms, avg_write_ms, avg_data_gen_ms, avg_commit_ms, avg_close_ms,
            memory_used_kb, peak_memory_kb, avg_cpu_percent, peak_cpu_percent,
            co_total_omission_ms, co_avg_omission_ms, co_corrected_avg_latency_ms
        ) VALUES (
            %s,%s,%s,%s,%s,%s,%s,
            %s,%s,
            %s,%s,%s,%s,
            %s,%s,%s,%s,%s,%s,
            %s,%s,%s,%s,
            %s,%s,%s,%s,%s,
            %s,%s,%s,
            %s,%s,%s,%s,%s,
            %s,%s,%s,%s,
            %s,%s,%s
        )""",
        (
            report_id, report_time, case.get("suite_name", ""), "benchmark",
            tc.get("mode", ""), tc.get("project", ""), tc.get("table", ""),
            tc.get("threads", 1), tc.get("rows_per_run", 0),
            ps.get("total_ops", 0), ps.get("total_rows", 0),
            ps.get("total_memory_size_bytes", 0), ps.get("total_wire_size_bytes", 0),
            ps.get("wall_clock_time_ms", 0), ps.get("qps", 0), ps.get("rps", 0),
            ps.get("memory_throughput_bytes_per_sec", 0),
            ps.get("wire_throughput_bytes_per_sec", 0),
            ps.get("error_count", 0),
            ls.get("avg_ms", 0), ls.get("min_ms", 0), ls.get("max_ms", 0),
            ls.get("std_dev_ms", 0),
            ls.get("p50_ms", 0), ls.get("p90_ms", 0), ls.get("p95_ms", 0),
            ls.get("p99_ms", 0), ls.get("p999_ms", 0),
            tb.get("avg_session_create_ms"), tb.get("avg_split_get_ms"),
            tb.get("avg_stream_create_ms"),
            tb.get("avg_read_ms"), tb.get("avg_write_ms"),
            tb.get("avg_data_gen_ms"), tb.get("avg_commit_ms"), tb.get("avg_close_ms"),
            sr.get("memory_used_kb"), sr.get("peak_memory_kb"),
            sr.get("avg_cpu_usage_percent"), sr.get("peak_cpu_usage_percent"),
            co.get("total_omission_ms"), co.get("avg_omission_ms"),
            co.get("corrected_avg_latency_ms"),
        ),
    )


def insert_scaling_case(cursor, report_id, report_time, case):
    """Insert one row per thread level in a linear_scaling case.

    Latency values in scaling_results are in *seconds*; we convert to ms for
    uniform storage.

    Note: For linear_scaling cases, we use the base suite_name without the
    _t{threads} suffix, so all thread levels belong to the same suite for
    proper Grafana visualization.
    """
    tc = case.get("test_config", {})
    suite_name = case.get("suite_name", "")

    for r in case.get("scaling_results", []):
        cursor.execute(
            """INSERT INTO benchmark_case (
                report_id, report_time, suite_name, case_type, mode, project, table_name,
                threads, rows_per_run,
                total_ops, total_rows, total_memory_size_bytes, total_wire_size_bytes,
                wall_clock_time_ms, qps, rps, memory_throughput_bps, wire_throughput_bps, error_count,
                avg_ms, min_ms, max_ms, std_dev_ms,
                p50_ms, p90_ms, p95_ms, p99_ms, p999_ms
            ) VALUES (
                %s,%s,%s,%s,%s,%s,%s,
                %s,%s,
                %s,%s,%s,%s,
                %s,%s,%s,%s,%s,%s,
                %s,%s,%s,%s,
                %s,%s,%s,%s,%s
            )""",
            (
                report_id, report_time,
                suite_name,  # Use base suite_name without _t{threads} suffix
                "linear_scaling",
                tc.get("mode", ""), tc.get("project", ""), tc.get("table", ""),
                r.get("threads", 1), 0,
                r.get("total_ops", 0), r.get("total_rows", 0),
                r.get("total_size_bytes", 0), r.get("total_wire_size_bytes", 0),
                r.get("wall_clock_time_ms", 0), r.get("qps", 0), r.get("rps", 0),
                r.get("throughput_bytes_per_sec", 0),
                r.get("throughput_bytes_per_sec", 0),  # wire = memory for scaling
                r.get("error_count", 0),
                r.get("avg_latency_s", 0) * 1000,
                r.get("min_latency_s", 0) * 1000,
                r.get("max_latency_s", 0) * 1000,
                0,  # std_dev not available in scaling
                r.get("p50_latency_s", 0) * 1000,
                r.get("p90_latency_s", 0) * 1000,
                r.get("p95_latency_s", 0) * 1000,
                r.get("p99_latency_s", 0) * 1000,
                r.get("p999_latency_s", 0) * 1000,
            ),
        )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Store benchmark report.json into MySQL")
    parser.add_argument("report", help="Path to report.json")
    parser.add_argument("--server-version", default="", help="Server version, e.g. V55.tunnel.RC4")
    parser.add_argument("--test-env", default="", help="Test environment, e.g. test-trunk")
    parser.add_argument("--db-host", required=True, help="MySQL host")
    parser.add_argument("--db-user", required=True, help="MySQL user")
    parser.add_argument("--db-pass", required=True, help="MySQL password")
    parser.add_argument("--db-name", default=_DB_DEFAULTS["database"], help="MySQL database name (default: %(default)s)")
    parser.add_argument("--db-port", default=_DB_DEFAULTS["port"], type=int, help="MySQL port (default: %(default)s)")
    args = parser.parse_args()

    # Build DB config from CLI args
    DB_CFG = {
        "host":     args.db_host,
        "user":     args.db_user,
        "password": args.db_pass,
        "database": args.db_name,
        "port":     args.db_port,
        "charset":  "utf8mb4",
    }

    report_path = args.report
    if not os.path.isfile(report_path):
        print(f"Error: file not found: {report_path}", file=sys.stderr)
        sys.exit(1)

    with open(report_path, "r") as f:
        data = json.load(f)

    report_time = data.get("report_time", "")
    total_cases = data.get("total_cases", 0)
    cases = data.get("cases", [])

    sdk_branch = get_sdk_branch()
    server_version = args.server_version
    test_env = args.test_env or "unknown"
    print(f"SDK branch: {sdk_branch}")
    print(f"Server version: {server_version}")
    print(f"Test environment: {test_env}")

    conn = pymysql.connect(**DB_CFG)
    try:
        with conn.cursor() as cur:
            cur.execute(CREATE_REPORT_TABLE)
            cur.execute(CREATE_CASE_TABLE)
        conn.commit()

        with conn.cursor() as cur:
            report_id = insert_report(cur, report_time, sdk_branch, server_version, test_env, total_cases)
            print(f"Inserted benchmark_report id={report_id}")

            for c in cases:
                ctype = c.get("type", "benchmark")
                if ctype == "linear_scaling":
                    insert_scaling_case(cur, report_id, report_time, c)
                    n = len(c.get("scaling_results", []))
                    print(f"  Inserted linear_scaling suite={c.get('suite_name','')} ({n} thread levels)")
                else:
                    insert_benchmark_case(cur, report_id, report_time, c)
                    print(f"  Inserted benchmark suite={c.get('suite_name','')}")

        conn.commit()
        print(f"\nDone. report_id={report_id}")

    finally:
        conn.close()


if __name__ == "__main__":
    main()
