/**
 * @file test_result.cpp
 * @brief Unit tests for Result<T, E> error handling type
 */

#include <string>

#include <catch2/catch_test_macros.hpp>
#include <engine/core/result.hpp>

using namespace hz;

// ============================================================================
// EngineError Tests
// ============================================================================

TEST_CASE("EngineError construction and accessors", "[result][error]") {
    SECTION("Default construction") {
        EngineError err;
        REQUIRE(err.category() == ErrorCategory::None);
        REQUIRE(err.message().empty());
    }

    SECTION("Construct with category and string") {
        EngineError err(ErrorCategory::Graphics, std::string("shader compilation failed"));
        REQUIRE(err.category() == ErrorCategory::Graphics);
        REQUIRE(err.message() == "shader compilation failed");
        REQUIRE(err.message_view() == "shader compilation failed");
    }

    SECTION("Construct with category and string_view") {
        std::string_view msg = "file not found";
        EngineError err(ErrorCategory::IO, msg);
        REQUIRE(err.category() == ErrorCategory::IO);
        REQUIRE(err.message() == "file not found");
    }

    SECTION("Equality comparison") {
        EngineError a(ErrorCategory::Runtime, std::string("oops"));
        EngineError b(ErrorCategory::Runtime, std::string("oops"));
        EngineError c(ErrorCategory::Runtime, std::string("different"));
        EngineError d(ErrorCategory::IO, std::string("oops"));

        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
        REQUIRE_FALSE(a == d);
    }
}

// ============================================================================
// Error Factory Helpers
// ============================================================================

TEST_CASE("Error factory helpers", "[result][error]") {
    SECTION("errors::runtime") {
        auto err = errors::runtime("test");
        REQUIRE(err.category() == ErrorCategory::Runtime);
        REQUIRE(err.message() == "test");
    }

    SECTION("errors::io") {
        auto err = errors::io("test");
        REQUIRE(err.category() == ErrorCategory::IO);
    }

    SECTION("errors::graphics") {
        auto err = errors::graphics("test");
        REQUIRE(err.category() == ErrorCategory::Graphics);
    }

    SECTION("errors::asset") {
        auto err = errors::asset("test");
        REQUIRE(err.category() == ErrorCategory::Asset);
    }

    SECTION("errors::physics") {
        auto err = errors::physics("test");
        REQUIRE(err.category() == ErrorCategory::Physics);
    }

    SECTION("errors::audio") {
        auto err = errors::audio("test");
        REQUIRE(err.category() == ErrorCategory::Audio);
    }

    SECTION("errors::platform") {
        auto err = errors::platform("test");
        REQUIRE(err.category() == ErrorCategory::Platform);
    }

    SECTION("errors::validation") {
        auto err = errors::validation("test");
        REQUIRE(err.category() == ErrorCategory::Validation);
    }

    SECTION("errors::out_of_memory") {
        auto err = errors::out_of_memory("test");
        REQUIRE(err.category() == ErrorCategory::OutOfMemory);
    }
}

// ============================================================================
// Result<T, E> Success Cases
// ============================================================================

TEST_CASE("Result<T> success construction", "[result]") {
    SECTION("Implicit construction from value") {
        Result<int> r = 42;
        REQUIRE(r.has_value());
        REQUIRE(static_cast<bool>(r));
        REQUIRE(r.value() == 42);
    }

    SECTION("String value") {
        Result<std::string> r = std::string("hello");
        REQUIRE(r.has_value());
        REQUIRE(r.value() == "hello");
    }

    SECTION("Pointer value") {
        int x = 10;
        Result<int*> r = &x;
        REQUIRE(r.has_value());
        REQUIRE(*r.value() == 10);
    }
}

// ============================================================================
// Result<T, E> Error Cases
// ============================================================================

TEST_CASE("Result<T> error construction via Unexpected", "[result]") {
    SECTION("Construct with Unexpected") {
        Result<int> r = Unexpected(errors::runtime("bad"));
        REQUIRE_FALSE(r.has_value());
        REQUIRE_FALSE(static_cast<bool>(r));
        REQUIRE(r.error().category() == ErrorCategory::Runtime);
        REQUIRE(r.error().message() == "bad");
    }

    SECTION("Construct with custom error type") {
        Result<int, std::string> r = Unexpected(std::string("custom error"));
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error() == "custom error");
    }
}

// ============================================================================
// Result<T, E> Observers
// ============================================================================

TEST_CASE("Result<T> observers", "[result]") {
    SECTION("operator* on success") {
        Result<int> r = 42;
        REQUIRE(*r == 42);
    }

    SECTION("operator* const on success") {
        const Result<int> r = 42;
        REQUIRE(*r == 42);
    }

    SECTION("operator-> on success") {
        Result<std::string> r = std::string("hello");
        REQUIRE(r->size() == 5);
    }

    SECTION("const operator-> on success") {
        const Result<std::string> r = std::string("hello");
        REQUIRE(r->size() == 5);
    }

    SECTION("move value out") {
        Result<std::string> r = std::string("move me");
        std::string moved = std::move(r).value();
        REQUIRE(moved == "move me");
    }

    SECTION("move error out") {
        Result<int> r = Unexpected(errors::io("lost"));
        EngineError moved = std::move(r).error();
        REQUIRE(moved.category() == ErrorCategory::IO);
        REQUIRE(moved.message() == "lost");
    }
}

// ============================================================================
// Result<T, E> value_or
// ============================================================================

TEST_CASE("Result<T> value_or", "[result]") {
    SECTION("Returns value when success") {
        Result<int> r = 42;
        REQUIRE(r.value_or(0) == 42);
    }

    SECTION("Returns default when error") {
        Result<int> r = Unexpected(errors::runtime("fail"));
        REQUIRE(r.value_or(99) == 99);
    }

    SECTION("Rvalue value_or returns moved value") {
        Result<std::string> r = std::string("hello");
        std::string s = std::move(r).value_or("default");
        REQUIRE(s == "hello");
    }

    SECTION("Rvalue value_or returns default on error") {
        Result<std::string> r = Unexpected(errors::runtime("x"));
        std::string s = std::move(r).value_or("default");
        REQUIRE(s == "default");
    }
}

// ============================================================================
// Result<T, E> Monadic Operations
// ============================================================================

TEST_CASE("Result<T> and_then", "[result][monadic]") {
    auto double_if_positive = [](int x) -> Result<int> {
        if (x > 0) {
            return x * 2;
        }
        return Unexpected(errors::validation("not positive"));
    };

    SECTION("Chains on success") {
        Result<int> r = 5;
        auto chained = r.and_then(double_if_positive);
        REQUIRE(chained.has_value());
        REQUIRE(*chained == 10);
    }

    SECTION("Propagates original error") {
        Result<int> r = Unexpected(errors::runtime("original"));
        auto chained = r.and_then(double_if_positive);
        REQUIRE_FALSE(chained.has_value());
        REQUIRE(chained.error().message() == "original");
    }

    SECTION("Returns new error from function") {
        Result<int> r = -1;
        auto chained = r.and_then(double_if_positive);
        REQUIRE_FALSE(chained.has_value());
        REQUIRE(chained.error().message() == "not positive");
    }
}

TEST_CASE("Result<T> transform", "[result][monadic]") {
    auto to_string = [](int x) -> std::string { return std::to_string(x); };

    SECTION("Transforms value on success") {
        Result<int> r = 42;
        auto transformed = r.transform(to_string);
        REQUIRE(transformed.has_value());
        REQUIRE(*transformed == "42");
    }

    SECTION("Propagates error") {
        Result<int> r = Unexpected(errors::runtime("err"));
        auto transformed = r.transform(to_string);
        REQUIRE_FALSE(transformed.has_value());
        REQUIRE(transformed.error().message() == "err");
    }
}

TEST_CASE("Result<T> or_else", "[result][monadic]") {
    auto recover = [](const EngineError& /*err*/) -> Result<int> { return 0; };

    SECTION("Passes through on success") {
        Result<int> r = 42;
        auto recovered = r.or_else(recover);
        REQUIRE(recovered.has_value());
        REQUIRE(*recovered == 42);
    }

    SECTION("Recovers on error") {
        Result<int> r = Unexpected(errors::runtime("fail"));
        auto recovered = r.or_else(recover);
        REQUIRE(recovered.has_value());
        REQUIRE(*recovered == 0);
    }
}

// ============================================================================
// Result<void, E> Specialization
// ============================================================================

TEST_CASE("Result<void> success", "[result][void]") {
    SECTION("Default construction is success") {
        Result<void> r;
        REQUIRE(r.has_value());
        REQUIRE(static_cast<bool>(r));
    }

    SECTION("Can return success from function") {
        auto fn = []() -> Result<void> { return {}; };
        auto r = fn();
        REQUIRE(r.has_value());
    }
}

TEST_CASE("Result<void> error", "[result][void]") {
    SECTION("Construct with Unexpected") {
        Result<void> r = Unexpected(errors::runtime("void error"));
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().category() == ErrorCategory::Runtime);
        REQUIRE(r.error().message() == "void error");
    }

    SECTION("Move error out") {
        Result<void> r = Unexpected(errors::io("io fail"));
        EngineError err = std::move(r).error();
        REQUIRE(err.category() == ErrorCategory::IO);
    }
}

TEST_CASE("Result<void> and_then", "[result][void][monadic]") {
    int call_count = 0;
    auto increment = [&]() -> Result<void> {
        call_count++;
        return {};
    };

    SECTION("Calls function on success") {
        Result<void> r;
        auto chained = r.and_then(increment);
        REQUIRE(chained.has_value());
        REQUIRE(call_count == 1);
    }

    SECTION("Skips function on error") {
        Result<void> r = Unexpected(errors::runtime("skip"));
        auto chained = r.and_then(increment);
        REQUIRE_FALSE(chained.has_value());
        REQUIRE(call_count == 0);
    }
}

TEST_CASE("Result<void> or_else", "[result][void][monadic]") {
    auto recover = [](const EngineError& /*err*/) -> Result<void> { return {}; };

    SECTION("Passes through on success") {
        Result<void> r;
        auto recovered = r.or_else(recover);
        REQUIRE(recovered.has_value());
    }

    SECTION("Recovers on error") {
        Result<void> r = Unexpected(errors::runtime("fail"));
        auto recovered = r.or_else(recover);
        REQUIRE(recovered.has_value());
    }
}

// ============================================================================
// Result<T, E> with function return patterns
// ============================================================================

TEST_CASE("Result in realistic usage patterns", "[result][integration]") {
    SECTION("Function returning success or error") {
        auto load_value = [](bool should_fail) -> Result<int> {
            if (should_fail) {
                return Unexpected(errors::io("load failed"));
            }
            return 42;
        };

        auto success = load_value(false);
        REQUIRE(success.has_value());
        REQUIRE(*success == 42);

        auto failure = load_value(true);
        REQUIRE_FALSE(failure.has_value());
        REQUIRE(failure.error().category() == ErrorCategory::IO);
    }

    SECTION("Chained operations") {
        auto parse_int = [](const std::string& s) -> Result<int> {
            if (s.empty()) {
                return Unexpected(errors::validation("empty string"));
            }
            return std::stoi(s);
        };

        auto double_it = [](int x) -> Result<int> { return x * 2; };

        auto r = parse_int("21").and_then(double_it);
        REQUIRE(r.has_value());
        REQUIRE(*r == 42);

        auto r2 = parse_int("").and_then(double_it);
        REQUIRE_FALSE(r2.has_value());
        REQUIRE(r2.error().message() == "empty string");
    }
}
