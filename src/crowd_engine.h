#ifndef CROWD_ENGINE_H
#define CROWD_ENGINE_H

#include <string>
#include <vector>
#include <unordered_map>

struct Edge {
    std::string neighbor_id;
    double travel_time;
    double crowd_penalty_weight;
};

struct StopNode {
    std::string id;
    std::string name;
    std::vector<int> historical_slots; // 5 slots
    int current_count;
    int threshold_low;
    int threshold_med;
    std::string crowd_level; // Low, Medium, High
    std::vector<Edge> connections;
};

class CrowdEngine {
private:
    std::vector<StopNode> stops;
    std::unordered_map<string, int> id_to_index;

public:
    bool loadData(const std::string& filepath);
    const std::vector<StopNode>& getStops() const;
    
    // DAA Algorithms
    void sortStopsByMergeSort(std::vector<StopNode>& list, bool ascending);
    int binarySearchStopById(const std::vector<StopNode>& sorted_list, const std::string& target_id) const;
    
    // Dijkstra Routing (Returns path, total distance, total cost)
    struct RouteResult {
        std::vector<std::string> path_names;
        double total_distance;
        double total_cost;
        bool found;
    };
    RouteResult findShortestDistanceRoute(const std::string& src_id, const std::string& dest_id) const;
    RouteResult findLowestCostRoute(const std::string& src_id, const std::string& dest_id, double alpha, double beta) const;

    // Dynamic Programming Optimal Departure Time
    struct DPResult {
        int optimal_slot;
        double min_total_cost;
        std::vector<double> slot_costs;
    };
    DPResult computeOptimalDepartureTime(const std::string& stop_id, double weight_crowd, double weight_wait) const;

    // Statistics
    struct Statistics {
        int total_stops;
        int high_crowd_count;
        double average_crowd;
        std::string least_crowded_name;
    };
    Statistics getStatistics() const;
};

#endif
