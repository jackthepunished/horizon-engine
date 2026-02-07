#pragma once

/**
 * @file result.hpp
 * @brief Unified error handling primitives for the Horizon Engine
 *
 * Provides a Result<T, E> type similar to std::expected (C++23) for use
 * in C++20 codebases. Enables explicit, composable error propagation
 * without relying on exceptions or raw boolean/nullptr returns.
 *
 * Usage:
 * @code
 *   Result<Texture*, EngineError> load_texture(const std::string& path);
 *
 *   auto result = load_texture("diffuse.png");
 *   if (result) {
 *       use(*result);
 *   } else {
 *       handle(result.error());
 *   }
 * @endcode
 */

#include "types.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace hz {

// ============================================================================
// Error Types
// ============================================================================

/**
 * @brief Error category for classifying engine errors
 */
enum class ErrorCategory : u8 {
    None = 0,
    Runtime,     ///< General runtime error
    IO,          ///< File I/O error
    Graphics,    ///< GPU / rendering error
    Asset,       ///< Asset loading/processing error
    Physics,     ///< Physics simulation error
    Audio,       ///< Audio subsystem error
    Platform,    ///< Platform/window error
    Validation,  ///< Input validation error
    OutOfMemory, ///< Memory allocation failure
};

/**
 * @brief Lightweight error descriptor carrying a category and message
 *
 * Designed to be cheap to copy and move. Intended as the default
 * error type for Result<T>.
 */
class EngineError {
public:
    EngineError() = default;

    EngineError(ErrorCategory category, std::string message)
        : m_category(category), m_message(std::move(message)) {}

    EngineError(ErrorCategory category, std::string_view message)
        : m_category(category), m_message(message) {}

    [[nodiscard]] ErrorCategory category() const noexcept { return m_category; }
    [[nodiscard]] const std::string& message() const noexcept { return m_message; }
    [[nodiscard]] std::string_view message_view() const noexcept { return m_message; }

    [[nodiscard]] bool operator==(const EngineError& other) const noexcept {
        return m_category == other.m_category && m_message == other.m_message;
    }

private:
    ErrorCategory m_category{ErrorCategory::None};
    std::string m_message;
};

// ============================================================================
// Error Factory Helpers
// ============================================================================

/**
 * @brief Convenience functions for creating typed errors
 */
namespace errors {

[[nodiscard]] inline EngineError runtime(std::string msg) {
    return {ErrorCategory::Runtime, std::move(msg)};
}

[[nodiscard]] inline EngineError io(std::string msg) {
    return {ErrorCategory::IO, std::move(msg)};
}

[[nodiscard]] inline EngineError graphics(std::string msg) {
    return {ErrorCategory::Graphics, std::move(msg)};
}

[[nodiscard]] inline EngineError asset(std::string msg) {
    return {ErrorCategory::Asset, std::move(msg)};
}

[[nodiscard]] inline EngineError physics(std::string msg) {
    return {ErrorCategory::Physics, std::move(msg)};
}

[[nodiscard]] inline EngineError audio(std::string msg) {
    return {ErrorCategory::Audio, std::move(msg)};
}

[[nodiscard]] inline EngineError platform(std::string msg) {
    return {ErrorCategory::Platform, std::move(msg)};
}

[[nodiscard]] inline EngineError validation(std::string msg) {
    return {ErrorCategory::Validation, std::move(msg)};
}

[[nodiscard]] inline EngineError out_of_memory(std::string msg) {
    return {ErrorCategory::OutOfMemory, std::move(msg)};
}

} // namespace errors

// ============================================================================
// Result<T, E>
// ============================================================================

/**
 * @brief Tag type for constructing a Result in the error state
 */
template <typename E>
struct Unexpected {
    E error;

    explicit Unexpected(E e) : error(std::move(e)) {}
};

/**
 * @brief Deduction guide for Unexpected
 */
template <typename E>
Unexpected(E) -> Unexpected<E>;

/**
 * @brief A value-or-error type for explicit error propagation
 *
 * Models a discriminated union between a success value of type T and
 * an error of type E (defaulting to EngineError).
 *
 * Key design goals:
 * - [[nodiscard]] everywhere so errors cannot be silently ignored
 * - Implicit construction from T for ergonomic success returns
 * - Explicit Unexpected<E> wrapper for error construction
 * - Monadic operations (and_then, transform, or_else) for composition
 *
 * @tparam T The success value type
 * @tparam E The error type (defaults to EngineError)
 */
template <typename T, typename E = EngineError>
class [[nodiscard]] Result {
public:
    // ========================================================================
    // Constructors
    // ========================================================================

    /**
     * @brief Construct a successful result from a value
     */
    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(T value) : m_storage(std::move(value)) {}

    /**
     * @brief Construct an error result from an Unexpected wrapper
     */
    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(Unexpected<E> err) : m_storage(std::move(err.error)) {}

    // ========================================================================
    // Observers
    // ========================================================================

    /**
     * @brief Check if the result holds a value (success)
     */
    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(m_storage); }

    /**
     * @brief Boolean conversion: true if success
     */
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    /**
     * @brief Access the value (undefined behavior if error)
     */
    [[nodiscard]] T& value() & {
        assert(has_value() && "Accessing value of error Result");
        return std::get<T>(m_storage);
    }

    [[nodiscard]] const T& value() const& {
        assert(has_value() && "Accessing value of error Result");
        return std::get<T>(m_storage);
    }

    [[nodiscard]] T&& value() && {
        assert(has_value() && "Accessing value of error Result");
        return std::get<T>(std::move(m_storage));
    }

    /**
     * @brief Access the error (undefined behavior if success)
     */
    [[nodiscard]] const E& error() const& {
        assert(!has_value() && "Accessing error of success Result");
        return std::get<E>(m_storage);
    }

    [[nodiscard]] E&& error() && {
        assert(!has_value() && "Accessing error of success Result");
        return std::get<E>(std::move(m_storage));
    }

    /**
     * @brief Dereference operator (access value)
     */
    [[nodiscard]] T& operator*() & { return value(); }
    [[nodiscard]] const T& operator*() const& { return value(); }
    [[nodiscard]] T&& operator*() && { return std::move(*this).value(); }

    /**
     * @brief Arrow operator (access value members)
     */
    [[nodiscard]] T* operator->() { return &value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }

    // ========================================================================
    // Value Extraction
    // ========================================================================

    /**
     * @brief Return the value if success, or the provided default
     */
    [[nodiscard]] T value_or(T default_value) const& {
        return has_value() ? value() : std::move(default_value);
    }

    [[nodiscard]] T value_or(T default_value) && {
        return has_value() ? std::move(*this).value() : std::move(default_value);
    }

    // ========================================================================
    // Monadic Operations
    // ========================================================================

    /**
     * @brief If success, apply f to the value and return the result.
     *        If error, propagate the error.
     *
     * f must return Result<U, E> for some U.
     */
    template <typename F>
    [[nodiscard]] auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
        if (has_value()) {
            return std::forward<F>(f)(value());
        }
        return Unexpected(error());
    }

    template <typename F>
    [[nodiscard]] auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
        if (has_value()) {
            return std::forward<F>(f)(std::move(*this).value());
        }
        return Unexpected(std::move(*this).error());
    }

    /**
     * @brief If success, apply f to the value and wrap in Result.
     *        If error, propagate the error.
     *
     * f returns a plain value U, not a Result.
     */
    template <typename F>
    [[nodiscard]] auto transform(F&& f) const& -> Result<std::invoke_result_t<F, const T&>, E> {
        if (has_value()) {
            return std::forward<F>(f)(value());
        }
        return Unexpected(error());
    }

    template <typename F>
    [[nodiscard]] auto transform(F&& f) && -> Result<std::invoke_result_t<F, T&&>, E> {
        if (has_value()) {
            return std::forward<F>(f)(std::move(*this).value());
        }
        return Unexpected(std::move(*this).error());
    }

    /**
     * @brief If error, apply f to the error and return the result.
     *        If success, propagate the value.
     *
     * f must return Result<T, E2> for some E2.
     */
    template <typename F>
    [[nodiscard]] auto or_else(F&& f) const& -> std::invoke_result_t<F, const E&> {
        if (has_value()) {
            return value();
        }
        return std::forward<F>(f)(error());
    }

    template <typename F>
    [[nodiscard]] auto or_else(F&& f) && -> std::invoke_result_t<F, E&&> {
        if (has_value()) {
            return std::move(*this).value();
        }
        return std::forward<F>(f)(std::move(*this).error());
    }

private:
    std::variant<T, E> m_storage;
};

// ============================================================================
// Result<void, E> Specialization
// ============================================================================

/**
 * @brief Specialization for operations that succeed with no value
 *
 * Usage:
 * @code
 *   Result<void> initialize() {
 *       if (failed) return Unexpected(errors::runtime("init failed"));
 *       return {};
 *   }
 * @endcode
 */
template <typename E>
class [[nodiscard]] Result<void, E> {
public:
    /**
     * @brief Construct a successful void result
     */
    Result() : m_error(std::nullopt) {}

    /**
     * @brief Construct an error result
     */
    // NOLINTNEXTLINE(google-explicit-constructor)
    Result(Unexpected<E> err) : m_error(std::move(err.error)) {}

    [[nodiscard]] bool has_value() const noexcept { return !m_error.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] const E& error() const& {
        assert(!has_value() && "Accessing error of success Result<void>");
        return *m_error;
    }

    [[nodiscard]] E&& error() && {
        assert(!has_value() && "Accessing error of success Result<void>");
        return std::move(*m_error);
    }

    template <typename F>
    [[nodiscard]] auto and_then(F&& f) const& -> std::invoke_result_t<F> {
        if (has_value()) {
            return std::forward<F>(f)();
        }
        return Unexpected(error());
    }

    template <typename F>
    [[nodiscard]] auto or_else(F&& f) const& -> std::invoke_result_t<F, const E&> {
        if (has_value()) {
            return {};
        }
        return std::forward<F>(f)(error());
    }

private:
    std::optional<E> m_error;
};

} // namespace hz
