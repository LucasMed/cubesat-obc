# Skill Registry — cubesat-obc

Generated: 2026-04-08
Mode: hybrid (openspec + engram)

---

## Project Standards

### C Coding (MISRA C / ECSS-Q-ST-80C)

| Rule | Standard |
|------|----------|
| Naming functions | `module_function_name()` |
| Naming constants | `MODULE_CONSTANT` |
| Naming types | `module_type_t` |
| Naming globals | `g_module_var` |
| Naming statics | `s_static_var` |
| Max line length | 100 characters |
| Indentation | 2 spaces, no tabs |
| Brace style | Allman (every brace on own line) |
| Memory | NO malloc in flight code (static allocation only) |
| Compiler flags | `-Wall -Wextra -pedantic -Werror` |
| C standard | C11 (`-std=c11`) |

### Build System

| Item | Value |
|------|-------|
| Build tool | CMake 3.13+ |
| Host build | `cmake -B build -DPICO_ENABLED=OFF` |
| Pico build | `cmake -B build -DPICO_ENABLED=ON -DPICO_SDK_PATH=...` |
| Test command | `ctest --test-dir build --output-on-failure` |
| Coverage | `gcovr --json summary.json` |
| Static analysis | `bash scripts/static_analysis.sh` |
| Clean build | `rm -rf build && cmake -B build ...` |

### Testing

| Layer | Location | Framework |
|-------|----------|-----------|
| Unit | `tests/unit/` | Custom C harness |
| Integration | `tests/integration/` | Custom C harness |
| Coverage target | ≥90% line, ≥90% function | gcovr |
| Current coverage | 93.0% line, 92.4% function | ✅ |

### Conventional Commits

```
<type>: <subject>

Types: feat, fix, docs, test, refactor, chore, style, perf
```

| Type | Usage |
|------|-------|
| feat | New feature |
| fix | Bug fix |
| docs | Documentation |
| test | Test additions |
| refactor | Code reorganization |
| chore | Build, CI, dependencies |
| style | Formatting (whitespace) |
| perf | Performance improvements |

### Branch Naming

```
feature/<description>     # New features
fix/<description>         # Bug fixes
docs/<description>        # Documentation
test/<description>        # Tests
refactor/<description>    # Code reorganization
```

### PR Checklist

- [ ] Tests pass: `ctest --output-on-failure`
- [ ] No warnings: `cmake --build build`
- [ ] Static analysis: `bash scripts/static_analysis.sh`
- [ ] Coverage maintained: ≥90%
- [ ] CHANGELOG.md updated
- [ ] Documentation updated (if applicable)

---

## User Skills Triggers

| Context | Skill to Load |
|---------|---------------|
| Writing Go tests | go-testing |
| Creating new AI skills | skill-creator |
| Running SDD phases | sdd-* (auto-resolved) |
| CDR review, ecss-cdr, cdr checklist, CDR gate validation | ecss-cdr-review |
| Traceability, ecss-trace, trace, requirements coverage, RTM validation | ecss-trace |
| ECSS gate, GO/NO-GO, review gate readiness | ecss-gate |

---

## Code Patterns

### HAL Pattern (Hardware Abstraction Layer)

```c
// include/pwm_hal.h
void pwm_hal_init(void);
void pwm_hal_set_duty(uint8_t channel, float duty);
float pwm_hal_get_duty(uint8_t channel);

// Implementations:
// src/actuators/pwm_hal.c       → Real hardware (Pico SDK)
// include/host/pwm_hal_stub.c   → Host mock (PICO_ENABLED=OFF)
__attribute__((weak)) in source for stubs
```

### Task Structure (FreeRTOS)

```c
// Standard FreeRTOS task pattern
void vTaskName_Task(void *pvParams) {
    for (;;) {
        if (task_init_complete()) {
            vTaskName_Step();
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));
    }
}
```

### Error Handling

```c
// Flight mode guard pattern
if (flight_mode_get() == FM_SAFE) {
    return;  // Do nothing in safe mode
}
```

---

## Documentation Standards

| Document | Location | Standard |
|----------|----------|----------|
| Requirements | `docs/ecss/requirements/SRS-*.md` | ECSS |
| Architecture | `docs/architecture/ARCHITECTURE.md` | ECSS |
| Design | `docs/ecss/design/DES-*.md` | ECSS |
| Test Plans | `docs/ecss/test_plans/STP-*.md` | ECSS |
| Safety | `docs/ecss/safety/FMEA-*.md` | ECSS |

---

## Relevant Files

| Purpose | Path |
|---------|------|
| Entry point | `src/obc_main.c` |
| Tasks | `src/tasks/*.c` |
| Control | `src/control/*.c` |
| Drivers | `src/drivers/*.c` |
| Config | `config/FreeRTOSConfig.h` |
| Tests | `tests/unit/*.c` |

---

## SDD Workflow

| Command | Purpose |
|---------|---------|
| `/sdd-init` | Initialize SDD context (done) |
| `/sdd-explore <topic>` | Investigate ideas |
| `/sdd-new <change>` | Start new change (proposal → specs → design → tasks) |
| `/sdd-continue` | Continue interrupted change |
| `/sdd-ff <name>` | Fast-forward (single-command workflow) |
| `/sdd-apply` | Implement tasks |
| `/sdd-verify` | Validate implementation |
| `/sdd-archive` | Close and persist |
