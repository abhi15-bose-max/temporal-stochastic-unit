# Temporal Stochastic Unit (TSU)

This repository demonstrates a **Temporal Stochastic Unit (TSU)** — a probabilistic decision primitive in which **probability and confidence are encoded in time rather than state**.

Unlike probabilistic bits (p-bits), which sample noisy states repeatedly, a TSU commits to a decision **once**, via a stochastic timing race between competing processes. The **decision latency itself encodes confidence**, eliminating the need for repeated sampling or post-hoc probability estimation.

This implementation runs on an ESP32 and exploits **intrinsic hardware timing noise** (RTOS scheduling jitter, clock phase noise, cache effects) as a computational resource.

---

## Core Idea

A TSU consists of two or more competing temporal processes racing toward biased time thresholds:

- **Bias** controls the relative drift of the processes (evidence).
- **Noise** enters naturally through hardware timing variability.
- **Decision** is made by first arrival.
- **Confidence** is encoded in:
  - decision latency (absolute confidence)
  - arrival-time separation Δt (relative confidence)

The system does not compute probabilities explicitly — they **emerge from time-domain dynamics**.

---

## Implementation Overview

- **Platform:** ESP32 (FreeRTOS, dual-core)
- **Noise sources:**
  - RTOS scheduling jitter
  - Interrupt latency
  - Cache and pipeline effects
  - CPU cycle counter phase noise
- **Architecture:**
  - A background task continuously injects timing noise.
  - A symmetric race engine measures first-arrival times.
  - The TSU outputs `(winner, latency, delta)` per decision.

---

## Output Observables

Each TSU decision returns:

| Variable | Meaning |
|--------|--------|
| `winner` | Which path won the race |
| `latency` | Time to decision (confidence proxy) |
| `delta` | Separation between arrivals (margin of victory) |

This exposes **confidence as a physical observable**, not an inferred statistic.

---

## Experimental Results
---
All the results are under plots folder

---

## Use Cases

Potential applications include:
- Edge decision engines
- Confidence-aware anomaly detection
- Real-time control and arbitration
- Event-driven and neuromorphic systems
- Hybrid classical–quantum control loops

---

## Status

This repository is an **experimental research prototype**.
The goal is to validate TSUs as a computational primitive, not to optimize performance.

Feedback and critical discussion are welcome.

---

## Author

Abhinav Basu  
BS-MS (Physics), IISER Pune  
January 2026
