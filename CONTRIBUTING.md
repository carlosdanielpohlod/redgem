# Contributing to RedGem

Thanks for wanting to help. RedGem is experimental and has plenty of rough edges: fixes, tests,
documentation and small features are all welcome.

Not sure where to start? [Known limitations](KNOWN_LIMITATIONS.md) lists what is missing or
fragile, roughly by how self-contained each piece is.

## Before you write code

- **A bug**: open an issue with the steps to reproduce it, or go straight to a pull request with
  a test that shows it.
- **A feature or a change of behaviour**: open an issue first and say what you have in mind. It
  saves you from building something that does not fit.

## Setting up

[Install](docs/install.md) covers building the engine. Then:

```bash
cmake -S . -B build -G Ninja && cmake --build build
cd build && ctest --output-on-failure      # everything; it opens real game windows for a few seconds each
```

[Working on the engine](docs/contributing.md) explains the layout of the code, the kinds of tests
and how the demo games are built.

## Keep pull requests small

A pull request should do **one thing** and be easy to read in one sitting: as a guide, **up to
about 400 changed lines**, tests included. Small pull requests get reviewed quickly and merged;
large ones wait.

- A big change goes in as a series of small pull requests, each of which leaves the project
  working. Say in the first one where the series is going.
- Keep refactoring apart from changes of behaviour: one pull request moves code, the next changes
  what it does.
- Generated files (the manual's HTML, a rebuilt scene) go in a pull request of their own when
  they are large.

## What a pull request needs

1. **Tests.** A fix comes with a test that fails without it. A feature comes with tests for what
   it does, and a system test when it is something a player or an editor user sees.
   Test code stays out of the engine: what only tests need goes in `src/testing` and
   `ruby/testing` (the `redgem-test` tool), never in what `redgem` is built from, which is what
   games ship as.
2. **Clean linters.** `tools/lint.sh` runs them all and must pass:
   - C++: clang-tidy, cppcheck, clang-format (`tools/lint.sh --fix format` formats for you).
   - Ruby: rubocop. Python: ruff. Shell: shellcheck.
3. **Documentation**, when what users see changes: the manual under `docs/`, and the scripting
   reference for anything new in the Ruby API.
4. **A filled-in description** (the template asks for what, why and how it was tested).

## How the code is written

The linters enforce most of this; the rest is habit worth keeping.

- **Small functions that do one thing.** At most 15 lines in C++ and 8 in Ruby, shallow logic,
  early returns instead of nested ifs.
- **Every number and every meaningful string has a name.** A constant with a short comment
  saying what it is and its unit.
- **Comments say why**, in plain English, where the code cannot say it itself.
- **Names say what something is for**, not how it is built.
- **Input from outside is not trusted.** Files (scenes, models, settings, saves) and anything
  from the network are checked before use: counts, sizes, kinds, ranges. Game scripts run in a
  Ruby that cannot touch files, processes or the network on its own; keep it that way.
- **Everything is in English**: code, comments, documentation, commit messages.

## Commits

Write the subject as what the change does, in the imperative or as a plain statement, under
about 70 characters; use the body for why. One logical change per commit. No need to squash
before review.

## Reporting a security problem

Please do not open a public issue for something that can be exploited. Write to the maintainer
privately first (see the repository's security policy or the maintainer's profile) and give us
time to fix it.

## Licensing of contributions

By sending a pull request you agree that your contribution is released under the project's
license. Only send code, models, sounds and images that you have the right to contribute, and
credit third-party material in the pull request.
