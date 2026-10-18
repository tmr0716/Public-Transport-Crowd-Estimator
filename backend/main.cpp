
#include "crowd_estimator.h"
#include "httplib.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

std::string escapeHTML(const std::string& input) {
    std::string out;
    for (char c : input) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

std::string page(const std::string& title, const std::string& body) {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>)HTML" + escapeHTML(title) + R"HTML(</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header><div class="nav-container">
<div class="logo-area">
<h1>TransitCrowd DAA</h1>
<p>Public Transport Occupancy Estimator & Alternative Route Finder</p>
</div>
<nav><ul>
<li><a href="/">Dashboard</a></li>
<li><a href="/results">Live Status</a></li>
<li><a href="/algorithms.html">DAA Algorithms</a></li>
</ul></nav>
</div></header>
<div class="container">
)HTML" + body + R"HTML(
</div>
<footer><p>Public Transport Crowd Estimator | DAA Academic Project</p></footer>
</body></html>)HTML";
}

std::string errorPage(const std::string& message) {
    return page("Request Error",
        "<section class='search-section'><h2>Unable to estimate crowd</h2><p>" +
        escapeHTML(message) +
        "</p><p><a href='/'>Return to dashboard</a></p></section>");
}

int main() {
    CrowdEstimatorBackend estimator;
    httplib::Server server;

    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::string html = readFile("index.html");
        if (html.empty()) {
            res.status = 500;
            res.set_content("Could not load index.html", "text/plain");
            return;
        }
        res.set_content(html, "text/html; charset=UTF-8");
    });

    server.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        std::string css = readFile("style.css");
        if (css.empty()) {
            res.status = 404;
            res.set_content("CSS file not found", "text/plain");
            return;
        }
        res.set_content(css, "text/css; charset=UTF-8");
    });

    server.Get("/algorithms.html",
        [](const httplib::Request&, httplib::Response& res) {
            std::string html = readFile("algorithms.html");
            if (html.empty()) {
                res.status = 404;
                res.set_content("Algorithms page not found", "text/plain");
                return;
            }
            res.set_content(html, "text/html; charset=UTF-8");
        });

    server.Get("/results",
        [&estimator](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_param("route_id") ||
                !req.has_param("destination")) {
                res.status = 400;
                res.set_content(
                    errorPage("Please enter a service ID and destination."),
                    "text/html; charset=UTF-8");
                return;
            }

            const std::string id = req.get_param_value("route_id");
            const std::string destination =
                req.get_param_value("destination");
            const std::string type =
                req.has_param("transport_type")
                ? req.get_param_value("transport_type") : "";

            // Binary Search sorts the service list by service ID.
            int index = estimator.binarySearchById(id);

            if (index == -1) {
                res.status = 404;
                res.set_content(
                    errorPage("Service ID '" + id +
                              "' was not found in the sample dataset."),
                    "text/html; charset=UTF-8");
                return;
            }

            const auto& services = estimator.getAllServices();
            const TransportService selected = services[index];

            if (!type.empty() && selected.transportType != type) {
                res.status = 400;
                res.set_content(
                    errorPage("The selected transport type does not match "
                              "this service ID."),
                    "text/html; charset=UTF-8");
                return;
            }

            const double occupancy = selected.getOccupancyPercentage();
            const double barWidth = occupancy > 100.0 ? 100.0 : occupancy;

            std::string color = "#16a34a";
            if (occupancy > 100.0) color = "#9333ea";
            else if (occupancy > 90.0) color = "#ea580c";
            else if (occupancy > 70.0) color = "#ca8a04";

            std::ostringstream body;
            body << std::fixed << std::setprecision(1);

            body << "<section class='search-section'>"
                 << "<h2>Selected Service Analysis</h2>"
                 << "<p><strong>Service ID:</strong> "
                 << escapeHTML(selected.serviceId) << "</p>"
                 << "<p><strong>Transport:</strong> "
                 << escapeHTML(selected.transportType) << "</p>"
                 << "<p><strong>Route:</strong> "
                 << escapeHTML(selected.routeName) << "</p>"
                 << "<p><strong>Destination in dataset:</strong> "
                 << escapeHTML(selected.destination) << "</p>"
                 << "<p><strong>Departure:</strong> "
                 << escapeHTML(selected.timeSlot) << "</p>"
                 << "<p><strong>Passengers / Capacity:</strong> "
                 << selected.passengerCount << " / "
                 << selected.passengerCapacity << "</p>"
                 << "<p><strong>Occupancy:</strong> "
                 << occupancy << "%</p>"
                 << "<p><strong>Crowd status:</strong> "
                 << escapeHTML(selected.getCrowdClassification())
                 << "</p>"
                 << "<div class='progress-bar-bg' style='height:20px'>"
                 << "<div class='progress-bar-fill' style='width:"
                 << barWidth << "%;background:" << color
                 << ";height:100%'></div></div>"
                 << "<p><small>Simulated dataset; not live passenger data.</small></p>"
                 << "</section>";

            // Recommend services for the destination entered in the form.
            auto alternatives = estimator.getAlternativeRecommendations(
                destination, selected.serviceId);

            body << "<h2>Alternative Services — Least Crowded First</h2>";

            if (alternatives.empty()) {
                body << "<p>No alternative services were found for destination '"
                     << escapeHTML(destination) << "'. "
                     << "Try the exact destination used in the sample dataset.</p>";
            } else {
                body << "<table><thead><tr>"
                     << "<th>Rank</th><th>Service ID</th><th>Transport</th>"
                     << "<th>Route</th><th>Departure</th>"
                     << "<th>Occupancy</th><th>Status</th>"
                     << "</tr></thead><tbody>";

                for (size_t i = 0; i < alternatives.size(); ++i) {
                    const auto& s = alternatives[i];
                    body << "<tr><td>" << (i + 1) << "</td><td>"
                         << escapeHTML(s.serviceId) << "</td><td>"
                         << escapeHTML(s.transportType) << "</td><td>"
                         << escapeHTML(s.routeName) << "</td><td>"
                         << escapeHTML(s.timeSlot) << "</td><td>"
                         << s.getOccupancyPercentage() << "%</td><td>"
                         << escapeHTML(s.getCrowdClassification())
                         << "</td></tr>";
                }
                body << "</tbody></table>";
            }

            body << "<p><a href='/'>← Back to dashboard</a></p>"
                 << "<p><small>Binary Search: O(log n) after sorting IDs. "
                 << "Alternative recommendations are ordered by occupancy.</small></p>";

            res.set_content(
                page("Estimation Results", body.str()),
                "text/html; charset=UTF-8");
        });

    const char* portEnv = std::getenv("PORT");
    const int port = portEnv ? std::atoi(portEnv) : 10000;

    if (port <= 0) return 1;

    return server.listen("0.0.0.0", port) ? 0 : 1;
}
