#include "crowd_estimator.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=========================================================\n";
    std::cout << "    PUBLIC TRANSPORT CROWD ESTIMATOR — DAA BACKEND       \n";
    std::cout << "=========================================================\n\n";

    CrowdEstimatorBackend estimator;

    // Display initial dataset loaded
    std::cout << "[INFO] Loaded " << estimator.getAllServices().size() << " simulated transport services.\n\n";

    // Demonstrate Binary Search
    std::string searchTarget = "M-101";
    std::cout << "[DAA Test] Executing Binary Search for Service ID: " << searchTarget << "\n";
    int bIndex = estimator.binarySearchById(searchTarget);
    if (bIndex != -1) {
        const auto& s = estimator.getAllServices()[bIndex];
        std::cout << " -> Found: " << s.serviceId << " | " << s.routeName 
                  << " | Occupancy: " << std::fixed << std::setprecision(1) 
                  << s.getOccupancyPercentage() << "% (" << s.getCrowdClassification() << ")\n\n";
    } else {
        std::cout << " -> Service ID not found.\n\n";
    }

    // Demonstrate Merge Sort by Occupancy
    std::cout << "[DAA Test] Executing Merge Sort by Occupancy Percentage...\n";
    estimator.sortServicesByOccupancy();
    std::cout << " -> Top 3 Least Crowded Services across network:\n";
    int count = 0;
    for (const auto& s : estimator.getAllServices()) {
        if (count >= 3) break;
        std::cout << "    " << (count + 1) << ". [" << s.serviceId << "] " << s.routeName 
                  << " -> " << std::fixed << std::setprecision(1) << s.getOccupancyPercentage() 
                  << "% (" << s.getCrowdClassification() << ")\n";
        count++;
    }
    std::cout << "\n";

    // Demonstrate Alternative Route Recommendation
    std::string targetDest = "Central Station";
    std::cout << "[DAA Test] Finding Alternatives for Destination: " << targetDest << "\n";
    auto alternatives = estimator.getAlternativeRecommendations(targetDest, "M-101");
    for (size_t i = 0; i < alternatives.size(); ++i) {
        std::cout << "    Alt " << (i + 1) << ": [" << alternatives[i].serviceId << "] " 
                  << alternatives[i].routeName << " at " << alternatives[i].timeSlot 
                  << " -> " << std::fixed << std::setprecision(1) 
                  << alternatives[i].getOccupancyPercentage() << "% (" << alternatives[i].getCrowdClassification() << ")\n";
    }

    std::cout << "\n[SUCCESS] C++ DAA backend execution completed successfully.\n";
    return 0;
}
