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
    int x, y;         // Map SVG coordinates
};

struct Edge {
    int to;
    int travelTimeMinutes;
};

// ============================================================================
// 2. GRAPH NETWORK & SHORTEST PATH LOGIC
// ============================================================================
class TransitNetwork {
    int V;
    vector<StopNode> nodes;
    vector<vector<Edge>> adj;

public:
    TransitNetwork() {
        // Initialize 5 transit stops with map coordinates
        nodes = {
            {0, "Central Terminal", 30, 100, 150},
            {1, "Tech Park Station", 88, 250, 70},
            {2, "University Junction", 45, 250, 230},
            {3, "Suburban Hub", 65, 400, 150},
            {4, "Airport Express", 25, 500, 230}
        };

        V = nodes.size();
        adj.resize(V);

        // Add connecting transit routes (u, v, travel time in mins)
        addRoute(0, 1, 12);
        addRoute(0, 2, 8);
        addRoute(1, 3, 10);
        addRoute(2, 3, 14);
        addRoute(3, 4, 15);
    }

    void addRoute(int u, int v, int time) {
        adj[u].push_back({v, time});
        adj[v].push_back({u, time});
    }

    // Shortest Path Computation (Dijkstra's Algorithm)
    string findOptimalRouteJson(int start, int end) {
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

        // Reconstruct path nodes
        vector<int> path;
        for (int v = end; v != -1; v = parent[v]) {
            path.push_back(v);
        }
        reverse(path.begin(), path.end());

        // Calculate Average Congestion along path
        int totalCrowd = 0;
        for (int nodeIdx : path) {
            totalCrowd += nodes[nodeIdx].crowdDensity;
        }
        int avgCrowd = path.empty() ? 0 : totalCrowd / path.size();

        // Build JSON payload for frontend consumption
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

    // Serve HTML, CSS, JS from the public folder
    svr.set_mount_point("/", "./public");

    // Dynamic Route Calculation API Endpoint
    svr.Get("/api/route", [&network](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("start") && req.has_param("end")) {
            int start = stoi(req.get_param_value("start"));
            int end = stoi(req.get_param_value("end"));
            
            string jsonResult = network.findOptimalRouteJson(start, end);
            res.set_content(jsonResult, "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"error\":\"Missing start or end parameter\"}", "application/json");
        }
    });

    cout << "=========================================================\n";
    cout << " Public Transport Route Server running on http://localhost:8080\n";
    cout << "=========================================================\n";

    svr.listen("0.0.0.0", 8080);
    return 0;
}
