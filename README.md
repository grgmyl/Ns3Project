Network Simulation with NS-3
Project Overview

This project focuses on network simulation using the NS-3 discrete-event network simulator. It is divided into two main parts, each designed to explore different aspects of network behavior, protocol performance, and dynamic routing under failure conditions.

    Part 1: Simulates a simple point-to-point connection between two nodes using UDP Echo applications
    to analyze basic traffic flow metrics.

    Part 2: Constructs a more complex multi-node network topology to investigate dynamic routing, 
    packet flow management, and the impact of link failures on packet loss and network performance.

Setup and Execution

1)Move up two directories
cd ../..

2)Copy the first example script
cp examples/tutorial/mypoint.cc scratch/mypoint

3)Build your example
./ns3 build

4)Run the 1st query with parameters
./ns3 run scratch/five-node-topology -- --failDownAt=6.0 --failUpAt=8.0
