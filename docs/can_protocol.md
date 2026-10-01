# 📡 Controller Area Network (CAN Bus)

## 📌 What is CAN?

The **Controller Area Network (CAN)** is a **message-based serial communication protocol** designed to let **Electronic Control Units (ECUs)** communicate with:

- Reliability
- Deterministic behavior
- Message priorities
- No need for a central host computer

CAN is widely used in:

- Cars
- Heavy vehicles
- Industrial systems
- Aviation
- Robotics

The protocol is standardized primarily by **ISO 11898**.

---

## 🧠 Key concept: message-based communication

In CAN:

- ❌ Messages do not contain source or destination addresses
- ✅ Every message is **broadcast**
- ✅ All ECUs on the bus can receive the messages
- ✅ Each ECU decides whether a message is relevant

A message's **priority** and **meaning** are associated with its **identifier**.

---

## ⚡ Main CAN features

- **Multi-master** communication: any node can transmit
- Priority-based arbitration without destructive collisions
- Hardware error detection and handling
- Deterministic, real-time communication
- High resistance to electrical noise

---

## 🔌 CAN layers

CAN primarily defines:

- The **physical layer**: electrical signals
- The **data link layer**: frames, arbitration, CRC, and ACK

Higher-layer behavior, such as interpreting application data, is defined by protocols and conventions including:

- CANopen
- J1939
- ISO-TP
- UDS
- Proprietary automotive protocols

---

## 🧩 CAN frame structure (standard, 11-bit identifier)

A CAN frame consists of several fields transmitted **bit by bit**.

```text
SOF | Identifier | RTR | IDE | R0 | DLC | Data | CRC | ACK | EOF | IFS
```

### 🟢 SOF — Start of Frame (1 bit)

- Always **dominant (0)**
- Marks the beginning of a new message
- Synchronizes the nodes on the network
- Occurs after the interframe spacing period

### 🆔 Identifier (11 bits)

- Identifies the **message type**
- Determines its **priority**
- A **lower value** means **higher priority**

For example:

- ID `0x100` → higher priority
- ID `0x300` → lower priority

The identifier is also used during **bus arbitration**.

### 🔄 RTR — Remote Transmission Request (1 bit)

- **Dominant (0)** → data frame
- **Recessive (1)** → remote request

A remote request allows one node to request data and another to respond with a data frame using the same identifier.

Remote frames are rarely used in modern systems.

### 🧩 IDE — Identifier Extension (1 bit)

- **Dominant (0)** → standard CAN (11-bit identifier)
- **Recessive (1)** → extended CAN (29-bit identifier)

This guide covers **standard CAN**.

### 🧪 R0 — Reserved (1 bit)

- Reserved for future use
- Transmitted as **dominant (0)**

### 📏 DLC — Data Length Code (4 bits)

- Indicates the number of bytes in the data field
- Classic CAN supports payloads of `0` to `8` bytes

For example, `DLC = 8` indicates eight data bytes.

In CAN FD, the DLC does not always equal the payload size in bytes.

### 📦 Data field (0 to 8 bytes)

- Contains application data
- Its layout is defined by the application or a higher-layer protocol
- Values are interpreted using masks, bit shifts, scaling, and offsets

Example payload layout:

- Bytes 0–1 → RPM
- Byte 2 → speed
- Byte 3 → flags

### 🔐 CRC — Cyclic Redundancy Check (16 bits including delimiter)

- Used for **error detection**
- Calculated from the preceding frame content
- Contains 15 CRC bits and one recessive delimiter bit

If the CRC does not match:

- The frame is rejected
- An error is signaled on the bus

### 🤝 ACK — Acknowledgement (2 bits)

Contains:

- **ACK slot**: 1 bit
- **ACK delimiter**: 1 bit

During the ACK slot:

- The transmitter sends a recessive bit
- A node that received the frame correctly asserts a dominant bit
- If no node acknowledges the frame, the transmitter detects an error

An acknowledgement confirms that **at least one other node received the frame correctly**.

### 🛑 EOF — End of Frame (7 bits)

- All seven bits are recessive (`1`)
- Marks the end of the CAN frame
- Completes the frame transmission

### ⏳ IFS — Interframe Space (at least 3 bits)

- Separates frames
- Gives the controller time to process the received frame and update buffers
- Includes **at least three consecutive recessive bits**

After the required spacing, a new dominant bit can mark the next SOF.

---

## ⚖️ CAN arbitration

When two nodes begin transmitting at the same time:

- Arbitration takes place **bit by bit**
- `0` (dominant) overrides `1` (recessive)
- A node that transmits `1` but reads `0` **loses arbitration**
- The losing node **stops transmitting immediately**, without treating the loss as an error

The message with the **lower identifier wins**, and arbitration does not corrupt the winning frame.

---

## 🧠 Key points

- CAN uses **broadcast** communication
- Identifiers determine **priority** and identify **message meaning**
- Byte order is an application-level convention for the **data field**
- Arbitration is **non-destructive**
- Hardware provides the underlying communication robustness

---

## 🚗 Working with ECUs and firmware

In practice, firmware:

- Receives bytes
- Applies masks and bit shifts
- Converts byte order as required
- Updates the ECU's internal state
- Generates new frames

These concepts provide a foundation for understanding automotive firmware.
