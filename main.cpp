#include "httplib.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
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

struct KruskalEdge {
    int u, v;
    int travelTimeMinutes;
    double weight;

    bool operator<(const KruskalEdge& other) const {
        return weight < other.weight;
    }
};

// Disjoint Set Union (Union-Find) for Cycle Detection in Kruskal's Algorithm
class DSU {
    vector<int> parent;
public:
    DSU(int n) : parent(n) {
        for (int i = 0; i < n; ++i) parent[i] = i;
    }

    int find(int x) {
        if (parent[x] == x) return x;
        return parent[x] = find(parent[x]); // Path compression
    }

    bool unite(int x, int y) {
        int rootX = find(x);
        int rootY = find(y);
        if (rootX == rootY) return false; // Cycle detected
        parent[rootX] = rootY;
        return true;
    }
};

// ============================================================================
// 2. GRAPH NETWORK & KRUSKAL'S ALGORITHM LOGIC
// ============================================================================
class TransitNetwork {
    int V;
    vector<StopNode> nodes;
    vector<vector<Edge>> adj;

    // DFS helper to extract the unique path between start and end inside Kruskal's MST
    bool findPathInMST(int curr, int target, vector<vector<Edge>>& mstAdj,
                       vector<bool>& visited, vector<int>& path, int& totalTime) {
        visited[curr] = true;
        path.push_back(curr);

        if (curr == target) return true;

        for (const auto& edge : mstAdj[curr]) {
            if (!visited[edge.to]) {
                totalTime += edge.travelTimeMinutes;
                if (findPathInMST(edge.to, target, mstAdj, visited, path, totalTime)) {
                    return true;
                }
                totalTime -= edge.travelTimeMinutes;
            }
        }

        path.pop_back();
        return false;
    }

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
        addRoute(4, 5, 10);
