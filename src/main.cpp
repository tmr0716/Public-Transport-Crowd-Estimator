#include <iostream>
#include "crowd_engine.h"

void printUsage() {
    std::cout << "Usage:\n";
    std::cout << "  crowd_engine stats <csv_path>\n";
    std::cout << "  crowd_engine list <csv_path> [asc|desc]\n";
    std::cout << "  crowd_engine search <csv_path> <stop_id>\n";
    std::cout << "  crowd_engine route <csv_path> <src_id> <dest_id>\n";
    std::cout << "  crowd_engine dp <csv_path> <stop_id>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    std::string csv_path = argv[2];

    CrowdEngine engine;
    if (!engine.loadData(csv_path)) {
        std::cerr << "Error: Could not load dataset from " << csv_path << "\n";
        return 1;
    }

    if (command == "stats") {
        auto stats = engine.getStatistics();
        std::cout << "TOTAL_STOPS:" << stats.total_stops << "\n";
        std::cout << "HIGH_CROWD:" << stats.high_crowd_count << "\n";
        std::cout << "AVG_CROWD:" << stats.average_crowd << "\n";
        std::cout << "LEAST_CROWDED:" << stats.least_crowded_name << "\n";
    }
    else if (command == "list") {
        bool asc = true;
        if (argc > 3 && std::string(argv[3]) == "desc") asc = false;
        auto list = engine.getStops();
        engine.sortStopsByMergeSort(list, asc);
        for (const auto& s : list) {
            std::cout << s.id << "|" << s.name << "|" << s.current_count << "|" << s.crowd_level << "\n";
        }
    }
    else if (command == "search") {
        if (argc < 4) {
            std::cout << "Error: Missing stop ID for search.\n";
            return 1;
        }
        std::string target_id = argv[3];
        auto list = engine.getStops();
        // Sort by ID for binary search
        std::sort(list.begin(), list.end(), [](const StopNode& a, const StopNode& b) {
            return a.id < b.id;
        });
        int idx = engine.binarySearchStopById(list, target_id);
        if (idx != -1) {
            const auto& s = list[idx];
            std::cout << "FOUND:ID=" << s.id << "|NAME=" << s.name << "|COUNT=" << s.current_count << "|LEVEL=" << s.crowd_level << "\n";
        } else {
            std::cout << "NOT_FOUND\n";
        }
    }
    else if (command == "route") {
        if (argc < 5) {
            std::cout << "Error: Missing source or destination ID.\n";
            return 1;
        }
        std::string src = argv[3];
        std::string dest = argv[4];

        auto r1 = engine.findShortestDistanceRoute(src, dest);
        auto r2 = engine.findLowestCostRoute(src, dest, 1.0, 1.5);

        std::cout << "--- SHORTEST ROUTE ---\n";
        if (r1.found) {
            std::cout << "DIST:" << r1.total_distance << "\nPATH:";
            for (size_t i = 0; i < r1.path_names.size(); ++i) {
                std::cout << r1.path_names[i] << (i + 1 < r1.path_names.size() ? " -> " : "");
            }
            std::cout << "\n";
        } else {
            std::cout << "ROUTE_NOT_FOUND\n";
        }

        std::cout << "--- LOWEST COST ROUTE ---\n";
        if (r2.found) {
            std::cout << "COST:" << r2.total_cost << "|DIST:" << r2.total_distance << "\nPATH:";
            for (size_t i = 0; i < r2.path_names.size(); ++i) {
                std::cout << r2.path_names[i] << (i + 1 < r2.path_names.size() ? " -> " : "");
            }
            std::cout << "\n";
        } else {
            std::cout << "ROUTE_NOT_FOUND\n";
        }
    }
    else if (command == "dp") {
        if (argc < 4) {
            std::cout << "Error: Missing stop ID for DP prediction.\n";
            return 1;
        }
        std::string stop_id = argv[3];
        auto dp_res = engine.computeOptimalDepartureTime(stop_id, 0.7, 0.3);
        std::string slot_names[5] = {"Morning Peak (08:00)", "Midday (12:00)", "Evening Peak (17:00)", "Night (21:00)", "Late Night (23:30)"};
        
        std::cout << "OPTIMAL_SLOT_IDX:" << dp_res.optimal_slot << "\n";
        std::cout << "OPTIMAL_SLOT_NAME:" << (dp_res.optimal_slot >= 0 && dp_res.optimal_slot < 5 ? slot_names[dp_res.optimal_slot] : "Unknown") << "\n";
        std::cout << "MIN_TOTAL_COST:" << dp_res.min_total_cost << "\n";
    }
    else {
        printUsage();
    }

    return 0;
}
