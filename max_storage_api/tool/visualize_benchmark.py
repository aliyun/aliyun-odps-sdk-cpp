#!/usr/bin/env python3
"""
MaxStorage Benchmark Visualization Script

Reads benchmark JSON reports and generates interactive HTML reports
and/or PNG chart images.

Usage:
    # Generate HTML report from single benchmark JSON
    python3 visualize_benchmark.py --json benchmark_report.json

    # Generate HTML report from scaling test JSON
    python3 visualize_benchmark.py --json linear_scaling_report.json

    # Generate PNG charts (requires matplotlib)
    python3 visualize_benchmark.py --json benchmark_report.json --png

    # Generate both HTML and PNG
    python3 visualize_benchmark.py --json benchmark_report.json --png

    # Auto-detect JSON files in current directory
    python3 visualize_benchmark.py
"""

import json
import os
import sys
import argparse


def fmt_bytes(b):
    """Format bytes into human-readable string."""
    if b >= 1024**3:
        return f"{b / 1024**3:.2f} GB"
    if b >= 1024**2:
        return f"{b / 1024**2:.2f} MB"
    if b >= 1024:
        return f"{b / 1024:.2f} KB"
    return f"{b:.0f} B"


def fmt_bytes_per_sec(bps):
    return fmt_bytes(bps) + "/s"


# ============================================================================
# HTML Report Generation (no external dependencies)
# ============================================================================

COMMON_CSS = """
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
       background: #f0f2f5; color: #333; padding: 24px; }
h1 { text-align: center; margin-bottom: 8px; color: #1a1a2e; }
.subtitle { text-align: center; color: #666; margin-bottom: 24px; font-size: 14px; }
.cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 16px; margin-bottom: 24px; }
.card { background: #fff; border-radius: 12px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.08); text-align: center; }
.card .label { font-size: 12px; color: #888; text-transform: uppercase; letter-spacing: 1px; }
.card .value { font-size: 28px; font-weight: 700; color: #1a1a2e; margin: 8px 0 4px; }
.card .unit { font-size: 12px; color: #aaa; }
.card.error .value { color: #e74c3c; }
.charts { display: grid; grid-template-columns: repeat(auto-fit, minmax(450px, 1fr)); gap: 24px; margin-bottom: 24px; }
.chart-box { background: #fff; border-radius: 12px; padding: 24px; box-shadow: 0 2px 8px rgba(0,0,0,0.08); }
.chart-box h3 { margin-bottom: 16px; color: #1a1a2e; font-size: 16px; }
table { width: 100%; border-collapse: collapse; font-size: 14px; }
th, td { padding: 10px 14px; text-align: left; border-bottom: 1px solid #eee; }
th { background: #f8f9fa; color: #666; font-weight: 600; }
tr:hover td { background: #f8f9fa; }
.sys-cards { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; }
.sys-card { background: #f8f9fa; border-radius: 8px; padding: 16px; text-align: center; }
.sys-card .sv { font-size: 22px; font-weight: 700; color: #2c3e50; }
.sys-card .sl { font-size: 11px; color: #888; margin-top: 4px; }
"""


def _card_html(label, value, unit, is_error=False):
    cls = ' error' if is_error else ''
    return (f'<div class="card{cls}">'
            f'<div class="label">{label}</div>'
            f'<div class="value">{value}</div>'
            f'<div class="unit">{unit}</div></div>\n')


def generate_single_html(data, output_path):
    """Generate self-contained HTML report from a single benchmark JSON."""
    config = data.get("test_config", {})
    perf = data.get("performance_summary", {})
    latency = data.get("latency_stats", {})
    breakdown = data.get("time_breakdown", {})
    histogram = data.get("latency_histogram", [])
    sysres = data.get("system_resources", {})
    coord = data.get("coordinated_omission", {})

    table_name = config.get("table", "?")
    mode = config.get("mode", "?")
    threads = config.get("threads", "?")
    project = config.get("project", "?")
    rows_per_run = config.get("rows_per_run", "?")

    lines = []
    w = lines.append

    w('<!DOCTYPE html>\n<html lang="en">\n<head>\n<meta charset="UTF-8">')
    w('<meta name="viewport" content="width=device-width, initial-scale=1.0">')
    w(f'<title>MaxStorage Benchmark Report - {table_name} ({mode})</title>')
    w('<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>')
    w(f'<style>\n{COMMON_CSS}\n</style>')
    w('</head>\n<body>')

    # Title
    w('<h1>MaxStorage Benchmark Report</h1>')
    w(f'<div class="subtitle">{project}.{table_name} | Mode: {mode} | '
      f'Threads: {threads} | Rows/Run: {rows_per_run}</div>')

    # Performance Summary Cards
    w('<div class="cards">')
    w(_card_html("QPS", f"{perf.get('qps', 0):.2f}", "ops/sec"))
    w(_card_html("RPS", f"{perf.get('rps', 0):.2f}", "rows/sec"))
    w(_card_html("Throughput (Mem)", fmt_bytes_per_sec(perf.get('memory_throughput_bytes_per_sec', 0)), ""))
    w(_card_html("Throughput (Wire)", fmt_bytes_per_sec(perf.get('wire_throughput_bytes_per_sec', 0)), ""))
    w(_card_html("Total Ops", str(perf.get('total_ops', 0)), "operations"))
    w(_card_html("Total Rows", str(perf.get('total_rows', 0)), "rows"))
    w(_card_html("Memory Size", fmt_bytes(perf.get('total_memory_size_bytes', 0)), ""))
    w(_card_html("Wire Size", fmt_bytes(perf.get('total_wire_size_bytes', 0)), ""))
    w(_card_html("Avg Latency", f"{latency.get('avg_ms', 0):.2f} ms", ""))
    w(_card_html("P99 Latency", f"{latency.get('p99_ms', 0):.2f} ms", ""))
    w(_card_html("Wall Clock", f"{perf.get('wall_clock_time_ms', 0) / 1000:.2f} s", ""))
    err_count = perf.get('error_count', 0)
    w(_card_html("Errors", str(err_count), "", err_count > 0))
    w('</div>')

    # Charts Section
    w('<div class="charts">')
    w('<div class="chart-box"><h3>Latency Percentiles</h3><canvas id="latencyPercentile"></canvas></div>')
    if histogram:
        w('<div class="chart-box"><h3>Latency Distribution</h3><canvas id="latencyHist"></canvas></div>')
    w('<div class="chart-box"><h3>Time Breakdown (per operation)</h3><canvas id="timeBreakdown"></canvas></div>')

    # System Resources
    if sysres.get('enabled'):
        w('<div class="chart-box"><h3>System Resources</h3>')
        w('<div class="sys-cards">')
        # Support both old field (cpu_usage_percent) and new fields (avg/peak)
        avg_cpu = sysres.get("avg_cpu_usage_percent", sysres.get("cpu_usage_percent", 0))
        peak_cpu = sysres.get("peak_cpu_usage_percent", avg_cpu)
        w(f'<div class="sys-card"><div class="sv">{avg_cpu:.1f}%</div><div class="sl">Avg CPU</div></div>')
        w(f'<div class="sys-card"><div class="sv">{peak_cpu:.1f}%</div><div class="sl">Peak CPU</div></div>')
        w(f'<div class="sys-card"><div class="sv">{fmt_bytes(sysres.get("memory_used_kb", 0) * 1024)}</div><div class="sl">Memory (RSS)</div></div>')
        w(f'<div class="sys-card"><div class="sv">{fmt_bytes(sysres.get("peak_memory_kb", 0) * 1024)}</div><div class="sl">Peak Memory</div></div>')
        w('</div></div>')

    w('</div>')

    # Config Table
    w('<div class="chart-box" style="margin-bottom:24px"><h3>Test Configuration</h3>')
    w('<table><tr><th>Parameter</th><th>Value</th></tr>')
    w(f'<tr><td>Project</td><td>{project}</td></tr>')
    w(f'<tr><td>Table</td><td>{table_name}</td></tr>')
    w(f'<tr><td>Schema</td><td>{config.get("schema", "default")}</td></tr>')
    if config.get("partition"):
        w(f'<tr><td>Partition</td><td>{config["partition"]}</td></tr>')
    w(f'<tr><td>Mode</td><td>{mode}</td></tr>')
    w(f'<tr><td>Threads</td><td>{threads}</td></tr>')
    w(f'<tr><td>Rows per Run</td><td>{rows_per_run}</td></tr>')
    rate_limit = config.get("rate_limit", 0)
    if rate_limit and rate_limit > 0:
        w(f'<tr><td>Rate Limit</td><td>{rate_limit:.1f} ops/s/thread</td></tr>')
    warmup = config.get("warmup_seconds", 0)
    if warmup and warmup > 0:
        w(f'<tr><td>Warmup</td><td>{warmup} seconds</td></tr>')
    w('</table></div>')

    # Coordinated Omission Table
    if coord.get('enabled') and coord.get('total_omission_ms', 0) > 0:
        total_ops = perf.get('total_ops', 1)
        w('<div class="chart-box" style="margin-bottom:24px"><h3>Coordinated Omission</h3>')
        w('<table><tr><th>Metric</th><th>Value</th></tr>')
        w(f'<tr><td>Total Omission</td><td>{coord["total_omission_ms"]} ms</td></tr>')
        w(f'<tr><td>Avg Omission</td><td>{coord.get("avg_omission_ms", 0):.2f} ms/op</td></tr>')
        w(f'<tr><td>Corrected Avg Latency</td><td>{coord.get("corrected_avg_latency_ms", 0):.2f} ms</td></tr>')
        w('</table></div>')

    # Chart.js Scripts
    w('<script>')

    # Latency Percentile Chart
    pct_labels = json.dumps(['P50', 'P90', 'P95', 'P99', 'P99.9', 'Avg', 'Min', 'Max'])
    pct_data = json.dumps([
        latency.get('p50_ms', 0), latency.get('p90_ms', 0),
        latency.get('p95_ms', 0), latency.get('p99_ms', 0),
        latency.get('p999_ms', 0), latency.get('avg_ms', 0),
        latency.get('min_ms', 0), latency.get('max_ms', 0),
    ])
    pct_colors = json.dumps(['#3498db','#2ecc71','#f1c40f','#e67e22','#e74c3c','#9b59b6','#1abc9c','#34495e'])
    w(f"""new Chart(document.getElementById('latencyPercentile'), {{
  type: 'bar',
  data: {{ labels: {pct_labels}, datasets: [{{ label: 'Latency (ms)', data: {pct_data}, backgroundColor: {pct_colors} }}] }},
  options: {{ responsive: true, plugins: {{ legend: {{ display: false }} }},
    scales: {{ y: {{ beginAtZero: true, title: {{ display: true, text: 'ms' }} }} }} }}
}});""")

    # Latency Histogram Chart
    if histogram:
        h_labels = json.dumps([h['range_ms'] for h in histogram])
        h_counts = json.dumps([h['count'] for h in histogram])
        w(f"""new Chart(document.getElementById('latencyHist'), {{
  type: 'bar',
  data: {{ labels: {h_labels}, datasets: [{{ label: 'Count', data: {h_counts}, backgroundColor: '#3498db' }}] }},
  options: {{ responsive: true, plugins: {{ legend: {{ display: false }} }},
    scales: {{ x: {{ title: {{ display: true, text: 'Latency (ms)' }} }},
             y: {{ beginAtZero: true, title: {{ display: true, text: 'Count' }} }} }} }}
}});""")

    # Time Breakdown Doughnut Chart
    bd_labels = []
    bd_values = []
    label_map = [
        ('avg_session_create_ms', 'Session Create'),
        ('avg_stream_create_ms', 'Stream Create'),
        ('avg_data_gen_ms', 'Data Gen'),
        ('avg_write_ms', 'Write'),
        ('avg_commit_ms', 'Commit'),
        ('avg_split_get_ms', 'Split Get'),
        ('avg_read_ms', 'Read'),
        ('avg_close_ms', 'Close'),
    ]
    for key, label in label_map:
        if key in breakdown:
            bd_labels.append(label)
            bd_values.append(round(breakdown[key], 2))

    if bd_values and sum(bd_values) > 0:
        bd_colors = ['#3498db', '#2ecc71', '#f1c40f', '#e67e22', '#e74c3c'][:len(bd_values)]
        w(f"""new Chart(document.getElementById('timeBreakdown'), {{
  type: 'doughnut',
  data: {{ labels: {json.dumps(bd_labels)}, datasets: [{{ data: {json.dumps(bd_values)}, backgroundColor: {json.dumps(bd_colors)} }}] }},
  options: {{ responsive: true, plugins: {{ legend: {{ position: 'right' }},
    tooltip: {{ callbacks: {{ label: function(ctx) {{ return ctx.label + ': ' + ctx.parsed.toFixed(2) + ' ms'; }} }} }} }} }}
}});""")
    else:
        w("""new Chart(document.getElementById('timeBreakdown'), {
  type: 'doughnut',
  data: { labels: ['No Data'], datasets: [{ data: [1], backgroundColor: ['#ccc'] }] },
  options: { responsive: true }
});""")

    w('</script>\n</body>\n</html>')

    html_content = '\n'.join(lines)
    with open(output_path, 'w') as f:
        f.write(html_content)
    print(f"  HTML report saved to: {output_path}")


def generate_scaling_html(data, output_path):
    """Generate self-contained HTML report from a scaling test JSON."""
    config = data.get("test_config", {})
    results = data.get("results", [])

    if not results:
        print("No scaling results found in JSON")
        return

    table_name = config.get("table", "?")
    project = config.get("project", "?")
    mode = config.get("mode", "?")

    threads = [r['threads'] for r in results]
    qps_vals = [r['qps'] for r in results]
    tp_vals = [r['throughput_bytes_per_sec'] / 1048576 for r in results]  # MB/s
    avg_lat = [r['avg_latency_s'] * 1000 for r in results]
    p50 = [r['p50_latency_s'] * 1000 for r in results]
    p90 = [r['p90_latency_s'] * 1000 for r in results]
    p99 = [r['p99_latency_s'] * 1000 for r in results]

    base_qps = qps_vals[0]
    base_tp = tp_vals[0]
    base_t = threads[0]
    ideal_qps = [base_qps * t / base_t for t in threads]
    ideal_tp = [base_tp * t / base_t for t in threads]

    scaling_css = """
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
       background: #f0f2f5; color: #333; padding: 24px; }
h1 { text-align: center; margin-bottom: 8px; color: #1a1a2e; }
.subtitle { text-align: center; color: #666; margin-bottom: 24px; font-size: 14px; }
.charts { display: grid; grid-template-columns: repeat(auto-fit, minmax(500px, 1fr)); gap: 24px; margin-bottom: 24px; }
.chart-box { background: #fff; border-radius: 12px; padding: 24px; box-shadow: 0 2px 8px rgba(0,0,0,0.08); }
.chart-box h3 { margin-bottom: 16px; color: #1a1a2e; font-size: 16px; }
table { width: 100%; border-collapse: collapse; font-size: 13px; }
th, td { padding: 8px 12px; text-align: right; border-bottom: 1px solid #eee; }
th { background: #f8f9fa; color: #666; font-weight: 600; text-align: right; }
th:first-child, td:first-child { text-align: left; }
tr:hover td { background: #f8f9fa; }
"""

    lines = []
    w = lines.append

    w('<!DOCTYPE html>\n<html lang="en">\n<head>\n<meta charset="UTF-8">')
    w('<meta name="viewport" content="width=device-width, initial-scale=1.0">')
    w(f'<title>Linear Scaling Test Report - {table_name}</title>')
    w('<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>')
    w(f'<style>\n{scaling_css}\n</style>')
    w('</head>\n<body>')

    w('<h1>Linear Scaling Test Report</h1>')
    w(f'<div class="subtitle">{project}.{table_name} | Mode: {mode} | '
      f'Threads: {threads[0]} - {threads[-1]}</div>')

    # Charts
    w('<div class="charts">')
    w('<div class="chart-box"><h3>QPS vs Threads</h3><canvas id="qpsChart"></canvas></div>')
    w('<div class="chart-box"><h3>Throughput vs Threads</h3><canvas id="tpChart"></canvas></div>')
    w('<div class="chart-box"><h3>Latency vs Threads</h3><canvas id="latChart"></canvas></div>')
    w('<div class="chart-box"><h3>Scaling Efficiency</h3><canvas id="effChart"></canvas></div>')
    w('</div>')

    # Data Table
    w('<div class="chart-box"><h3>Detailed Results</h3>')
    w('<table>')
    w('<tr><th>Threads</th><th>QPS</th><th>RPS</th><th>Throughput</th>'
      '<th>Avg Lat</th><th>P50</th><th>P90</th><th>P95</th><th>P99</th><th>P99.9</th><th>Errors</th></tr>')
    for r in results:
        w(f'<tr><td>{r["threads"]}</td>'
          f'<td>{r["qps"]:.2f}</td>'
          f'<td>{r["rps"]:.2f}</td>'
          f'<td>{r["throughput_bytes_per_sec"] / 1048576:.2f} MB/s</td>'
          f'<td>{r["avg_latency_s"] * 1000:.2f} ms</td>'
          f'<td>{r["p50_latency_s"] * 1000:.2f}</td>'
          f'<td>{r["p90_latency_s"] * 1000:.2f}</td>'
          f'<td>{r["p95_latency_s"] * 1000:.2f}</td>'
          f'<td>{r["p99_latency_s"] * 1000:.2f}</td>'
          f'<td>{r["p999_latency_s"] * 1000:.2f}</td>'
          f'<td>{r["error_count"]}</td></tr>')
    w('</table></div>')

    # Chart.js Scripts
    js_threads = json.dumps(threads)
    js_qps = json.dumps([round(q, 2) for q in qps_vals])
    js_tp = json.dumps([round(t, 2) for t in tp_vals])
    js_ideal_qps = json.dumps([round(q, 2) for q in ideal_qps])
    js_ideal_tp = json.dumps([round(t, 2) for t in ideal_tp])
    js_avg_lat = json.dumps([round(v, 2) for v in avg_lat])
    js_p50 = json.dumps([round(v, 2) for v in p50])
    js_p90 = json.dumps([round(v, 2) for v in p90])
    js_p99 = json.dumps([round(v, 2) for v in p99])

    w('<script>')
    w(f'const threads = {js_threads};')

    # QPS Chart
    w(f"""new Chart(document.getElementById('qpsChart'), {{
  type: 'line',
  data: {{ labels: threads, datasets: [
    {{ label: 'Actual QPS', data: {js_qps}, borderColor: '#3498db', backgroundColor: 'rgba(52,152,219,0.1)', fill: true, tension: 0.3 }},
    {{ label: 'Ideal Linear', data: {js_ideal_qps}, borderColor: '#ccc', borderDash: [5,5], fill: false, pointRadius: 0 }}
  ] }},
  options: {{ responsive: true, scales: {{ x: {{ title: {{ display: true, text: 'Threads' }} }}, y: {{ beginAtZero: true, title: {{ display: true, text: 'QPS' }} }} }} }}
}});""")

    # Throughput Chart
    w(f"""new Chart(document.getElementById('tpChart'), {{
  type: 'line',
  data: {{ labels: threads, datasets: [
    {{ label: 'Actual (MB/s)', data: {js_tp}, borderColor: '#2ecc71', backgroundColor: 'rgba(46,204,113,0.1)', fill: true, tension: 0.3 }},
    {{ label: 'Ideal Linear', data: {js_ideal_tp}, borderColor: '#ccc', borderDash: [5,5], fill: false, pointRadius: 0 }}
  ] }},
  options: {{ responsive: true, scales: {{ x: {{ title: {{ display: true, text: 'Threads' }} }}, y: {{ beginAtZero: true, title: {{ display: true, text: 'MB/s' }} }} }} }}
}});""")

    # Latency Chart
    w(f"""new Chart(document.getElementById('latChart'), {{
  type: 'line',
  data: {{ labels: threads, datasets: [
    {{ label: 'Avg', data: {js_avg_lat}, borderColor: '#9b59b6', tension: 0.3 }},
    {{ label: 'P50', data: {js_p50}, borderColor: '#3498db', tension: 0.3 }},
    {{ label: 'P90', data: {js_p90}, borderColor: '#f1c40f', tension: 0.3 }},
    {{ label: 'P99', data: {js_p99}, borderColor: '#e74c3c', tension: 0.3 }}
  ] }},
  options: {{ responsive: true, scales: {{ x: {{ title: {{ display: true, text: 'Threads' }} }}, y: {{ title: {{ display: true, text: 'Latency (ms)' }} }} }} }}
}});""")

    # Scaling Efficiency Chart
    w(f"""var actualQPS = {js_qps};
var idealQPS = {js_ideal_qps};
var effData = actualQPS.map(function(v, i) {{ return idealQPS[i] > 0 ? (v / idealQPS[i] * 100).toFixed(1) : 0; }});
new Chart(document.getElementById('effChart'), {{
  type: 'bar',
  data: {{ labels: threads, datasets: [{{
    label: 'Scaling Efficiency (%)',
    data: effData,
    backgroundColor: effData.map(function(v) {{ return v >= 90 ? '#2ecc71' : v >= 70 ? '#f1c40f' : '#e74c3c'; }})
  }}] }},
  options: {{ responsive: true, scales: {{ x: {{ title: {{ display: true, text: 'Threads' }} }},
    y: {{ beginAtZero: true, max: 120, title: {{ display: true, text: '%' }} }} }},
    plugins: {{ legend: {{ display: false }} }} }}
}});""")

    w('</script>\n</body>\n</html>')

    html_content = '\n'.join(lines)
    with open(output_path, 'w') as f:
        f.write(html_content)
    print(f"  Scaling HTML report saved to: {output_path}")


# ============================================================================
# PNG Chart Generation (requires matplotlib)
# ============================================================================

def _require_matplotlib():
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import matplotlib.ticker as ticker
        return plt, ticker
    except ImportError:
        print("Error: matplotlib is required for PNG output. Install with: pip install matplotlib")
        sys.exit(1)


def plot_single_report(data, output_dir):
    """Generate PNG charts from a single benchmark JSON report."""
    plt, ticker = _require_matplotlib()

    config = data.get("test_config", {})
    perf = data.get("performance_summary", {})
    latency = data.get("latency_stats", {})
    breakdown = data.get("time_breakdown", {})
    histogram = data.get("latency_histogram", [])
    sysres = data.get("system_resources", {})

    title_suffix = f"{config.get('table', '?')} ({config.get('mode', '?')}, {config.get('threads', '?')} threads)"

    # --- 1. Latency Percentile Bar Chart ---
    fig, ax = plt.subplots(figsize=(10, 5))
    labels = ['P50', 'P90', 'P95', 'P99', 'P99.9', 'Avg', 'Min', 'Max']
    values = [
        latency.get('p50_ms', 0), latency.get('p90_ms', 0),
        latency.get('p95_ms', 0), latency.get('p99_ms', 0),
        latency.get('p999_ms', 0), latency.get('avg_ms', 0),
        latency.get('min_ms', 0), latency.get('max_ms', 0),
    ]
    colors = ['#3498db', '#2ecc71', '#f1c40f', '#e67e22', '#e74c3c', '#9b59b6', '#1abc9c', '#34495e']
    bars = ax.bar(labels, values, color=colors, edgecolor='white', linewidth=0.5)
    for bar, val in zip(bars, values):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                f'{val:.2f}', ha='center', va='bottom', fontsize=9)
    ax.set_ylabel('Latency (ms)')
    ax.set_title(f'Latency Percentiles - {title_suffix}')
    ax.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'latency_percentiles.png'), dpi=150)
    plt.close()
    print(f"  Saved: latency_percentiles.png")

    # --- 2. Latency Histogram ---
    if histogram:
        fig, ax = plt.subplots(figsize=(10, 5))
        h_labels = [h['range_ms'] for h in histogram]
        h_counts = [h['count'] for h in histogram]
        bars = ax.bar(h_labels, h_counts, color='#3498db', edgecolor='white', linewidth=0.5)
        for bar, val in zip(bars, h_counts):
            if val > 0:
                ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                        str(val), ha='center', va='bottom', fontsize=9)
        ax.set_xlabel('Latency Range (ms)')
        ax.set_ylabel('Count')
        ax.set_title(f'Latency Distribution - {title_suffix}')
        ax.grid(axis='y', alpha=0.3)
        plt.xticks(rotation=30, ha='right')
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, 'latency_histogram.png'), dpi=150)
        plt.close()
        print(f"  Saved: latency_histogram.png")

    # --- 3. Time Breakdown Pie Chart ---
    if breakdown:
        fig, ax = plt.subplots(figsize=(8, 6))
        bd_labels = []
        bd_values = []
        label_map = {
            'avg_session_create_ms': 'Session Create',
            'avg_stream_create_ms': 'Stream Create',
            'avg_data_gen_ms': 'Data Gen',
            'avg_write_ms': 'Write',
            'avg_commit_ms': 'Commit',
            'avg_split_get_ms': 'Split Get',
            'avg_read_ms': 'Read',
            'avg_close_ms': 'Close',
        }
        for key, label in label_map.items():
            if key in breakdown:
                bd_labels.append(label)
                bd_values.append(breakdown[key])

        if bd_values and sum(bd_values) > 0:
            colors_pie = ['#3498db', '#2ecc71', '#f1c40f', '#e67e22', '#e74c3c']
            wedges, texts, autotexts = ax.pie(
                bd_values, labels=bd_labels, autopct='%1.1f%%',
                colors=colors_pie[:len(bd_values)], startangle=90,
                pctdistance=0.75
            )
            for t in autotexts:
                t.set_fontsize(9)
            ax.set_title(f'Time Breakdown - {title_suffix}')
            plt.tight_layout()
            plt.savefig(os.path.join(output_dir, 'time_breakdown.png'), dpi=150)
            plt.close()
            print(f"  Saved: time_breakdown.png")

    # --- 4. Summary Card (text figure) ---
    fig, ax = plt.subplots(figsize=(10, 4))
    ax.axis('off')
    summary_text = (
        f"Performance Summary - {title_suffix}\n"
        f"{'='*60}\n"
        f"QPS: {perf.get('qps', 0):.2f}    "
        f"RPS: {perf.get('rps', 0):.2f}    "
        f"Errors: {perf.get('error_count', 0)}\n"
        f"Throughput (Mem): {fmt_bytes(perf.get('memory_throughput_bytes_per_sec', 0))}/s    "
        f"Throughput (Wire): {fmt_bytes(perf.get('wire_throughput_bytes_per_sec', 0))}/s\n"
        f"Total Size (Mem): {fmt_bytes(perf.get('total_memory_size_bytes', 0))}    "
        f"Total Size (Wire): {fmt_bytes(perf.get('total_wire_size_bytes', 0))}\n"
        f"Avg Latency: {latency.get('avg_ms', 0):.2f} ms    "
        f"P99: {latency.get('p99_ms', 0):.2f} ms    "
        f"Wall Clock: {perf.get('wall_clock_time_ms', 0)/1000:.2f} s\n"
    )
    if sysres.get('enabled'):
        # Support both old field (cpu_usage_percent) and new fields (avg/peak)
        avg_cpu = sysres.get('avg_cpu_usage_percent', sysres.get('cpu_usage_percent', 0))
        peak_cpu = sysres.get('peak_cpu_usage_percent', avg_cpu)
        summary_text += (
            f"CPU (avg/peak): {avg_cpu:.1f}% / {peak_cpu:.1f}%    "
            f"Memory: {fmt_bytes(sysres.get('memory_used_kb', 0) * 1024)}    "
            f"Peak: {fmt_bytes(sysres.get('peak_memory_kb', 0) * 1024)}\n"
        )
    ax.text(0.05, 0.95, summary_text, transform=ax.transAxes,
            fontsize=11, verticalalignment='top', fontfamily='monospace',
            bbox=dict(boxstyle='round', facecolor='#f0f2f5', alpha=0.8))
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'summary.png'), dpi=150)
    plt.close()
    print(f"  Saved: summary.png")


def plot_scaling_report(data, output_dir):
    """Generate PNG charts from a scaling test JSON report."""
    plt, ticker = _require_matplotlib()

    results = data.get("results", [])
    if not results:
        print("No scaling results found")
        return

    threads = [int(r['threads']) for r in results]
    qps = [r['qps'] for r in results]
    throughput = [r['throughput_bytes_per_sec'] / 1048576 for r in results]  # MB/s
    avg_lat = [r['avg_latency_s'] * 1000 for r in results]
    p50 = [r['p50_latency_s'] * 1000 for r in results]
    p90 = [r['p90_latency_s'] * 1000 for r in results]
    p99 = [r['p99_latency_s'] * 1000 for r in results]

    base_qps = qps[0]
    base_tp = throughput[0]
    base_t = threads[0]
    ideal_qps = [base_qps * t / base_t for t in threads]
    ideal_tp = [base_tp * t / base_t for t in threads]

    # --- 1. QPS vs Threads ---
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(threads, qps, 'o-', color='#3498db', linewidth=2, markersize=6, label='Actual QPS')
    ax.plot(threads, ideal_qps, '--', color='#ccc', linewidth=1.5, label='Ideal Linear')
    ax.fill_between(threads, qps, alpha=0.1, color='#3498db')
    ax.set_xlabel('Threads')
    ax.set_ylabel('QPS')
    ax.set_title('QPS vs Threads')
    ax.legend()
    ax.grid(alpha=0.3)
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'scaling_qps.png'), dpi=150)
    plt.close()
    print(f"  Saved: scaling_qps.png")

    # --- 2. Throughput vs Threads ---
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(threads, throughput, 'o-', color='#2ecc71', linewidth=2, markersize=6, label='Actual (MB/s)')
    ax.plot(threads, ideal_tp, '--', color='#ccc', linewidth=1.5, label='Ideal Linear')
    ax.fill_between(threads, throughput, alpha=0.1, color='#2ecc71')
    ax.set_xlabel('Threads')
    ax.set_ylabel('Throughput (MB/s)')
    ax.set_title('Throughput vs Threads')
    ax.legend()
    ax.grid(alpha=0.3)
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'scaling_throughput.png'), dpi=150)
    plt.close()
    print(f"  Saved: scaling_throughput.png")

    # --- 3. Latency vs Threads ---
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(threads, avg_lat, 'o-', color='#9b59b6', linewidth=2, markersize=5, label='Avg')
    ax.plot(threads, p50, 's-', color='#3498db', linewidth=1.5, markersize=5, label='P50')
    ax.plot(threads, p90, '^-', color='#f1c40f', linewidth=1.5, markersize=5, label='P90')
    ax.plot(threads, p99, 'D-', color='#e74c3c', linewidth=1.5, markersize=5, label='P99')
    ax.set_xlabel('Threads')
    ax.set_ylabel('Latency (ms)')
    ax.set_title('Latency vs Threads')
    ax.legend()
    ax.grid(alpha=0.3)
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'scaling_latency.png'), dpi=150)
    plt.close()
    print(f"  Saved: scaling_latency.png")

    # --- 4. Scaling Efficiency ---
    fig, ax = plt.subplots(figsize=(10, 5))
    efficiency = [(q / iq * 100) if iq > 0 else 0 for q, iq in zip(qps, ideal_qps)]
    colors = ['#2ecc71' if e >= 90 else '#f1c40f' if e >= 70 else '#e74c3c' for e in efficiency]
    bars = ax.bar(threads, efficiency, color=colors, edgecolor='white', linewidth=0.5)
    ax.axhline(y=100, color='#ccc', linestyle='--', linewidth=1)
    ax.axhline(y=90, color='#f1c40f', linestyle=':', linewidth=1, alpha=0.5)
    for bar, val in zip(bars, efficiency):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                f'{val:.1f}%', ha='center', va='bottom', fontsize=9)
    ax.set_xlabel('Threads')
    ax.set_ylabel('Efficiency (%)')
    ax.set_title('Scaling Efficiency (Actual QPS / Ideal QPS)')
    ax.set_ylim(0, 120)
    ax.grid(axis='y', alpha=0.3)
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'scaling_efficiency.png'), dpi=150)
    plt.close()
    print(f"  Saved: scaling_efficiency.png")


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='MaxStorage Benchmark Visualization - generates HTML reports from JSON',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  python3 visualize_benchmark.py --json benchmark_report.json
  python3 visualize_benchmark.py --json linear_scaling_report.json
  python3 visualize_benchmark.py --json benchmark_report.json --png
  python3 visualize_benchmark.py  (auto-detect JSON files in current dir)
""")
    parser.add_argument('--json', type=str, action='append',
                        help='Path to JSON report file (can specify multiple times)')
    parser.add_argument('--png', action='store_true',
                        help='Also generate PNG charts (requires matplotlib)')
    parser.add_argument('--output-dir', type=str, default='.',
                        help='Output directory (default: current dir)')
    args = parser.parse_args()

    json_files = args.json or []

    # Auto-detect if no files given
    if not json_files:
        for name in ['benchmark_report.json', 'linear_scaling_report.json']:
            if os.path.exists(name):
                json_files.append(name)
        if not json_files:
            print("No JSON files found. Provide --json, or run from the benchmark output directory.")
            sys.exit(1)

    os.makedirs(args.output_dir, exist_ok=True)

    for json_path in json_files:
        print(f"Processing: {json_path}")
        with open(json_path, 'r') as f:
            data = json.load(f)

        report_type = data.get("report_type", "")
        is_scaling = report_type == "linear_scaling" or "results" in data

        # Determine HTML output filename
        base_name = os.path.splitext(os.path.basename(json_path))[0]
        html_path = os.path.join(args.output_dir, f"{base_name}.html")

        if is_scaling:
            generate_scaling_html(data, html_path)
            if args.png:
                print(f"  Generating PNG charts...")
                plot_scaling_report(data, args.output_dir)
        else:
            generate_single_html(data, html_path)
            if args.png:
                print(f"  Generating PNG charts...")
                plot_single_report(data, args.output_dir)

    print("Done.")


if __name__ == '__main__':
    main()
