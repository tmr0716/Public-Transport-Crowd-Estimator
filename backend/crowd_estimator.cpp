#include "crowd_estimator.h"
#include <algorithm>
#include <queue>
#include <climits>

CrowdEstimatorBackend::CrowdEstimatorBackend() {
    // 15+ initial simulated transport services
    services = {
        {"M-101", "Metro", "Blue Line Express", "Central Station", "Airport Terminal", 320, 400, "08:30 AM"},
        {"M-102", "Metro", "Blue Line Express", "Central Station", "Airport Terminal", 180, 400, "09:00 AM"},
        {"M-103", "Metro", "Red Line Metro", "North Hub", "South Terminal", 380, 350, "08:45 AM"},
        {"B-201", "Bus", "Route 42 Crosstown", "Downtown", "Tech Park", 45, 50, "08:15 AM"},
        {"B-202", "Bus", "Route 42 Crosstown", "Downtown", "Tech Park", 52, 50, "09:15 AM"},
        {"B-203", "Bus", "Route 15 Feeder", "Suburban Mall", "Central Station", 22, 60, "08:00 AM"},
        {"L-301", "Local Train", "Commuter Line Alpha", "East Suburb", "City Center", 850, 1000, "08:00 AM"},
        {"L-302", "Local Train", "Commuter Line Alpha", "East Suburb", "City Center", 1050, 1000, "08:30 AM"},
        {"M-104", "Metro", "Green Line Ring", "West End", "East End", 210, 300, "09:30 AM"},
        {"M-105", "Metro", "Green Line Ring", "West End", "East End", 290, 300, "10:00 AM"},
        {"B-204", "Bus", "Express Shuttle 7", "University Gate", "Downtown", 48, 45, "08:50 AM"},
        {"B-205", "Bus", "Express Shuttle 7", "University Gate", "Downtown", 30, 45, "09:20 AM"},
        {"L-303", "Local Train", "Express Suburban Beta", "North Station", "South Station", 1120, 1100, "09:00 AM"},
        {"M-106", "Metro", "Airport Express", "Central Station", "Airport Terminal", 350, 400, "10:15 AM"},
        {"M-107", "Metro", "Yellow Line", "University", "Museum District", 140, 250, "09:45 AM"},
        {"B-206", "Bus", "Night Owl 1", "Downtown", "Suburban Mall", 15, 50, "11:00 PM"}
    };

    // Initialize Graph Connections (Stops and Weighted Edges: time in mins, cost in currency)
    // Stops: Central Station, North Hub, South Terminal, Downtown, Tech Park, Airport Terminal, East Suburb, City Center
    adjList["Central Station"] = {{"North Hub", 12, 5}, {"Downtown", 8, 3}, {"Airport Terminal", 25, 12}, {"City Center", 15, 6}};
    adjList["North Hub"] = {{"South Terminal", 18, 8}, {"Central Station", 12, 5}};
    adjList["South Terminal"] = {{"Tech Park", 14, 6}, {"North Hub", 18, 8}};
    adjList["Downtown"] = {{"Tech Park", 10, 4}, {"Central Station", 8, 3}, {"City Center", 10, 4}};
    adjList["Tech Park"] = {{"South Terminal", 14, 6}, {"Downtown", 10, 4}};
    adjList["Airport Terminal"] = {{"Central Station", 25, 12}};
    adjList["East Suburb"] = {{"City Center", 20, 8}, {"Central Station", 22, 9}};
    adjList["City Center"] = {{"Central Station", 15, 6}, {"East Suburb", 20, 8}, {"Downtown", 10, 4}};
}

void CrowdEstimatorBackend::addService(const TransportService& service) {
    services.push_back(service);
}

// 1. Linear Search implementation
std::vector<TransportService> CrowdEstimatorBackend::linearSearchByRoute(const std::string& routeName) const {
    std::vector<TransportService> results;
    std::string searchLower = routeName;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (const auto& svc : services) {
        std::string curRoute = svc.routeName;
        std::transform(curRoute.begin(), curRoute.end(), curRoute.begin(), ::tolower);
        if (curRoute.find(searchLower) != std::string::npos) {
            results.push_back(svc);
        }
    }
    return results;
}

// 1. Binary Search implementation (Requires dataset sorted by serviceId)
TransportService* CrowdEstimatorBackend::binarySearchById(const std::string& serviceId) {
    // Ensure sorted copy or operate on sorted vector
    static std::vector<TransportService> sortedServices = services;
    static bool sorted = false;
    if (!sorted) {
        std::sort(sortedServices.begin(), sortedServices.end(), [](const TransportService& a, const TransportService& b) {
            return a.serviceId < b.serviceId;
        });
        sorted = true;
    }

    int left = 0;
    int right = static_cast<int>(sortedServices.size()) - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (sortedServices[mid].serviceId == serviceId) {
            // Find original reference or return from sorted
            for (auto& s : services) {
                if (s.serviceId == serviceId) return &s;
            }
            return &sortedServices[mid];
        }
        if (sortedServices[mid].serviceId < serviceId) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return nullptr;
}

// 2. Merge Sort implementation
void CrowdEstimatorBackend::merge(std::vector<TransportService>& arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    std::vector<TransportService> L(n1);
    std::vector<TransportService> R(n2);

    for (int i = 0; i < n1; i++) L[i] = arr[left + i];
    for (int j = 0; j < n2; j++) R[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (L[i].getOccupancyPercentage() <= R[j].getOccupancyPercentage()) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];
}

void CrowdEstimatorBackend::mergeSortHelper(std::vector<TransportService>& arr, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSortHelper(arr, left, mid);
        mergeSortHelper(arr, mid + 1, right);
        merge(arr, left, mid, right);
    }
}

void CrowdEstimatorBackend::sortByOccupancy() {
    if (!services.empty()) {
        mergeSortHelper(services, 0, static_cast<int>(services.size()) - 1);
    }
}

// 3. Graph Algorithm: Dijkstra's Shortest Path
CrowdEstimatorBackend::DijkstraResult CrowdEstimatorBackend::findShortestPath(const std::string& start, const std::string& end) {
    std::unordered_map<std::string, int> distances;
    std::unordered_map<std::string, std::string> previous;
    std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, std::greater<std::pair<int, std::string>>> pq;

    for (const auto& pair : adjList) {
        distances[pair.first] = INT_MAX;
    }
    distances[start] = 0;
    pq.push({0, start});

    while (!pq.empty()) {
        int dist = pq.top().first;
        std::string u = pq.top().second;
        pq.pop();

        if (dist > distances[u]) continue;
        if (u == end) break;

        if (adjList.find(u) != adjList.end()) {
            for (const auto& edge : adjList[u]) {
                int alt = distances[u] + edge.travelTimeMinutes;
                if (alt < distances[edge.toStop]) {
                    distances[edge.toStop] = alt;
                    previous[edge.toStop] = u;
                    pq.push({alt, edge.toStop});
                }
            }
        }
    }

    DijkstraResult result;
    if (distances.find(end) == distances.end() || distances[end] == INT_MAX) {
        result.found = false;
        result.totalTime = -1;
        return result;
    }

    result.found = true;
    result.totalTime = distances[end];
    std::string curr = end;
    while (curr != start && previous.find(curr) != previous.end()) {
        result.path.push_back(curr);
        curr = previous[curr];
    }
    result.path.push_back(start);
    std::reverse(result.path.begin(), result.path.end());
    return result;
}

// 4. Dynamic Programming Algorithm: Bounded-Leg Minimum Cost Path
DPResult CrowdEstimatorBackend::findMinimumCostPathDP(const std::string& start, const std::string& end, int maxLegs) {
    // DP state: dp[stop][legs] = min cost
    // We can use a tabular / memoized approach
    // For simplicity, let's collect all unique stops
    std::vector<std::string> stops;
    for (const auto& pair : adjList) stops.push_back(pair.first);

    // To track paths, we store parent mapping for reconstruction
    // dpTable[legs][stop]
    const int INF = 1e9;
    int nStops = stops.size();
    
    // Map stop name to index
    std::unordered_map<std::string, int> stopIdx;
    for (int i = 0; i < nStops; i++) stopIdx[stops[i]] = i;

    if (stopIdx.find(start) == stopIdx.end() || stopIdx.find(end) == stopIdx.end()) {
        return {INF, {}, false};
    }

    int startI = stopIdx[start];
    int endI = stopIdx[end];

    // dp[legs][stop]
    std::vector<std::vector<int>> dp(maxLegs + 1, std::vector<int>(nStops, INF));
    std::vector<std::vector<int>> parent(maxLegs + 1, std::vector<int>(nStops, -1));

    dp[0][startI] = 0;

    for (int l = 1; l <= maxLegs; l++) {
        for (int i = 0; i < nStops; i++) {
            dp[l][i] = dp[l - 1][i];
            parent[l][i] = parent[l - 1][i];
        }

        for (int u = 0; u < nStops; u++) {
            if (dp[l - 1][u] == INF) continue;
            std::string uName = stops[u];
            if (adjList.find(uName) != adjList.end()) {
                for (const auto& edge : adjList.at(uName)) {
                    if (stopIdx.find(edge.toStop) != stopIdx.end()) {
                        int v = stopIdx[edge.toStop];
                        int newCost = dp[l - 1][u] + edge.baseCost;
                        if (newCost < dp[l][v]) {
                            dp[l][v] = newCost;
                            parent[l][v] = u;
                        }
                    }
                }
            }
        }
    }

    // Find best cost across legs <= maxLegs
    int bestCost = INF;
    int bestLegs = -1;
    for (int l = 0; l <= maxLegs; l++) {
        if (dp[l][endI] < bestCost) {
            bestCost = dp[l][endI];
            bestLegs = l;
        }
    }

    DPResult result;
    if (bestCost >= INF) {
        result.reachable = false;
        result.minCost = -1;
        return result;
    }

    result.reachable = true;
    result.minCost = bestCost;

    // Reconstruct path
    int currV = endI;
    int currL = bestLegs;
    std::vector<int> pathIndices;
    pathIndices.push_back(currV);

    while (currL > 0 && currV != startI) {
        int p = parent[currL][currV];
        if (p == -1) break;
        pathIndices.push_back(p);
        currV = p;
        currL--;
    }

    std::reverse(pathIndices.begin(), pathIndices.end());
    for (int idx : pathIndices) {
        result.path.push_back(stops[idx]);
    }

    return result;
}

std::vector<TransportService> CrowdEstimatorBackend::getAlternativeServices(const std::string& destination, const std::string& excludeId) const {
    std::vector<std::string> altDestinations = {destination};
    if (destination == "Airport Terminal") altDestinations.push_back("Central Station");
    else if (destination == "Tech Park") altDestinations.push_back("Downtown");

    std::vector<TransportService> alts;
    for (const auto& svc : services) {
        if (svc.serviceId == excludeId) continue;
        for (const auto& d : altDestinations) {
            if (svc.destination == d || svc.source == d) {
                alts.push_back(svc);
                break;
            }
        }
    }
    // Sort alternatives by occupancy ascending
    std::sort(alts.begin(), alts.end(), [](const TransportService& a, const TransportService& b) {
        return a.getOccupancyPercentage() < b.getOccupancyPercentage();
    });
    return alts;
}

std::string CrowdEstimatorBackend::escapeHtml(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '\"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            default:   result += c; break;
        }
    }
    return result;
}
