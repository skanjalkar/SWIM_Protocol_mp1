# SWIM_Protocol
Implementation of SWIM member protocol in Cloud Computing

# SWIM Protocol

The SWIM (Scalable Weakly-consistent Infection-style Process Group Membership) protocol is a distributed system protocol designed to manage membership and detect failures in large-scale systems. It provides a scalable and fault-tolerant approach to maintain an up-to-date view of active members in a distributed network.

## Overview

The SWIM protocol employs a gossip-based communication model to disseminate information about the members' statuses throughout the system. Each process in the system maintains a local membership list, and periodically, it gossips its own status to a randomly selected set of other processes. The gossip messages contain information about the sender's status, such as availability or failure.
Failure Detection

SWIM combines direct and indirect pings to detect failures in the system. When a process wants to check the status of another process, it can either send a direct ping to that process or ask another process to ping on its behalf (indirect ping). This approach helps distribute the workload of failure detection across the system and enhances scalability.

To handle network delays and potential false positives, the SWIM protocol utilizes various mechanisms such as timeouts and failure suspicion counters. If a process doesn't receive a timely response to a ping (direct or indirect), it assumes that the target process has failed.

## Benefits

-    **Scalability**: The SWIM protocol is designed to handle large-scale distributed systems with a high number of processes. By leveraging gossip-based communication and random sampling, it achieves scalability while minimizing network overhead.

-    **Fault Tolerance**: SWIM provides fault-tolerant membership management by allowing processes to detect failures and maintain an up-to-date view of the active members. It handles network delays, message losses, and false positives through mechanisms such as timeouts and failure suspicion counters.

-    **Flexibility**: The SWIM protocol allows for configuration parameters to be tuned based on the specific requirements of the distributed system, such as the frequency of gossiping, timeout values, and indirect ping mechanisms.

Usage

To use the SWIM protocol in your distributed system, follow these steps:

1. Implement the gossiping mechanism to exchange membership information between processes.
2. Periodically select a random set of processes and gossip the sender's status to them.
3. Handle incoming gossip messages and update the local membership list accordingly.
4. Implement failure detection mechanisms using direct and indirect pings, and handle timeouts and failure suspicion counters.
5. Use the updated membership list to make informed decisions based on the current state of the distributed system.

## Implementation:

The P2P layer in order to complete the protocol has been implemented in ```MP1Node.cpp``` file. In every iteration of ```MP1Node::nodeLoop()``` function, the member checks for new incoming messages, performs actions related to those messages and also sends out its own membership list message to k random other nodes.

Running the code:

In order to run the code, first compile it using

```make```

Then, to execute the program, from the program directory run:

```./Application testcases/<test_name>.conf```

You can verify if the protocol is working as intended by checking dbg.log file.