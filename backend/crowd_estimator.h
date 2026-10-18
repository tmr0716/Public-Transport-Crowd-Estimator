#ifndef CROWD_ESTIMATOR_H
#define CROWD_ESTIMATOR_H

#include <string>
#include <vector>

// Structure representing a public transport service
struct TransportService {
    std::string serviceId;     // e.g., "M-101"
    std::string transportType; // "Bus", "Metro", "Local"
    std::string routeName;     // e.g., "Blue Line Express"
    std::string destination;   // e.g., "Central Station"
    int passengerCount;        // Current estimated passengers
    int passengerCapacity;     // Maximum passenger capacity
    std::string timeSlot;      // e.g., "08:00"

    // Calculates occupancy percentage
    double getOccupancyPercentage() const {
        if (passengerCapacity <= 0) return 0.0;
        return (static_cast<double>(passengerCount) / passengerCapacity) * 100.0;
    }

    // Classifies crowd level based on strict thresholds
    std::string getCrowdClassification() const {
        double occ = getOccupancyPercentage();
        if (occ <= 40.0) return "Low";
        else if (occ <= 70.0) return "Moderate";
        else if (occ <= 90.0) return "High";
        else if (occ <= 100.0) return "Very High";
        else return "Overcrowded";
    }
};

class CrowdEstimatorBackend {
private:
    std::vector<TransportService> services;

    // Helper functions for Merge Sort
    void merge(std::vector<TransportService>& arr, int left, int mid, int right);
    void mergeSortHelper(std::vector<TransportService>& arr, int left, int right);

public:
    CrowdEstimatorBackend();

    // Dataset Management
    void addService(const TransportService& service);
    const std::vector<TransportService>& getAllServices() const;
    void loadSampleData();

    // DAA Algorithm Implementations
    int linearSearchByRoute(const std::string& routeName) const;
    int binarySearchById(const std::string& serviceId); // Requires sorted dataset
    void sortServicesByOccupancy();                     // Merge Sort implementation

    // Recommendation Engine
    std::vector<TransportService> getAlternativeRecommendations(const std::string& destination, const std::string& excludeServiceId) const;
};

#endif // CROWD_ESTIMATOR_H
