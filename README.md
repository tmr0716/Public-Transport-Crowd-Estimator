Public Transport Crowd Estimator — DAA Academic Project

Problem Statement

Urban public transit networks (buses, metros, suburban local trains) frequently suffer from severe passenger congestion during peak hours. Commuters lack real-time visibility into carriage occupancy levels, leading to overcrowded platforms and inefficient load distribution across alternative routes.

Objectives

Estimate Occupancy: Calculate passenger occupancy percentage dynamically using simulated fleet datasets.

Classify Crowd Levels: Categorize occupancy into Low (0–40%), Moderate (>40–70%), High (>70–90%), Very High (>90–100%), and Overcrowded (>100%).

Apply DAA Algorithms: Implement Linear Search, Binary Search, and Merge Sort with rigorous complexity analysis.

Recommend Alternatives: Filter and rank feasible alternative routes heading to the same destination by ascending occupancy.

Instructions to Compile and Run C++ Backend

Navigate to the backend/ directory:

cd backend


Compile using a modern C++17 compiler (e.g., g++):

g++ -std=c++17 main.cpp crowd_estimator.cpp -o crowd_estimator


Run the executable:

./crowd_estimator


Instructions to Launch Frontend

Open index.html directly in any modern web browser to view the dashboard and search interface.

Navigate between index.html, results.html, and algorithms.html to review occupancy reports and DAA complexity analyses.
