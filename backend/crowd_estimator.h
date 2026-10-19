#ifndef CROWD_ESTIMATOR_H
#define CROWD_ESTIMATOR_H

#include <string>
#include <vector>
#include <map>
#include <unordered_map>

struct TransportService {
    std::string serviceId;
    std::string transportType; // Metro, Bus, Local Train
    std::string routeName;
    std::string source;
    std::string destination;
    int passengerCount;
    int passengerCapacity;
    std::string timeSlot;
    
    double getOccupancyPercentage() const {
        if (passengerCapacity <= 0) return 0.0;
        return (static_cast<double>(passengerCount) / passengerCapacity) * 100.0;
    }
    
    std::string getCrowdClassification() const {
        double occ = getOccupancyPercentage();
        if (occ <= 40.0) return "Low";
        if (occ <= 70.0) return "Moderate";
        if (occ <= 90.0) return "High";
        if (occ <= 100.0) return "Very High";
        return "Overcrowded";
    }
    
    std::string getBadgeClass() const {
        double occ = getOccupancyPercentage();
        if (occ <= 40.0) return "badge-low";
        if (occ <= 70.0) return "badge-moderate";
        if (occ <= 90.0) return "badge-high";
        if (occ <= 100.0) return "badge-very-high";
        return "badge-overcrowded";
    }
};

struct GraphEdge {
    std::string toStop;
    int travelTimeMinutes;
    int baseCost;
};

struct DPResult {
    int minCost;
    std::vector<std::string> path;
    bool reachable;
};

class CrowdEstimatorBackend {
private:
    std::vector<TransportService> services;
    std::unordered_map<std::string, std::vector<GraphEdge>> adjList;

    // Merge Sort Helper
    void merge(std::vector<TransportService>& arr, int left, int mid, int right);
    void mergeSortHelper(std::vector<TransportService>& arr, int left, int right);

public:
    CrowdEstimatorBackend();

    // Dataset access
    const std::vector<TransportService>& getServices() const { return services; }
    void addService(const TransportService& service);

    // 1. Searching Algorithms
    std::vector<TransportService> linearSearchByRoute(const std::string& routeName) const;
    TransportService* binarySearchById(const std::string& serviceId); // Requires sorted data by ID

    // 2. Sorting Algorithm
    void sortByOccupancy(); // Merge Sort

    // 3. Graph Algorithm (Dijkstra's)
    struct DijkstraResult {
        int totalTime;
        std::vector<std::string> path;
        bool found;
    };
    DijkstraResult findShortestPath(const std::string& start, const std::string& end);

    // 4. Dynamic Programming Algorithm (Bounded-Leg Minimum Cost)
    DPResult findMinimumCostPathDP(const std::string& start, const std::string& end, int maxLegs);

    // Alternative Route Recommendation
    std::vector<TransportService> getAlternativeServices(const std::string& destination, const std::string& excludeId) const;

    // Utility for HTML escaping
    static std::string escapeHtml(const std::string& str);
};

#endif
