# Your First Frame

# Your First Frame

The Horizon Engine architecture has evolved to a manual composition model.

Please refer to `game/src/main.cpp` for the canonical example of how to:

1. Initialize the Engine (Window, Renderer, Audio, Physics)
2. Create the Game Loop
3. Load Assets via `AssetRegistry`
4. Create Entities and Components
5. Run the Application

> **Note:** The `hz::Application` base class pattern has been replaced by a composition approach for greater flexibility.
