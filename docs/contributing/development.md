# Contributing (Development)

## The Agent Framework

We use a strict, deterministic agent framework to manage code quality.
Before submitting a PR, you must verify your changes against the agent rules.

### Running Verification

```bash
# Verify rules integrity
yamllint .agent/rules/

# Run simulator script (if available)
./scripts/verify_agent_compliance.sh
```

### Framework Structure

- **`.agent/rules/`**: YAML definitions of forbidden behaviors (e.g., `global.rules.yaml`).
- **`.agent/workflows/`**: Analysis pipelines used by the AI agents.
- **`.github/workflows/`**: CI pipelines that enforce these rules on PRs.

## Workflow

1. **Fork & Branch**: Create a feature branch.
2. **Implement**: Write C++20 code using `std::pmr` and RAII.
3. **Verify**: Ensure no new warnings are introduced (`-Werror` is on).
4. **Submit**: Open a PR. The strict CI will run `antigravity.yml` and `engine-ci.yml`.

---

## Dealing with "Strict" Mode

If the CI fails because you used `malloc` or forgot `[[nodiscard]]`, **do not disable the check**. Fix the code. The constraints are there to save us from debugging race conditions later.
