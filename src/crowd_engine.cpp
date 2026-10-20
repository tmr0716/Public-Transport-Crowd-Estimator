#include "crowd_engine.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <queue>
#include <cmath>

bool CrowdEngine::loadData(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    stops.clear();
    id_to_index.clear();

    std::string line;
    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string val;
        StopNode node;

        std::getline(ss, node.id, ',');
        std::getline(ss, node.name, ',');

        for (int i = 0; i < 5; ++i) {
            std::getline(ss, val, ',');
            node.historical_slots.push_back(std::stoi(val));
        }

        std::getline(ss, val, ',');
        node.current_count = std::stoi(val);

        std::getline(ss, val, ',');
        node.threshold_low = std::stoi(val);

        std::getline(ss, val, ',');
        node.threshold_med = std::stoi(val);

        // Classify Crowd Level
        if (node.current_count <= node.threshold_low) node.crowd_level = "Low";
        else if (node.current_count <= node.threshold_med) node.crowd_level = "Medium";
        else node.crowd_level = "High";

        std::getline(ss, val); // connections string e.g. "S02:5:10,S03:8:15"
        // Remove quotes if present
        if (!val.empty() && val.front() == '"') val.erase(0, 1);
        if (!val.empty() && val.back() == '"') val.pop_back();

        if (!val.empty()) {
            std::stringstream conn_ss(val);
            std::string conn_item;
            while (std::getline(conn_ss, conn_item, ',')) {
                std::stringstream part_ss(conn_item);
                std::string n_id, t_str, p_str;
                std::getline(part_ss, n_id, ':');
                std::getline(part_ss, t_str, ':');
                std::getline(part_ss, p_str, ':');
                if (!n_id.empty() && !t_str.empty() && !p_str.empty()) {
                    node.connections.push_back({n_id, std::stod(t_str), std::stod(p_str)});
                }
            }
        }

        id_to_index[node.id] = stops.size();
        stops.push_back(node);
    }
    return true;
}

const std::vector<StopNode>& CrowdEngine::getStops() const {
    return stops;
}

// --- Manual Merge Sort Implementation ---
void merge(std::vector<StopNode>& list, int left, int mid, int right, bool ascending) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    std::vector<StopNode> L(n1), R(n2);
    for (int i = 0; i < n1; i++) L[i] = list[left + i];
    for (int j = 0; j < n2; j++) R[j] = list[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        bool condition = ascending ? (L[i].current_count <= R[j].current_count) 
                                   : (L[i].current_count >= R[j].current_count);
        if (condition) {
            list[k++] = L[i++];
        } else {
            list[k++] = R[j++];
        }
    }
    while (i < n1) list[k++] = L[i++];
    while (j < n2) list[k++] = R[j++];
}

void mergeSortHelper(std::vector<StopNode>& list, int left, int right, bool ascending) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSortHelper(list, left, mid, ascending);
        mergeSortHelper(list, mid + 1, right, ascending);
        merge(list, left, mid, right, ascending);
    }
}

void CrowdEngine::sortStopsByMergeSort(std::vector<StopNode>& list, bool ascending) {
    if (list.empty()) return;
    mergeSortHelper(list, 0, list.size() - 1, ascending);
}

// --- Manual Binary Search Implementation ---
int CrowdEngine::binarySearchStopById(const std::vector<StopNode>& sorted_list, const std::string& target_id) const {
    int low = 0;
    int high = sorted_list.size() - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (sorted_list[mid].id == target_id) return mid;
        if (sorted_list[mid].id < target_id) low = mid + 1;
        else high = mid - 1;
    }
    return -1;
}

// --- Dijkstra's Shortest Distance Route ---
CrowdEngine::RouteResult CrowdEngine::findShortestDistanceRoute(const std::string& src_id, const std::string& dest_id) const {
    std::unordered_map<std::string, double> distances;
    std::unordered_map<std::string, std::string> parent;
    std::priority_queue<std::pair<double, std::string>, std::vector<std::pair<double, std::string>>, std::greater<>> pq;

    for (const auto& stop : stops) {
        distances[stop.id] = 1e9;
    }

    if (distances.find(src_id) == distances.end() || distances.find(dest_id) == distances.end()) {
        return {{}, 0, 0, false};
    }

    distances[src_id] = 0;
    pq.push({0, src_id});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > distances[u]) continue;
        if (u == dest_id) break;

        // Find node u connections
        auto it = std::find_if(stops.begin(), stops.end(), [&u](const StopNode& s) { return s.id == u; });
        if (it != stops.end()) {
            for (const auto& edge : it->connections) {
                double weight = edge.travel_time;
                if (distances[u] + weight < distances[edge.neighbor_id]) {
                    distances[edge.neighbor_id] = distances[u] + weight;
                    parent[edge.neighbor_id] = u;
                    pq.push({distances[edge.neighbor_id], edge.neighbor_id});
                }
            }
        }
    }

    if (distances[dest_id] >= 1e8) {
        return {{}, 0, 0, false};
    }

    // Reconstruct path
    std::vector<std::string> path;
    std::string curr = dest_id;
    while (curr != src_id) {
        // find name
        auto it = std::find_if(stops.begin(), stops.end(), [&curr](const StopNode& s) { return s.id == curr; });
        path.push_back(it != stops.end() ? it->name : curr);
        curr = parent[curr];
    }
    auto src_it = std::find_if(stops.begin(), stops.end(), [&src_id](const StopNode& s) { return s.id == src_id; });
    path.push_back(src_it != stops.end() ? src_it->name : src_id);
    std::reverse(path.begin(), path.end());

    return {path, distances[dest_id], distances[dest_id], true};
}

// --- Dijkstra's Lowest Cost (Crowd-Aware) Route ---
CrowdEngine::RouteResult CrowdEngine::findLowestCostRoute(const std::string& src_id, const std::string& dest_id, double alpha, double beta) const {
    std::unordered_map<std::string, double> costs;
    std::unordered_map<std::string, std::string> parent;
    std::priority_queue<std::pair<double, std::string>, std::vector<std::pair<double, std::string>>, std::greater<>> pq;

    for (const auto& stop : stops) {
        costs[stop.id] = 1e9;
    }

    if (costs.find(src_id) == costs.end() || costs.find(dest_id) == costs.end()) {
        return {{}, 0, 0, false};
    }

    costs[src_id] = 0;
    pq.push({0, src_id});

    while (!pq.empty()) {
        auto [c, u] = pq.top();
        pq.pop();

        if (c > costs[u]) continue;
        if (u == dest_id) break;

        auto it = std::find_if(stops.begin(), stops.end(), [&u](const StopNode& s) { return s.id == u; });
        if (it != stops.end()) {
            for (const auto& edge : it->connections) {
                // Find neighbor current crowd count
                auto n_it = std::find_if(stops.begin(), stops.end(), [&edge](const StopNode& s) { return s.id == edge.neighbor_id; });
                int neighbor_crowd = (n_it != stops.end()) ? n_it->current_count : 50;

                double edge_cost = (alpha * edge.travel_time) + (beta * neighbor_crowd * 0.1);
                if (costs[u] + edge_cost < costs[edge.neighbor_id]) {
                    costs[edge.neighbor_id] = costs[u] + edge_cost;
                    parent[edge.neighbor_id] = u;
                    pq.push({costs[edge.neighbor_id], edge.neighbor_id});
                }
            }
        }
    }

    if (costs[dest_id] >= 1e8) {
        return {{}, 0, 0, false};
    }

    std::vector<std::string> path;
    std::string curr = dest_id;
    double actual_distance = 0;
    while (curr != src_id) {
        auto it = std::find_if(stops.begin(), stops.end(), [&curr](const StopNode& s) { return s.id == curr; });
        path.push_back(it != stops.end() ? it->name : curr);
        std::string p_node = parent[curr];
        // find edge weight
        auto p_it = std::find_if(stops.begin(), stops.end(), [&p_node](const StopNode& s) { return s.id == p_node; });
        if (p_it != stops.end()) {
            for (const auto& e : p_it->connections) {
                if (e.neighbor_id == curr) actual_distance += e.travel_time;
            }
        }
        curr = p_node;
    }
    auto src_it = std::find_if(stops.begin(), stops.end(), [&src_id](const StopNode& s) { return s.id == src_id; });
    path.push_back(src_it != stops.end() ? src_it->name : src_id);
    std::reverse(path.begin(), path.end());

    return {path, actual_distance, costs[dest_id], true};
}

// --- Dynamic Programming Optimal Departure Time ---
CrowdEngine::DPResult CrowdEngine::computeOptimalDepartureTime(const std::string& stop_id, double weight_crowd, double weight_wait) const {
    auto it = std::find_if(stops.begin(), stops.end(), [&stop_id](const StopNode& s) { return s.id == stop_id; });
    if (it == stops.end() || it->historical_slots.empty()) {
        return {0, 0.0, {}};
    }

    const auto& slots = it->historical_slots;
    int n = slots.size();
    std::vector<double> dp(n, 0.0);
    std::vector<double> slot_costs(n, 0.0);

    for (int i = 0; i < n; ++i) {
        slot_costs[i] = (slots[i] * weight_crowd) + (i * 5.0 * weight_wait);
    }

    // Base case: last slot
    dp[n - 1] = slot_costs[n - 1];

    // Tabulation recurrence: DP[i] = slot_cost[i] + min_{j > i} (DP[j] + transition_penalty)
    for (int i = n - 2; i >= 0; --i) {
        double min_future = 1e9;
        for (int j = i + 1; j < n; ++j) {
            double transition_penalty = (j - i) * 2.5; // waiting or delay penalty
            if (dp[j] + transition_penalty < min_future) {
                min_future = dp[j] + transition_penalty;
            }
        }
        dp[i] = slot_costs[i] + min_future;
    }

    // Find optimal starting slot minimizing dp[i]
    int best_slot = 0;
    double min_cost = dp[0];
    for (int i = 1; i < n; ++i) {
        if (dp[i] < min_cost) {
            min_cost = dp[i];
            best_slot = i;
        }
    }

    return {best_slot, min_cost, slot_costs};
}

// --- Statistics ---
CrowdEngine::Statistics CrowdEngine::getStatistics() const {
    int total = stops.size();
    if (total == 0) return {0, 0, 0.0, "N/A"};

    int high_count = 0;
    double sum = 0;
    int min_crowd = 1e9;
    std::string least_name = "N/A";

    for (const auto& s : stops) {
        sum += s.current_count;
        if (s.crowd_level == "High") high_count++;
        if (s.current_count < min_crowd) {
            min_crowd = s.current_count;
            least_name = s.name;
        }
    }

    return {total, high_count, sum / total, least_name};
}
