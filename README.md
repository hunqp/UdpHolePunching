# Kết Nối P2P qua UDP Hole Punching

## Summary

**UDP Hole Punching** is a technique used to establish a direct peer-to-peer (P2P) connection between devices behind a NAT (Network Address Translation). This method allows peers to communicate directly with each other without needing to go through any intermediary servers.

## Server Usage

- **Signaling Server**: `broker.emqx.io:1883`
- **STUN Server**: `stun.l.google.com:19302`

## Description

### 1. **Signaling qua MQTT**
   - MQTT is used for the signaling mechanism to exchange **Candidates** between devices..

### 2. **STUN Protocol**   
   - Obtains the **PUBLIC_IP** and opens the **PUBLIC_PORT** by sending a query to the **STUN SERVER**. The router temporarily holds this **PUBLIC_PORT** for a short time.
   - Using **STUN** is crucial for UDP Hole Punching, a device behind NAT will map its **LOCAL_PORT** to this **PUBLIC_PORT** to send messages to another device behind NAT.

### 3. **Gathering Candidate**
   - The connection setup process depends on:
     - **Case 1**: Both devices are behind the same NAT and on the same machine.
     - **Case 2**: Both devices are behind the same NAT but on different machines in the same local network.
     - **Case 3**: Both devices are behind different NATs, requiring each device to **map** its **LOCAL_PORT** to a **PUBLIC_PORT**.
     
### 4. **UDP Hole Punching**
   - Devices will start sending/receiving messages based on the **Candidates** received from the other side.

## How to use ?

To run the application, simply execute the following steps:
- Install dependency
  ```
  sudo apt install libmosquitto-dev
  ```
 - Build
   ```bash
   make
   ```
The program, once built, includes two applications **Peer1** and **Peer2**. You can test the connection with 3 different cases:
- Run both peers on the same Linux machine using two terminals..
- Run the peers on two Linux machines connected to the same network.
- Run the peers on two Linux machines located on different Internet service providers.

## Reference
 - [P2P-NAT](https://bford.info/pub/net/p2pnat)
