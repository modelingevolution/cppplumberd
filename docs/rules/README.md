# Principles

1. Make requirements less dump, question requirements in the first place.
2. Delete unnecessary code. Simplify. Less code lines is better.
3. Fail fast, don't be resilent.
4. Code should be self-explanatory. Avoid comments. Only add comments when things are ambiguous or very! not common.

# Naming conventions:

1. All methods from capital pascal case.
2. All classes from capital pascal case.
3. All class fields from underscore snake case.

# Development

1. Exceptions over return codes.
2. Composition over inheritance.
3. SOLID
4. std over boost.

# Tests

1. gMock over custom mocking.
2. Possibly small tests, but in BDD style.
3. Naming: module.hpp -> module_tests.cpp

# Design

1. Start from interfaces, it's resposibility, and call-stack hierarchy.
2. Explain call-stack hierarchy with example.
3. Write a failing test with interfaces and right call hierarchy.
4. Simply and get rid of unnecessary components. Refine resposibility.