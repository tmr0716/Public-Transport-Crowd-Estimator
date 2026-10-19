# Public Transport Crowd Estimator — DAA Project

A web-based public transport occupancy estimator and router built with **HTML5**, **CSS3**, and a high-performance **C++17** backend serving dynamic HTTP requests.

## DAA Algorithms Implemented
1. **Linear Search**: $O(N)$ search through unsorted route records.
2. **Binary Search**: $O(\log N)$ lookup by unique Service ID on pre-sorted data.
3. **Merge Sort**: $O(N \log N)$ sorting algorithm used to rank transit services by occupancy percentage.
4. **Dijkstra's Algorithm**: Shortest travel-time path calculation across a weighted graph of transit stops ($O((V+E)\log V)$).
5. **Dynamic Programming**: Bounded-leg minimum cost path optimization ($O(K \cdot (V+E))$).

## Local Development & Compilation
```bash
g++ -std=c++17 -O3 backend/main.cpp backend/crowd_estimator.cpp -o transport_server -lpthread
./transport_server
