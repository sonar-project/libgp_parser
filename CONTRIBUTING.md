# Contributing to libgp_parser

Thank you for your interest in libgp_parser! Since this project serves as the foundation for the sonarpractice rewrite, I place great emphasis on code quality, maintainability, and a clean architecture.

If you would like to help improve the project or extend the parser logic, please follow the guidelines below.

## How can I help?

* Report a bug: If you encounter any bugs or if parsing a file fails, please open an issue.
* Adding Features: Since the current focus is on metadata and basic structures, enhancements to note and track data (GPX/GTP) are particularly welcome.
* Documentation: Improvements to the API documentation or examples are always welcome.

## Development Workflow

This project utilizes a Test-Driven Development (TDD) approach. Please adhere to the following workflow:

1. Analysis: Understand the reference implementation (TuxGuitar/Java) or the affected format.
2. Test: Create a new unit test in the `tests/` directory using Catch2 that covers the desired behavior or the error case.
3. Implementation: Write the minimal C++23 code necessary to pass the test.
4. Refactoring: Optimizing the code using modern C++ features to achieve improved performance and security (preferring references over pointers).

## Code Quality & Standards

To ensure the consistency and security of the project, we use automated analysis tools:

* Formatting: We use .clang-format (LLVM standard). Please ensure that your code is formatted before submitting.
* Static Analysis: We rely on clang-tidy with a restrictive rule set to eliminate security risks and performance issues early on.

## Pull Request Process

1. Fork the repository.
2. Create a branch for your change (git checkout -b feature/new-feature).
3. Implement your changes and ensure that all tests (including your new tests) pass successfully (ctest --test-dir build).
4. Submit a pull request. Please briefly describe which problem was solved or which functionality was added.

## A Quick Note for Contributors

Since this project serves as the core foundation for my main project, sonarpractice, I reserve the right to review pull requests based on the project's current focus. However, I am deeply grateful for any support that helps make the project more robust and feature-rich!
