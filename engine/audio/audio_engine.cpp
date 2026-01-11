#include "audio_engine.hpp"

#include "engine/core/log.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace hz {

struct AudioSystem::Impl {
    ma_engine engine;
};

// ==========================================
// AudioSystem Implementation
// ==========================================

AudioSystem::AudioSystem() : m_impl(std::make_unique<Impl>()) {}
AudioSystem::~AudioSystem() {
    shutdown();
}

void AudioSystem::SoundDeleter::operator()(ma_sound* sound) const {
    if (!sound)
        return;
    ma_sound_uninit(sound);
    delete sound;
}

void AudioSystem::init() {
    if (m_initialized)
        return;

    ma_result result;
    result = ma_engine_init(NULL, &m_impl->engine);
    if (result != MA_SUCCESS) {
        HZ_ENGINE_ERROR("Failed to initialize audio engine!");
        return;
    }

    m_initialized = true;
    HZ_ENGINE_INFO("Audio engine initialized.");
}

void AudioSystem::shutdown() {
    if (!m_initialized)
        return;

    // Unload all sounds (deleter handles uninit + delete)
    m_sounds.clear();

    ma_engine_uninit(&m_impl->engine);
    m_initialized = false;
    HZ_ENGINE_INFO("Audio engine shutdown.");
}

SoundHandle AudioSystem::load_sound(const std::string& path) {
    if (!m_initialized) {
        HZ_ENGINE_ERROR("Audio engine not initialized!");
        return {};
    }

    auto sound = SoundPtr(new ma_sound());
    auto result =
        ma_sound_init_from_file(&m_impl->engine, path.c_str(), 0, NULL, NULL, sound.get());

    if (result != MA_SUCCESS) {
        HZ_ENGINE_ERROR("Failed to load sound: {}", path);
        return {};
    }

    m_sounds.push_back(std::move(sound));

    // Generate simplified handle (using index for now)
    // Real system would use a better handle mechanism
    return SoundHandle{static_cast<u32>(m_sounds.size())};
}

void AudioSystem::play(SoundHandle handle, bool loop) {
    if (!m_initialized || !handle.is_valid())
        return;

    // 1-based index to 0-based
    u32 index = handle.id - 1;
    if (index >= m_sounds.size())
        return;

    ma_sound* sound = m_sounds[index].get();

    // Reset cursor if not looping? Or just play.
    // miniaudio restart on start?
    ma_sound_set_looping(sound, loop);
    ma_sound_seek_to_pcm_frame(sound, 0);
    ma_sound_start(sound);
}

void AudioSystem::stop(SoundHandle handle) {
    if (!m_initialized || !handle.is_valid())
        return;

    u32 index = handle.id - 1;
    if (index >= m_sounds.size())
        return;

    ma_sound* sound = m_sounds[index].get();
    ma_sound_stop(sound);
}

void AudioSystem::set_master_volume(float volume) {
    if (!m_initialized)
        return;
    ma_engine_set_volume(&m_impl->engine, volume);
}

} // namespace hz
