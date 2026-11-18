#include "httplib.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <random>
#include <cmath>

using namespace std;

// ============================================================================
// 1. DATA STRUCTURES
// ============================================================================
struct StopNode {
    int id;
    string name;
    int crowdDensity; // Dynamic random stop crowd percentage (15% - 95%)
    int x, y;         // Map SVG coordinates (720x380 viewBox)
};

struct Edge {
    int to;
    int travelTimeMinutes;
};

struct BusInfo {
    string busNumber;
    string busRoute;
    int passengersInside;
    int maxCapacity;
    int availableSeats;
    int occupancyPercent;
    string crowdStatus;
};

// ============================================================================
// 2. GRAPH NETWORK & DYNAMIC PROGRAMMING ROUTE ENGINE
// ============================================================================
class TransitNetwork {
    int V;
    vector<StopNode> nodes;
    vector<vector<Edge>> adj;
    mt19937 rng;

    // Generates fresh random crowd densities (15% to 95%) for all stops
    void randomizeCrowdLevels() {
        uniform_int_distribution<int> dist(15, 95);
        for (auto& node : nodes) {
            node.crowdDensity = dist(rng);
        }
    }

    // Separate Bus Crowd Estimation generator (independent of Stop crowdDensity)
    BusInfo generateBusEstimation(const vector<int>& path) {
        static const vector<string> puneBusNumbers = {
            "MH-12 AB 1234",
            "MH-12 PQ 4521",
            "MH-12 RN 7890",
            "MH-14 GD 3319",
            "MH-12 SF 6042",
            "MH-14 JX 9184"
        };

        uniform_int_distribution<size_t> busDist(0, puneBusNumbers.size() - 1);
        int maxCap = 60;
        uniform_int_distribution<int> passDist(10, 58);

        BusInfo bus;
        bus.busNumber = puneBusNumbers[busDist(rng)];

        stringstream routeStr;
        for (size_t i = 0; i < path.size(); ++i) {
            routeStr << nodes[path[i]].name;
            if (i < path.size() - 1) routeStr << " \xE2\x86\x92 "; // UTF-8 right arrow
        }
        bus.busRoute = routeStr.str();

        bus.maxCapacity = maxCap;
        bus.passengersInside = passDist(rng);
        bus.availableSeats = max(0, bus.maxCapacity - bus.passengersInside);
        bus.occupancyPercent = static_cast<int>(round((static_cast<double>(bus.passengersInside) / bus.maxCapacity) * 100.0));

        if (bus.occupancyPercent <= 40) {
            bus.crowdStatus = "LOW CROWD";
        } else if (bus.occupancyPercent <= 70) {
            bus.crowdStatus = "MODERATE CROWD";
        } else {
            bus.crowdStatus = "HIGH CROWD";
        }

        return bus;
    }

    // Helper to serialize all nodes & edges
    string serializeFullNetwork() const {
        stringstream json;
        json << "\"nodes\": [";
        for (size_t i = 0; i < nodes.size(); ++i) {
            json << "{\"id\":" << nodes[i].id
                 << ",\"name\":\"" << nodes[i].name << "\""
                 << ",\"density\":" << nodes[i].crowdDensity
                 << ",\"x\":" << nodes[i].x
                 << ",\"y\":" << nodes[i].y << "}";
            if (i < nodes.size() - 1) json << ",";
        }
        json << "], \"edges\": [";
        bool firstEdge = true;
        for (int u = 0; u < V; ++u) {
            for (const auto& edge : adj[u]) {
                if (u < edge.to) {
                    if (!firstEdge) json << ",";
                    json << "{\"from\":" << u
                         << ",\"to\":" << edge.to
                         << ",\"time\":" << edge.travelTimeMinutes << "}";
                    firstEdge = false;
                }
            }
        }
        json << "]";
        return json.str();
    }

public:
    TransitNetwork() : rng(random_device{}()) {
        // 10 realistic Pune PMPML / Metro stops with balanced SVG coordinates
        nodes = {
            {0, "Hinjawadi Phase 1", 50,  90,  75},
            {1, "Chinchwad",         50, 235,  55},
            {2, "Aundh",             50, 235, 165},
            {3, "Shivajinagar",      50, 385, 135},
            {4, "Manapa",            50, 455, 215},
            {5, "Deccan Gymkhana",   50, 320, 260},
            {6, "Kothrud Depot",     50, 135, 305},
            {7, "Swargate",          50, 445, 325},
            {8, "Viman Nagar",       50, 610,  95},
            {9, "Hadapsar",          50, 625, 290}
        };

        V = nodes.size();
        adj.resize(V);

        // Bidirectional Pune transit routes (u, v, travel time in mins)
        addRoute(0, 1, 18); // Hinjawadi <-> Chinchwad
        addRoute(0, 2, 16); // Hinjawadi <-> Aundh
        addRoute(1, 2, 14); // Chinchwad <-> Aundh
        addRoute(1, 3, 22); // Chinchwad <-> Shivajinagar
        addRoute(2, 3, 12); // Aundh <-> Shivajinagar
        addRoute(2, 5, 15); // Aundh <-> Deccan Gymkhana
        addRoute(3, 4,  8); // Shivajinagar <-> Manapa
        addRoute(3, 8, 20); // Shivajinagar <-> Viman Nagar
        addRoute(4, 5, 10); // Manapa <-> Deccan Gymkhana
        addRoute(4, 7, 12); // Manapa <-> Swargate
        addRoute(4, 9, 24); // Manapa <-> Hadapsar
        addRoute(5, 6, 14); // Deccan Gymkhana <-> Kothrud Depot
        addRoute(5, 7, 13); // Deccan Gymkhana <-> Swargate
        addRoute(7, 9, 22); // Swargate <-> Hadapsar
        addRoute(8, 9, 25); // Viman Nagar <-> Hadapsar

        randomizeCrowdLevels();
    }

    void addRoute(int u, int v, int time) {
        adj[u].push_back({v, time});
        adj[v].push_back({u, time});
    }

    // Returns initial stop list
    string getFullNetworkJson() {
        return "{\n  " + serializeFullNetwork() + "\n}";
    }

    // Dynamic Programming (Space-Optimized Bellman-Ford) Crowd-Aware Route Finder
    string findOptimalRouteJson(int start, int end) {
        if (start < 0 || start >= V || end < 0 || end >= V) {
            return "{\"error\":\"Invalid stop ID\"}";
        }

        // Generate fresh random crowd % for every stop on each request
        randomizeCrowdLevels();

        // DP tables: minimum crowd-penalized cost, actual travel time, and parent pointers
        vector<double> dpCost(V, 1e9);
        vector<int> actualTime(V, 1e9);
        vector<int> parent(V, -1);

        dpCost[start] = 0.0;
        actualTime[start] = 0;

        const double CROWD_PENALTY_WEIGHT = 85.0;

        // DP Tabulation: V - 1 edge relaxation stages
        for (int step = 1; step <= V - 1; ++step) {
            bool updated = false;
            for (int u = 0; u < V; ++u) {
                if (dpCost[u] >= 1e9) continue;

                for (const auto& edge : adj[u]) {
                    int v = edge.to;
                    // Penalize intermediate stop congestion; destination itself has 0 detour penalty
                    int crowd = (v == end) ? 0 : nodes[v].crowdDensity;
                    double ratio = crowd / 100.0;
                    double weight = edge.travelTimeMinutes + (ratio * ratio * CROWD_PENALTY_WEIGHT);

                    if (dpCost[u] + weight < dpCost[v]) {
                        dpCost[v] = dpCost[u] + weight;
                        actualTime[v] = actualTime[u] + edge.travelTimeMinutes;
                        parent[v] = u;
                        updated = true;
                    }
                }
            }
            if (!updated) break; // Early termination if DP table converged
        }

        // Backtrack parent pointers to reconstruct optimal path
        vector<int> path;
        for (int v = end; v != -1; v = parent[v]) {
            path.push_back(v);
        }
        reverse(path.begin(), path.end());

        // Calculate average stop congestion along the chosen path
        int totalCrowd = 0;
        for (int nodeIdx : path) {
            totalCrowd += nodes[nodeIdx].crowdDensity;
        }
        int avgCrowd = path.empty() ? 0 : totalCrowd / path.size();

        // Generate independent Bus Crowd Estimation for this route
        BusInfo bus = generateBusEstimation(path);

        // Build JSON response
        stringstream json;
        json << "{\n";
        json << "  \"totalTime\": " << actualTime[end] << ",\n";
        json << "  \"avgCrowd\": " << avgCrowd << ",\n";
        json << "  " << serializeFullNetwork() << ",\n";
        json << "  \"path\": [";
        for (size_t i = 0; i < path.size(); ++i) {
            int id = path[i];
            json << "{\"id\":" << nodes[id].id
                 << ",\"name\":\"" << nodes[id].name << "\""
                 << ",\"density\":" << nodes[id].crowdDensity
                 << ",\"x\":" << nodes[id].x
                 << ",\"y\":" << nodes[id].y << "}";
            if (i < path.size() - 1) json << ",";
        }
        json << "],\n";
        json << "  \"bus\": {\n";
        json << "    \"busNumber\": \"" << bus.busNumber << "\",\n";
        json << "    \"busRoute\": \"" << bus.busRoute << "\",\n";
        json << "    \"passengersInside\": " << bus.passengersInside << ",\n";
        json << "    \"maxCapacity\": " << bus.maxCapacity << ",\n";
        json << "    \"availableSeats\": " << bus.availableSeats << ",\n";
        json << "    \"occupancyPercent\": " << bus.occupancyPercent << ",\n";
        json << "    \"crowdStatus\": \"" << bus.crowdStatus << "\"\n";
        json << "  }\n";
        json << "}";

        return json.str();
    }
};

// ============================================================================
// 3. MAIN SERVER ROUTER
// ============================================================================
int main() {
    httplib::Server svr;
    TransitNetwork network;

    // Serve static files from ./public
    svr.set_mount_point("/", "./public");

    // Initial network endpoint
    svr.Get("/api/network", [&network](const httplib::Request& req, httplib::Response& res) {
        res.set_content(network.getFullNetworkJson(), "application/json");
    });

    // Dynamic Programming crowd-aware route endpoint
    svr.Get("/api/route", [&network](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("start") && req.has_param("end")) {
            try {
                int start = stoi(req.get_param_value("start"));
                int end = stoi(req.get_param_value("end"));
                string jsonResult = network.findOptimalRouteJson(start, end);
                res.set_content(jsonResult, "application/json");
            } catch (...) {
                res.status = 400;
                res.set_content("{\"error\":\"Invalid numeric parameter\"}", "application/json");
            }
        } else {
            res.status = 400;
            res.set_content("{\"error\":\"Missing start or end parameter\"}", "application/json");
        }
    });

    cout << "=========================================================\n";
    cout << " Pune Public Transport Server running on http://localhost:8080\n";
    cout << "=========================================================\n";

    svr.listen("0.0.0.0", 8080);
    return 0;
}
