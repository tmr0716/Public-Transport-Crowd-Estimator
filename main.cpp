#include "httplib.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <queue>
#include <sstream>

using namespace std;

// ============================================================================
// 1. DATA STRUCTURES
// ============================================================================
struct StopNode {
    int id;
    string name;
    int crowdDensity; // Percentage (0 - 100%)
    int x, y;         // Map SVG coordinates (720x380 viewBox)
};

struct Edge {
    int to;
    int travelTimeMinutes;
};

// ============================================================================
// 2. GRAPH NETWORK & SHORTEST PATH LOGIC (PUNE TRANSIT NETWORK)
// ============================================================================
class TransitNetwork {
    int V;
    vector<StopNode> nodes;
    vector<vector<Edge>> adj;

public:
    TransitNetwork() {
        // 10 realistic Pune PMPML / Metro stops with balanced SVG coordinates
        nodes = {
            {0, "Hinjawadi Phase 1", 85,  90,  75},
            {1, "Chinchwad",         72, 235,  55},
            {2, "Aundh",             58, 235, 165},
            {3, "Shivajinagar",      82, 385, 135},
            {4, "Manapa",            90, 455, 215},
            {5, "Deccan Gymkhana",   64, 320, 260},
            {6, "Kothrud Depot",     45, 135, 305},
            {7, "Swargate",          88, 445, 325},
            {8, "Viman Nagar",       52, 610,  95},
            {9, "Hadapsar",          76, 625, 290}
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
    }

    void addRoute(int u, int v, int time) {
        adj[u].push_back({v, time});
        adj[v].push_back({u, time});
    }

    // Returns ALL stops and ALL edges for rendering the full network map
    string getFullNetworkJson() {
        stringstream json;
        json << "{\n  \"nodes\": [";
        for (size_t i = 0; i < nodes.size(); ++i) {
            json << "{\"id\":" << nodes[i].id
                 << ",\"name\":\"" << nodes[i].name << "\""
                 << ",\"density\":" << nodes[i].crowdDensity
                 << ",\"x\":" << nodes[i].x
                 << ",\"y\":" << nodes[i].y << "}";
            if (i < nodes.size() - 1) json << ",";
        }
        json << "],\n  \"edges\": [";
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
        json << "]\n}";
        return json.str();
    }

    // Shortest Path Computation (Dijkstra's Algorithm)
    string findOptimalRouteJson(int start, int end) {
        if (start < 0 || start >= V || end < 0 || end >= V) {
            return "{\"error\":\"Invalid stop ID\"}";
        }

        vector<int> dist(V, 1e9);
        vector<int> parent(V, -1);
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;

        dist[start] = 0;
        pq.push({0, start});

        while (!pq.empty()) {
            int u = pq.top().second;
            int d = pq.top().first;
            pq.pop();

            if (d > dist[u]) continue;

            for (const auto& edge : adj[u]) {
                if (dist[u] + edge.travelTimeMinutes < dist[edge.to]) {
                    dist[edge.to] = dist[u] + edge.travelTimeMinutes;
                    parent[edge.to] = u;
                    pq.push({dist[edge.to], edge.to});
                }
            }
        }

        // Reconstruct path
        vector<int> path;
        for (int v = end; v != -1; v = parent[v]) {
            path.push_back(v);
        }
        reverse(path.begin(), path.end());

        // Calculate average crowd density
        int totalCrowd = 0;
        for (int nodeIdx : path) {
            totalCrowd += nodes[nodeIdx].crowdDensity;
        }
        int avgCrowd = path.empty() ? 0 : totalCrowd / path.size();

        // Build JSON response
        stringstream json;
        json << "{\n";
        json << "  \"totalTime\": " << dist[end] << ",\n";
        json << "  \"avgCrowd\": " << avgCrowd << ",\n";
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
        json << "]\n}";

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

    // Full network endpoint
    svr.Get("/api/network", [&network](const httplib::Request& req, httplib::Response& res) {
        res.set_content(network.getFullNetworkJson(), "application/json");
    });

    // Shortest route calculation endpoint
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
