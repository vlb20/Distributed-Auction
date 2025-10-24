# Distributed Auction

<img width="200" height="200" alt="image" src="https://github.com/user-attachments/assets/703cc9af-aa4d-4f52-8278-00cf4f60db71" />


The project implements a distributed auction system based on an IoT architecture. The system utilizes a distributed algorithm to guarantee both total and causal ordering of all bids submitted by participants.

The system relies on two core mechanisms for managing event order:
1. **Total Ordering**: This is guaranteed by a special node designated as the **Sequencer**. This node is responsible for assigning a global sequence number to all auction events, ensuring that all participants process the bids in the exact same order.
2. **Causal Ordering**: This is maintained by all participants using Vector Clocks. This mechanism ensures that the cause-and-effect relationship between messages is respected, preventing messages from being processed before the messages that causally precede them.

## System Architecture

<img width="1405" height="710" alt="image" src="https://github.com/user-attachments/assets/fa236849-b913-430e-9a4a-d2e3638aa8c4" />

The architecture consists of 5 processes (nodes), implemented on ESP-32 microcontrollers.
- **1x Sequencer**: This node acts as both the auction director (handling `Start Auction`, `Check End Auction`, and `Ensure Total Ordering`) and as an active participant, capable of placing its own bids.
- **4x Participants**: These nodes can submit bids (`Send Bid`) and must maintain causal consistency (`Maintain Causality`).

The system model assumes a closed group of $N$ processes ($N≤5$) , reliable communication channels , and that processes do not fail.

## Hardware Components
The prototype was built using the following hardware:
- 5x ESP-32 Microcontrollers
- 5x LCD Displays
- 6x Push-buttons
- Breadboards, Connectors, and 5KΩ Resistors

## Algorithm and Logic
The system state is managed by global variables on each node, including `vectorClock`, `sequenceNumber`, `highestBid`, and `auctionIsStarted`.

### Message Structure

Communication relies on a `Message` struct with the following key fields:
- **`type`**: (String) `start`, `end`, `bid`, or `order`.
- **`bidValue`**: (Integer) The value of the offer.
- **`vectorClock`**: (Array) The sender's logical clock at the time of sending.
- **`sequenceNumber`**: (Integer) The total order number (only valid for `order` messages).
- **`senderId`** & **`messageId`**: (Integer) Create a unique ID for the message.

### Participant Logic (On Message Reception)

To guarantee both causal and total ordering, participants use two queues:

1. **`bid` Message Received**:
   - The participant checks the message's `vectorClock` for causal validity.
   - If the message is not causally ready (a preceding message is missing), it is placed in the `HoldBackQueue`.
   - Once its causal dependencies are met, it is moved from the `HoldBackQueue` to the `CausalQueue`, where it waits for the total ordering message.
2. **`order` Message Received**:
   - The participant receives the `order` message from the Sequencer, which contains a `sequenceNumber`.
   - If the corresponding `bid` message is already in the `CausalQueue`, the `bid` is "delivered" (i.e., processed as the new `highestBid`) in the correct total order (TO-Deliver).
   - If the `bid` has not arrived yet (or is still in the `HoldBackQueue`), the `order` message is held in an `OrderQueue`.

### Sequencer Logic (On Message Reception)

The Sequencer's logic is different, as it generates the total order:
1. **`bid` Message Received**:
   - The Sequencer, acting as a participant, also validates the `bid` message's causal order, using its own `HoldBackQueue`.
   - As soon as a `bid` is causally valid, the Sequencer immediately assigns it the next `sequenceNumber`, delivers it, and broadcasts the `order` message to all other participants (TO-Send).
2. **`order`, `start`, `end` Messages Received**:
   - The Sequencer ignores these messages, as it is the original sender.

https://github.com/user-attachments/assets/934cc729-7413-42d1-b418-cbad19b5117e


### ⚠️ Identified Issues and Limitations
This design has several known limitations:
- **Single Point of Failure (SPoF):** The Sequencer is a SPoF. If the Sequencer node fails, the auction halts, as no new bids can be totally ordered.
- **Bottleneck**: The Sequencer must process every single `bid` to assign a sequence number. This can become a performance bottleneck as the number of participants or the frequency of bids increases.
- **Network Assumptions**: The model assumes reliable channels. **If messages are lost** (especially `order` messages from the Sequencer), it can lead to inconsistencies where some nodes get stuck. **Duplicate messages** could also cause issues like duplicate bids or inconsistent message IDs.


