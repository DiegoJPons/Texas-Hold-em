# Technical Projects: Systems Engineering & Low-Level Design

### **Texas Hold 'em Concurrent Server**
[cite_start]**The Challenge:** Developing a robust, multi-client server that strictly adheres to a complex communication protocol while managing the synchronized state of a six-player Texas Hold 'em game[cite: 53]. [cite_start]This project required bridging the gap between low-level network programming and high-level game logic to ensure consistent data across a distributed environment[cite: 54, 55].

**Technical Architecture & "The Craft":**
* **Custom Communication Protocol:** Implemented a precise packet-exchange system using `JOIN`, `READY`, `INFO`, `ACK/NACK`, and `END` packet types[cite: 115, 116, 151].
* **Non-Blocking Logic:** Engineered the server to avoid blocking reads, ensuring it only communicates with clients at specific intervals to maintain responsiveness across all 6 player ports[cite: 168, 171].
* **State Machine Management:** Architected a centralized game state that transitioned through five distinct phases: Dealing, Flop, Turn, River, and Showdown[cite: 175, 185, 195, 205, 215].
* **Dynamic Player Tracking:** Developed logic to track player stacks, current bets, and active statuses (Folded, Active, or Left) to determine valid moves at each turn[cite: 122, 123, 128, 129].
* **Advanced Hand Evaluation:** Developed a complex comparison engine to evaluate the best 5-card hand from 7 available cards[cite: 77, 78].
* **Tie-Breaking Implementation:** Manually implemented nine hand rankings—from High Card to Straight-Flush—including specific tie-breaking rules for pairs, flushes, and straights[cite: 79, 80, 81, 82, 83].
* **Distributed Resilience:** Engineered the server to handle "Player Left" scenarios gracefully, reassigning the dealer and turn order to ensure the game continues for remaining players[cite: 103, 219].
* **Systems Rigor:** Utilized **GDB** for debugging state transitions and **Valgrind** to ensure memory stability throughout the server lifecycle[cite: 11, 39].

> **Technical Decision Highlight:** "One of the most critical decisions was how to handle the 'Showdown' phase. Instead of just identifying a winner, I built a modular evaluation system that could be reused to calculate hand strength in real-time. By separating the hand-ranking logic (using pointer arithmetic and custom C structs) from the networking code, I was able to validate the win-determination algorithms independently before deploying them to the socket-based environment."

---
