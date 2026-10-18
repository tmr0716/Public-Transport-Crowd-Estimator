#include "crowd_estimator.h"
#include <algorithm>
#include <iostream>

CrowdEstimatorBackend::CrowdEstimatorBackend() {
    loadSampleData();
}

// Populate sample simulated transit dataset (15-20 records)
void CrowdEstimatorBackend::loadSampleData() {
    services = {
        {"M-101", "Metro", "Blue Line Express", "Central Station", 180, 220, "08:00"},
        {"M-102", "Metro", "Blue Line Express", "Central Station", 140, 220, "08:15"},
        {"M-103", "Metro", "Blue Line Express", "Central Station", 90, 220, "08:30"},
        {"B-201", "Bus", "Ring Road Shuttle", "Tech Park", 42, 50, "08:00"},
        {"B-202", "Bus", "Ring Road Shuttle", "Tech Park", 25, 50, "08:30"},
        {"B-203", "Bus", "University Link", "North Campus", 48, 45, "08:00"}, // Overcrowded
        {"L-301", "Local", "Suburban Fast", "Downtown Terminal", 950, 850, "08:00"}, // Overcrowded
        {"L-302", "Local", "Suburban Fast", "Downtown Terminal", 720, 850, "08:30"},
        {"M-104", "Metro", "Green Line Metro", "West End", 110, 200, "09:00"},
        {"M-105", "Metro", "Green Line Metro", "West End", 75, 200, "09:15"},
        {"B-204", "Bus", "City Circular", "Market Square", 30, 50, "12:00"},
        {"B-205", "Bus", "City Circular", "Market Square", 18, 50, "12:30"},
        {"L-303", "Local", "Night Commuter", "Central Station", 300, 800, "21:00"},
        {"M-106", "Metro", "Red Line Rapid", "Airport", 160, 210, "17:30"},
        {"M-107", "Metro", "Red Line Rapid", "Airport", 130, 210, "18:00"},
        {"B-206", "Bus", "Suburban Feeder", "Railway Station", 38, 50, "17:30"}
    };
}

void CrowdEstimatorBackend::addService(const TransportService& service) {
    services.push_back(service);
}

const std::vector<TransportService>& CrowdEstimatorBackend::getAllServices() const {
    return services;
}

// 1. Linear Search: Search unsorted dataset by route name
int CrowdEstimatorBackend::linearSearchByRoute(const std::string& routeName) const {
    for (size_t i = 0; i < services.size(); ++i) {
        if (services[i].routeName == routeName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Helper for Merge Sort (Merging two sorted subarrays)
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

// 3. Sorting Algorithm: Merge Sort ordered by Occupancy Percentage
void CrowdEstimatorBackend::sortServicesByOccupancy() {
    if (!services.empty()) {
        mergeSortHelper(services, 0, services.size() - 1);
    }
}

// 2. Binary Search: Requires dataset sorted by Service ID
int CrowdEstimatorBackend::binarySearchById(const std::string& serviceId) {
    // Ensure sorted by serviceId for correct binary search semantics
    std::sort(services.begin(), services.end(), [](const TransportService& a, const TransportService& b) {
        return a.serviceId < b.serviceId;
    });

    int low = 0;
    int high = static_cast<int>(services.size()) - 1;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (services[mid].serviceId == serviceId) {
            return mid;
        }
        if (services[mid].serviceId < serviceId) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

// 4. Alternative Route Recommendation Engine
std::vector<TransportService> CrowdEstimatorBackend::getAlternativeRecommendations(
    const std::string& destination, const std::string& excludeServiceId) const {
    
    std::vector<TransportService> alternatives;
    for (const auto& service : services) {
        if (service.destination == destination && service.serviceId != excludeServiceId) {
            alternatives.push_back(service);
        }
    }

    // Sort alternatives by occupancy percentage (ascending: least crowded first)
    std::sort(alternatives.begin(), alternatives.end(), [](const TransportService& a, const TransportService& b) {
        return a.getOccupancyPercentage() < b.getOccupancyPercentage();
    });

    return alternatives;
}
