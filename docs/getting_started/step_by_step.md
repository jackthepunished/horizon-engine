# Your First Frame

This guide will walk you through initializing the engine and rendering your first entity.

## 1. The Entry Point

In Horizon, your game logic lives in the `Application` class.

```cpp
#include <engine/core/entry_point.hpp>
#include <engine/core/application.hpp>

class MyGame : public hz::Application {
public:
    MyGame() : hz::Application("My First Game") {}

    void on_init() override {
        HZ_INFO("Hello, World!");
    }

    void on_update(hz::Timestep ts) override {
        // Game logic here
    }
};

hz::Application* hz::create_application() {
    return new MyGame();
}
```

## 2. Creating an Entity

Entities are created via the `EntityManager`.

```cpp
void on_init() override {
    // defined in base class
    auto entity = m_world->create_entity();

    // Add a Transform component
    m_world->add_component<hz::TransformComponent>(entity, {
        .position = {0.0f, 0.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    // Add a Mesh component
    m_world->add_component<hz::MeshComponent>(entity, {
        .mesh_handle = m_asset_manager->load_mesh("assets/cube.obj")
    });
}
```

## 3. The Game Loop

The engine automatically handles the loop for you.

- `on_update(ts)` is called every frame (variable step).
- `on_physics_update(fixed_ts)` is called at 60Hz (fixed step).

Override `on_physics_update` for deterministic logic!
