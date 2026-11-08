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
struct Edge {
    int src, dest, weight;
    bool operator<(const Edge& other) const {
        return weight < other.weight;
    }
};

struct TransitStop {
    int id;
    string name;
    int crowdDensity; // 1 to 100 %
    int peakHourDemand;
};

// ============================================================================
// 2. DISJOINT SET UNION (DSU) FOR KRUSKAL'S MST
// ============================================================================
class DSU {
    vector<int> parent, rank;
public:
    DSU(int n) {
        parent.resize(n);
        rank.resize(n, 0);
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]); // Path compression
    }

    void unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i != root_j) {
            if (rank[root_i] < rank[root_j]) swap(root_i, root_j);
            parent[root_j] = root_i;
            if (rank[root_i] == rank[root_j]) rank[root_i]++;
        }
    }
};

// ============================================================================
// 3. GRAPH ENGINE: KRUSKAL'S ALGORITHM
// ============================================================================
class TransportGraph {
    int V;
    vector<Edge> edges;
public:
    TransportGraph(int vertices) : V(vertices) {}

    void addRoute(int u, int v, int congestionCost) {
        edges.push_back({u, v, congestionCost});
    }

    vector<Edge> computeMinimumSpanningBackbone(int& totalCost) {
        sort(edges.begin(), edges.end()); // O(E log E)
        DSU dsu(V);
        vector<Edge> mst;
        totalCost = 0;

        for (const auto& edge : edges) {
            if (dsu.find(edge.src) != dsu.find(edge.dest)) {
                dsu.unite(edge.src, edge.dest);
                mst.push_back(edge);
                totalCost += edge.weight;
            }
        }
        return mst;
    }
};

// ============================================================================
// 4. DYNAMIC PROGRAMMING: FLEET CAPACITY OPTIMIZATION (0/1 KNAPSACK)
// ============================================================================
int optimizeFleetDispatch(int maxCapacity, const vector<int>& weights, const vector<int>& passengerDemand, int n) {
    vector<vector<int>> dp(n + 1, vector<int>(maxCapacity + 1, 0));

    for (int i = 1; i <= n; i++) {
        for (int w = 0; w <= maxCapacity; w++) {
            if (weights[i - 1] <= w) {
                dp[i][w] = max(dp[i - 1][w], passengerDemand[i - 1] + dp[i - 1][w - weights[i - 1]]);
            } else {
                dp[i][w] = dp[i - 1][w];
            }
        }
    }
    return dp[n][maxCapacity];
}

// ============================================================================
// 5. SORTING & SEARCHING ALGORITHMS
// ============================================================================
void merge(vector<TransitStop>& stops, int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;
    vector<TransitStop> L(n1), R(n2);

    for (int i = 0; i < n1; i++) L[i] = stops[l + i];
    for (int j = 0; j < n2; j++) R[j] = stops[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (L[i].crowdDensity >= R[j].crowdDensity) { // Descending Order
            stops[k] = L[i++];
        } else {
            stops[k] = R[j++];
        }
        k++;
    }
    while (i < n1) stops[k++] = L[i++];
    while (j < n2) stops[k++] = R[j++];
}

void mergeSortStops(vector<TransitStop>& stops, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        mergeSortStops(stops, l, m);
        mergeSortStops(stops, m + 1, r);
        merge(stops, l, m, r);
    }
}

int binarySearchStopByDensity(const vector<TransitStop>& sortedStops, int targetDensity) {
    int low = 0, high = sortedStops.size() - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (sortedStops[mid].crowdDensity == targetDensity) return mid;
        if (sortedStops[mid].crowdDensity < targetDensity) high = mid - 1;
        else low = mid + 1;
    }
    return -1;
}

// ============================================================================
// 6. JSON RESPONSE BUILDER FOR FRONTEND INTEGRATION
// ============================================================================
string generateAnalyticsJson() {
    // A. Initialize & Sort Stops (Merge Sort)
    vector<TransitStop> stops = {
        {0, "Central Terminal", 85, 450},
        {1, "Tech Park Station", 92, 600},
        {2, "University Junction", 45, 200},
        {3, "Suburban Hub", 68, 350},
        {4, "Airport Express", 30, 150}
    };
    mergeSortStops(stops, 0, stops.size() - 1);

    // B. Graph Computation (Kruskal's MST)
    TransportGraph graph(5);
    graph.addRoute(0, 1, 15);
    graph.addRoute(0, 2, 10);
    graph.addRoute(1, 2, 5);
    graph.addRoute(1, 3, 25);
    graph.addRoute(2, 3, 20);
    graph.addRoute(3, 4, 8);

    int totalMstCost = 0;
    vector<Edge> mstEdges = graph.computeMinimumSpanningBackbone(totalMstCost);

    // C. Dynamic Programming Optimization
    vector<int> busCapacityCost = {10, 20, 30, 40};
    vector<int> passengerClearance = {60, 100, 120, 240};
    int maxFleetCapacity = 50;
    int maxCleared = optimizeFleetDispatch(maxFleetCapacity, busCapacityCost, passengerClearance, busCapacityCost.size());

    // D. Search Execution
    int searchResultRank = binarySearchStopByDensity(stops, 68);

    // E. Serialize to JSON format
    stringstream json;
    json << "{\n";

    // Stops array
    json << "  \"stops\": [\n";
    for (size_t i = 0; i < stops.size(); ++i) {
        json << "    {\"id\": " << stops[i].id
             << ", \"name\": \"" << stops[i].name << "\""
             << ", \"density\": " << stops[i].crowdDensity
             << ", \"demand\": " << stops[i].peakHourDemand << "}";
        if (i < stops.size() - 1) json << ",";
        json << "\n";
    }
    json << "  ],\n";

    // MST array
    json << "  \"mst\": {\n";
    json << "    \"totalCost\": " << totalMstCost << ",\n";
    json << "    \"edges\": [\n";
    for (size_t i = 0; i < mstEdges.size(); ++i) {
        json << "      {\"src\": " << mstEdges[i].src
             << ", \"dest\": " << mstEdges[i].dest
             << ", \"weight\": " << mstEdges[i].weight << "}";
        if (i < mstEdges.size() - 1) json << ",";
        json << "\n";
    }
    json << "    ]\n";
    json << "  },\n";

    // DP & Search metrics
    json << "  \"dp\": {\n";
    json << "    \"maxCapacity\": " << maxFleetCapacity << ",\n";
    json << "    \"maxClearedPassengers\": " << maxCleared << "\n";
    json << "  },\n";
    json << "  \"searchQuery\": {\n";
    json << "    \"targetDensity\": 68,\n";
    json << "    \"foundIndex\": " << searchResultRank << "\n";
    json << "  }\n";
    json << "}";

    return json.str();
}

// ============================================================================
// 7. MAIN SERVER ROUTER
// ============================================================================
int main() {
    httplib::Server svr;

    // Serve static HTML/CSS files from the public/ folder
    svr.set_mount_point("/", "./public");

    // DAA API endpoint
    svr.Get("/api/analytics", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(generateAnalyticsJson(), "application/json");
    });

    cout << "=========================================================\n";
    cout << " Public Transport Crowd Estimator - Server Running\n";
    cout << " Access UI at: http://localhost:8080\n";
    cout << " API Endpoint: http://localhost:8080/api/analytics\n";
    cout << "=========================================================\n";

    svr.listen("0.0.0.0", 8080);
    return 0;
}
