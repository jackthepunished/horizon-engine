# Introduction

> _"Control is not a feature. It is the foundation."_

**Horizon Engine** was born from a singular refusal: the refusal to accept "magic" in game development.

In an era of black-box engines, unpredictable garbage collectors, and "good enough" performance, Horizon stands as a fortress of **determinism, explicit ownership, and absolute control**. We are building an engine for the engineers who demand to know exactly what is happening in every microsecond of the frame.

## The Vision

We envision a development environment where **predictability** is the default state.

- **No Guesswork**: Memory is allocated where you say it is. Threads synchronize when you tell them to.
- **No Pauses**: A game loop that never stutters, powered by `std::pmr` arenas that wipe clean every frame instantly.
- **No Legacy**: Built on **C++20** from day one. We leverage Concepts, Modules, and Coroutines to write code that is expressive, safe, and ruthlessly efficient.

## The Philosophy

Our architecture is guided by three immutable pillars:

### 1. Determinism is Holy

If you run the simulation twice with the same input, you must get the **exact same frame**. To achieve this, we reject floating-point non-determinism, we reject race conditions, and we reject global mutable state. Everything is an input; everything is a pure transformation.

### 2. Zero-Cost, For Real

We don't just say "zero-cost abstractions"—we enforce them.

- We prefer **Generational Indices** over pointers to eliminate cache misses and dangling references.
- We prefer **Data-Oriented Design (ECS)** over OOP hierarchies to ensure your CPU cache is always hot.
- We prefer **Vulkan** over high-level wrappers to ensure the GPU does exactly what logic demands.

### 3. Safety through Architecture

Safety shouldn't come at the cost of performance. By using **RAII** for every single resource—from Vulkan handles to memory pools—we ensure that leaks are mathematically impossible. We don't need a Garbage Collector because we don't generate garbage. We have **Architecture**.

---

## Why Horizon?

Because you are tired of fighting your tools. Because you want to build simulations that scale to millions of entities. Because you believe that code should be beautiful, correct, and fast—all at the same time.

Welcome to the event horizon. **Welcome to Horizon Engine.**
