#include "httplib.h"
#include "crowd_estimator.h"
#include <iostream>
#include <fstream>
#include <sstream>

std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    CrowdEstimatorBackend backend;

    httplib::Server svr;

    // Serve static CSS
    svr.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        std::string css = readFile("style.css");
        res.set_content(css, "text/css");
    });

    // Serve Home / Dashboard
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::string html = readFile("index.html");
        res.set_content(html, "text/html");
    });

    // Serve Algorithms Page
    svr.Get("/algorithms", [](const httplib::Request&, httplib::Response& res) {
        std::string html = readFile("algorithms.html");
        res.set_content(html, "text/html");
    });

    // Handle Results Route (Crowd Estimation & Searching / Sorting / Alternatives)
    svr.Get("/results", [&backend](const httplib::Request& req, httplib::Response& res) {
        std::string serviceId = req.get_param_value("serviceId");
        std::string routeQuery = req.get_param_value("route");

        TransportService* selectedService = nullptr;
        if (!serviceId.empty()) {
            // Demonstrate Binary Search
            selectedService = backend.binarySearchById(serviceId);
        }

        std::vector<TransportService> matchingServices;
        if (!routeQuery.empty()) {
            // Demonstrate Linear Search
            matchingServices = backend.linearSearchByRoute(routeQuery);
        }

        if (!selectedService && !matchingServices.empty()) {
            selectedService = &matchingServices[0];
        } else if (!selectedService) {
            // Default to first service if nothing matched
            auto all = backend.getServices();
            if (!all.empty()) selectedService = &const_cast<std::vector<TransportService>&>(all)[0];
        }

        // Generate dynamic HTML for results
        std::string templateHtml = readFile("results.html");
        
        std::string dynamicContent = "";
        if (selectedService) {
            double occ = selectedService->getOccupancyPercentage();
            std::string badge = selectedService->getBadgeClass();
            std::string classification = selectedService->getCrowdClassification();

            dynamicContent += "<div class='card mb-4'>";
            dynamicContent += "<h2>Selected Service: " + CrowdEstimatorBackend::escapeHtml(selectedService->serviceId) + " (" + CrowdEstimatorBackend::escapeHtml(selectedService->transportType) + ")</h2>";
            dynamicContent += "<p><strong>Route:</strong> " + CrowdEstimatorBackend::escapeHtml(selectedService->routeName) + "</p>";
            dynamicContent += "<p><strong>Route:</strong> " + CrowdEstimatorBackend::escapeHtml(selectedService->source) + " &rarr; " + CrowdEstimatorBackend::escapeHtml(selectedService->destination) + "</p>";
            dynamicContent += "<p><strong>Time Slot:</strong> " + CrowdEstimatorBackend::escapeHtml(selectedService->timeSlot) + "</p>";
            dynamicContent += "<p><strong>Occupancy:</strong> " + std::to_string(selectedService->passengerCount) + " / " + std::to_string(selectedService->passengerCapacity) + " (" + std::to_string((int)occ) + "%)</p>";
            dynamicContent += "<p><strong>Crowd Status:</strong> <span class='badge " + badge + "'>" + classification + "</span></p>";
            
            // Visual occupancy bar
            dynamicContent += "<div class='progress-bar-container'><div class='progress-bar-fill' style='width: " + std::to_string(occ > 100 ? 100 : occ) + "%;'></div></div>";
            dynamicContent += "</div>";

            // Alternative routes using Merge Sort ranking
            auto alts = backend.getAlternativeServices(selectedService->destination, selectedService->serviceId);
            dynamicContent += "<div class='card'><h3>Least Crowded Alternative Services (Merge Sort Ranked)</h3>";
            if (alts.empty()) {
                dynamicContent += "<p>No alternative routes available for this destination.</p>";
            } else {
                dynamicContent += "<table class='table'><tr><th>ID</th><th>Type</th><th>Route</th><th>Occupancy</th><th>Status</th></tr>";
                for (const auto& alt : alts) {
                    dynamicContent += "<tr><td>" + CrowdEstimatorBackend::escapeHtml(alt.serviceId) + "</td>";
                    dynamicContent += "<td>" + CrowdEstimatorBackend::escapeHtml(alt.transportType) + "</td>";
                    dynamicContent += "<td>" + CrowdEstimatorBackend::escapeHtml(alt.routeName) + "</td>";
                    dynamicContent += "<td>" + std::to_string((int)alt.getOccupancyPercentage()) + "%</td>";
                    dynamicContent += "<td><span class='badge " + alt.getBadgeClass() + "'>" + alt.getCrowdClassification() + "</span></td></tr>";
                }
                dynamicContent += "</table>";
            }
            dynamicContent += "</div>";
        } else {
            dynamicContent += "<div class='card'><p>No transport service found matching your query.</p></div>";
        }

        // Simple template replacement
        size_t pos = templateHtml.find("<!-- DYNAMIC_RESULTS_PLACEHOLDER -->");
        if (pos != std::string::npos) {
            templateHtml.replace(pos, 38, dynamicContent);
        }

        res.set_content(templateHtml, "text/html");
    });

    // Handle Dijkstra Shortest Path Route
    svr.Get("/dijkstra", [&backend](const httplib::Request& req, httplib::Response& res) {
        std::string start = req.get_param_value("start");
        std::string end = req.get_param_value("end");

        if (start.empty()) start = "Central Station";
        if (end.empty()) end = "Tech Park";

        auto result = backend.findShortestPath(start, end);

        std::string html = "<html><head><title>Dijkstra Result</title><link rel='stylesheet' href='/style.css'></head><body>";
        html += "<div class='container'><h1>Graph Shortest Path (Dijkstra's Algorithm)</h1>";
        html += "<div class='card'>";
        html += "<p><strong>From:</strong> " + CrowdEstimatorBackend::escapeHtml(start) + " &rarr; <strong>To:</strong> " + CrowdEstimatorBackend::escapeHtml(end) + "</p>";
        if (result.found) {
            html += "<p><strong>Total Estimated Travel Time:</strong> " + std::to_string(result.totalTime) + " minutes</p>";
            html += "<p><strong>Stops Visited:</strong> ";
            for (size_t i = 0; i < result.path.size(); ++i) {
                html += CrowdEstimatorBackend::escapeHtml(result.path[i]);
                if (i + 1 < result.path.size()) html += " &rarr; ";
            }
            html += "</p>";
        } else {
            html += "<p>No path found between these stops.</p>";
        }
        html += "<br><a href='/' class='btn'>Back to Dashboard</a></div></div></body></html>";

        res.set_content(html, "text/html");
    });

    // Handle Dynamic Programming Route
    svr.Get("/dp", [&backend](const httplib::Request& req, httplib::Response& res) {
        std::string start = req.get_param_value("start");
        std::string end = req.get_param_value("end");
        int maxLegs = 3;
        try {
            std::string legsStr = req.get_param_value("legs");
            if (!legsStr.empty()) maxLegs = std::stoi(legsStr);
        } catch (...) {}

        if (start.empty()) start = "Central Station";
        if (end.empty()) end = "Tech Park";

        auto dpRes = backend.findMinimumCostPathDP(start, end, maxLegs);

        std::string html = "<html><head><title>DP Result</title><link rel='stylesheet' href='/style.css'></head><body>";
        html += "<div class='container'><h1>Dynamic Programming Minimum Cost Route</h1>";
        html += "<div class='card'>";
        html += "<p><strong>From:</strong> " + CrowdEstimatorBackend::escapeHtml(start) + " &rarr; <strong>To:</strong> " + CrowdEstimatorBackend::escapeHtml(end) + "</p>";
        html += "<p><strong>Max Allowed Connections / Legs:</strong> " + std::to_string(maxLegs) + "</p>";
        if (dpRes.reachable) {
            html += "<p><strong>Minimum Travel Cost:</strong> ₹" + std::to_string(dpRes.minCost) + "</p>";
            html += "<p><strong>Optimal Route Path:</strong> ";
            for (size_t i = 0; i < dpRes.path.size(); ++i) {
                html += CrowdEstimatorBackend::escapeHtml(dpRes.path[i]);
                if (i + 1 < dpRes.path.size()) html += " &rarr; ";
            }
            html += "</p>";
        } else {
            html += "<p>No feasible route found within " + std::to_string(maxLegs) + " legs.</p>";
        }
        html += "<br><a href='/' class='btn'>Back to Dashboard</a></div></div></body></html>";

        res.set_content(html, "text/html");
    });

    // Determine port from environment or fallback
    int port = 10000;
    const char* envPort = std::getenv("PORT");
    if (envPort) {
        port = std::stoi(envPort);
    }

    std::cout << "Public Transport Crowd Estimator C++ Server starting on 0.0.0.0:" << port << "...\n";
    svr.listen("0.0.0.0", port);

    return 0;
}
