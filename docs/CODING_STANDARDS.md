# Coding Standards (Summary)

**Full document**: [standards/CODING_STANDARDS.md](standards/CODING_STANDARDS.md)

---

## Quick Reference

- Follow MISRA-like conventions where practical
- Use clear module-prefixed symbols: `module_function()`
- Avoid dynamic memory in flight code (`no malloc/free`)
- Compile with `-Wall -Wextra -pedantic` and treat warnings as errors for release builds
- Use `stdint.h` types for hardware registers (`uint8_t`, `int32_t`)
- Error return convention: `0` = success, `>0` = warning, `<0` = error
- Every public API must have a unit test
- Commit format: `<type>(<scope>): <description>`

See the full coding standards in [standards/CODING_STANDARDS.md](standards/CODING_STANDARDS.md) for:
- Naming conventions (functions, types, constants, variables)
- Code safety rules (memory, type safety, bounds checking)
- File organization and documentation patterns
- FreeRTOS conventions (tasks, priorities, shared data)
- Git commit message format
- Testing requirements
