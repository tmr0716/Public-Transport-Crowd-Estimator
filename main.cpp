#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>

using namespace std;

// --- 1. DATA STRUCTURES ---
struct Edge {
    int src, dest, weight;
    bool operator<(const Edge& other) const {
        return weight < other.weight;
    }
};

struct TransitStop {
    int id;
    string name;
    int crowdDensity; // 1 to 100
    int peakHourDemand;
};

// --- 2. DISJOINT SET UNION (DSU) FOR KRUSKAL'S MST ---
class DSU {
    vector<int> parent, rank;
public:
    DSU(int n) {
        parent.resize(n);
        rank.resize(n, 0);
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    int find(int i) {
        if (parent[i] == i)
            return i;
        return parent[i] = find(parent[i]); // Path compression
    }

    void unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i != root_j) {
            if (rank[root_i] < rank[root_j])
                swap(root_i, root_j);
            parent[root_j] = root_i;
            if (rank[root_i] == rank[root_j])
                rank[root_i]++;
        }
    }
};

// --- 3. GRAPH ENGINE: KRUSKAL'S ALGORITHM ---
class TransportGraph {
    int V;
    vector<Edge> edges;
public:
    TransportGraph(int vertices) : V(vertices) {}

    void addRoute(int u, int v, int congestionCost) {
        edges.push_back({u, v, congestionCost});
    }

    void computeMinimumSpanningBackbone() {
        sort(edges.begin(), edges.end()); // O(E log E)
        DSU dsu(V);
        vector<Edge> mst;
        int totalCost = 0;

        for (const auto& edge : edges) {
            if (dsu.find(edge.src) != dsu.find(edge.dest)) {
                dsu.unite(edge.src, edge.dest);
                mst.push_back(edge);
                totalCost += edge.weight;
            }
        }

        cout << "\n============================================\n";
        cout << " [MST] OPTIMAL TRANSIT BACKBONE (Kruskal's)\n";
        cout << "============================================\n";
        for (const auto& e : mst) {
            cout << " Corridor: Stop " << e.src << " <---> Stop " << e.dest 
                 << " | Congestion Weight: " << e.weight << "\n";
        }
        cout << " Minimum Total Congestion Backbone Weight: " << totalCost << "\n\n";
    }
};

// --- 4. DYNAMIC PROGRAMMING: CROWD CAPACITATED ROUTE SELECTION ---
// Knapsack DP: Maximize passenger clearance given max fleet capacity
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

// --- 5. SORTING & SEARCHING ALGORITHMS ---
void merge(vector<TransitStop>& stops, int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;
    vector<TransitStop> L(n1), R(n2);

    for (int i = 0; i < n1; i++) L[i] = stops[l + i];
    for (int j = 0; j < n2; j++) R[j] = stops[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (L[i].crowdDensity >= R[j].crowdDensity) { // Descending order
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
        if (sortedStops[mid].crowdDensity < targetDensity) high = mid - 1; // Reverse sorted
        else low = mid + 1;
    }
    return -1;
}

// --- MAIN EXECUTION ---
int main() {
    // 1. Initialize Stops
    vector<TransitStop> stops = {
        {0, "Central Terminal", 85, 450},
        {1, "Tech Park Station", 92, 600},
        {2, "University Junction", 45, 200},
        {3, "Suburban Hub", 68, 350},
        {4, "Airport Express", 30, 150}
    };

    // 2. Sorting by Crowd Density (Merge Sort)
    mergeSortStops(stops, 0, stops.size() - 1);

    cout << "============================================\n";
    cout << " [SORTING] STOPS RANKED BY CONGESTION (Merge Sort)\n";
    cout << "============================================\n";
    for (const auto& s : stops) {
        cout << " [" << s.id << "] " << left << setw(22) << s.name 
             << " | Density: " << s.crowdDensity << "% | Demand: " << s.peakHourDemand << "\n";
    }

    // 3. Binary Search Example
    int target = 68;
    int index = binarySearchStopByDensity(stops, target);
    cout << "\n============================================\n";
    cout << " [SEARCHING] BINARY SEARCH FOR DENSITY = " << target << "%\n";
    cout << "============================================\n";
    if (index != -1) {
        cout << " Found Stop: " << stops[index].name << " at sorted rank " << index + 1 << "\n";
    } else {
        cout << " Stop with density " << target << "% not found.\n";
    }

    // 4. Graph Construction & Kruskal's MST
    TransportGraph graph(5);
    graph.addRoute(0, 1, 15);
    graph.addRoute(0, 2, 10);
    graph.addRoute(1, 2, 5);
    graph.addRoute(1, 3, 25);
    graph.addRoute(2, 3, 20);
    graph.addRoute(3, 4, 8);

    graph.computeMinimumSpanningBackbone();

    // 5. Dynamic Programming Allocation
    vector<int> busCapacityCost = {10, 20, 30, 40};
    vector<int> passengerClearance = {60, 100, 120, 240};
    int maxFleetCapacity = 50;

    int maxCleared = optimizeFleetDispatch(maxFleetCapacity, busCapacityCost, passengerClearance, busCapacityCost.size());

    cout << "============================================\n";
    cout << " [DP] FLEET CAPACITY OPTIMIZATION (Knapsack DP)\n";
    cout << "============================================\n";
    cout << " Maximum Fleet Bus Capacity Limit: " << maxFleetCapacity << " units\n";
    cout << " Maximum Passenger Flow Cleared: " << maxCleared << " passengers/hr\n";
    cout << "============================================\n";

    return 0;
}