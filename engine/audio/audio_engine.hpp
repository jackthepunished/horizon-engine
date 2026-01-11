#pragma once

/**
 * @file audio_engine.hpp
 * @brief High-performance audio engine using miniaudio
 */

#include "engine/core/types.hpp"

#include <memory>
#include <string>
#include <vector>

// Forward declaration
struct ma_engine;
struct ma_sound;

namespace hz {

/**
 * @brief Handle for a loaded sound resource
 */
struct SoundHandle {
    u32 id{0};
    bool is_valid() const { return id != 0; }
};

/**
 * @brief Main audio engine class
 */
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem() noexcept;

    HZ_NON_COPYABLE(AudioSystem);
    HZ_NON_MOVABLE(AudioSystem);

    struct DebugType {};

    /**
     * @brief Initialize the audio engine
     */
    void init();

    /**
     * @brief Shutdown the audio engine
     */
    void shutdown();

    /**
     * @brief Load a sound from a file
     * @param path Path to audio file (wav, mp3, flac)
     * @return Handle to the loaded sound
     */
    SoundHandle load_sound(const std::string& path);

    /**
     * @brief Play a sound
     * @param handle Handle of sound to play
     * @param loop Whether to loop the sound
     */
    void play(SoundHandle handle, bool loop = false);

    /**
     * @brief Stop a sound
     */
    void stop(SoundHandle handle);

    /**
     * @brief Set global master volume
     * @param volume Volume level (0.0 to 1.0+)
     */
    void set_master_volume(float volume);

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool is_initialized() const { return m_initialized; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_initialized{false};

    struct SoundDeleter {
        void operator()(ma_sound* sound) const;
    };

    using SoundPtr = std::unique_ptr<ma_sound, SoundDeleter>;

    // Keep track of sound resources
    std::vector<SoundPtr> m_sounds;
};

} // namespace hz
