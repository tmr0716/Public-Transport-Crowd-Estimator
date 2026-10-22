import os
import subprocess
from flask import Flask, render_template, request, redirect, url_for

app = Flask(__name__)

ENGINE_PATH = os.path.join(
    "build",
    "crowd_engine.exe" if os.name == "nt" else "crowd_engine"
)
CSV_PATH = os.path.join("data", "stops.csv")

def run_cpp_command(args):
    cmd = [ENGINE_PATH] + args + [CSV_PATH]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        return f"Error executing C++ engine: {e.stderr}"

@app.route("/")
def index():
    # Fetch statistics
    stats_raw = run_cpp_command(["stats"])
    stats = {"total_stops": "12", "high_crowd_count": "3", "average_crowd": "115.0", "least_crowded_name": "North Suburbs"}
    
    for line in stats_raw.split("\n"):
        if ":" in line:
            k, v = line.split(":", 1)
            if k == "TOTAL_STOPS": stats["total_stops"] = v
            elif k == "HIGH_CROWD": stats["high_crowd_count"] = v
            elif k == "AVG_CROWD": stats["average_crowd"] = f"{float(v):.1f}"
            elif k == "LEAST_CROWDED": stats["least_crowded_name"] = v

    # Fetch stop list for tables and dropdowns
    list_raw = run_cpp_command(["list", "asc"])
    stops = []
    for line in list_raw.split("\n"):
        parts = line.split("|")
        if len(parts) == 4:
            stops.append({"id": parts[0], "name": parts[1], "count": parts[2], "level": parts[3]})

    return render_template("index.html", stats=stats, stops=stops)

@app.route("/search", methods=["POST"])
def search_stop():
    stop_id = request.form.get("stop_id", "").strip().upper()
    res = run_cpp_command(["search", stop_id])
    
    search_result = None
    if "FOUND:ID=" in res:
        search_result = {}
        for part in res.replace("FOUND:", "").split("|"):
            if "=" in part:
                k, v = part.split("=", 1)
                search_result[k.lower()] = v
    
    # Reload dashboard data
    stats_raw = run_cpp_command(["stats"])
    stats = {}
    for line in stats_raw.split("\n"):
        if ":" in line:
            k, v = line.split(":", 1)
            if k == "TOTAL_STOPS": stats["total_stops"] = v
            elif k == "HIGH_CROWD": stats["high_crowd_count"] = v
            elif k == "AVG_CROWD": stats["average_crowd"] = f"{float(v):.1f}"
            elif k == "LEAST_CROWDED": stats["least_crowded_name"] = v

    list_raw = run_cpp_command(["list", "asc"])
    stops = [dict(zip(["id", "name", "count", "level"], l.split("|"))) for l in list_raw.split("\n") if "|" in l]

    return render_template("index.html", stats=stats, stops=stops, search_result=search_result, searched_id=stop_id)

@app.route("/sort", methods=["POST"])
def sort_stops():
    order = request.form.get("order", "asc")
    list_raw = run_cpp_command(["list", order])
    
    stats_raw = run_cpp_command(["stats"])
    stats = {}
    for line in stats_raw.split("\n"):
        if ":" in line:
            k, v = line.split(":", 1)
            if k == "TOTAL_STOPS": stats["total_stops"] = v
            elif k == "HIGH_CROWD": stats["high_crowd_count"] = v
            elif k == "AVG_CROWD": stats["average_crowd"] = f"{float(v):.1f}"
            elif k == "LEAST_CROWDED": stats["least_crowded_name"] = v

    stops = [dict(zip(["id", "name", "count", "level"], l.split("|"))) for l in list_raw.split("\n") if "|" in l]
    return render_template("index.html", stats=stats, stops=stops, current_sort=order)

@app.route("/predict", methods=["POST"])
def predict_departure():
    stop_id = request.form.get("stop_id", "S01")
    res = run_cpp_command(["dp", stop_id])
    
    dp_data = {"optimal_slot_name": "Morning Peak", "min_total_cost": "45.0"}
    for line in res.split("\n"):
        if ":" in line:
            k, v = line.split(":", 1)
            if k == "OPTIMAL_SLOT_NAME": dp_data["optimal_slot_name"] = v
            elif k == "MIN_TOTAL_COST": dp_data["min_total_cost"] = f"{float(v):.1f}"

    return render_template("results.html", title="Departure Time Prediction (DP Result)", result_type="dp", data=dp_data, stop_id=stop_id)

@app.route("/route", methods=["POST"])
def route_planning():
    src = request.form.get("source_id", "S01")
    dest = request.form.get("dest_id", "S09")
    
    res = run_cpp_command(["route", src, dest])
    
    shortest = {"dist": "0", "path": []}
    lowest = {"cost": "0", "dist": "0", "path": []}
    
    current_section = None
    for line in res.split("\n"):
        if "--- SHORTEST ROUTE ---" in line:
            current_section = "shortest"
        elif "--- LOWEST COST ROUTE ---" in line:
            current_section = "lowest"
        elif current_section == "shortest":
            if line.startswith("DIST:"): shortest["dist"] = line.split(":")[1]
            elif line.startswith("PATH:"): shortest["path"] = line.split(":")[1].split(" -> ")
        elif current_section == "lowest":
            if line.startswith("COST:"):
                parts = line.split("|")
                lowest["cost"] = parts[0].split(":")[1]
                if len(parts) > 1: lowest["dist"] = parts[1].split(":")[1]
            elif line.startswith("PATH:"): lowest["path"] = line.split(":")[1].split(" -> ")

    route_data = {"src": src, "dest": dest, "shortest": shortest, "lowest": lowest}
    return render_template("results.html", title="Route Comparison & DAA Analysis", result_type="route", data=route_data)

if __name__ == "__main__":
    app.run(debug=True, port=5000)
