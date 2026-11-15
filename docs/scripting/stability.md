---
description: "What stable and experimental mean in the scripting API, and what each release promises."
---

# API stability

Every class and method of the [API reference](api.md) is marked **stable** or **experimental**.
The mark says whether a game may rely on it from one release to the next.

## What the marks promise

- **Stable**: from 1.0, no breaking change within a 1.x minor release. A game written for 1.2
  runs on 1.3 with no change to its scripts. A stable call is removed or changed only in a major
  release (2.0), and its release notes say how to move a game across.
- **Experimental**: it may change, or go, in any release, a minor or a patch one. Use it, but
  expect to change your scripts when you update the engine.

A breaking change is one a game's scripts can see: a class or method removed or renamed, a
parameter removed, made required or given another meaning, a default changed, or a return value
of another type. Adding a method, an optional parameter or a keyword is not breaking, and neither
is a fix that makes a call do what its reference line says.

Nothing stable rests on what may change. An experimental class has only experimental methods,
and the classes nested in it or made from it are experimental too. A stable method takes and
returns only stable classes. A stable class may have a few experimental methods, which are marked
one by one.

Before 1.0, every release may change anything (see the CHANGELOG): the marks already say which
calls are meant to stay as they are.

## What is experimental

- What came in the last two releases, until it has been used in a game for a while.
- What is likely to change: NPCs and their interaction, dialogue, quests, and the assistant's
  tools.

A call becomes stable in a minor release, once it has settled; the CHANGELOG says so. A stable call
never goes back to experimental within a major version.

## Where the marks are

- The [API reference](api.md): the index has a Stability column, and an experimental class or
  method ends its line with *Experimental.*
- `redgem api`: experimental classes and methods say `(experimental)`; `redgem api --json` gives
  each class and method a `"stability"` of `"stable"` or `"experimental"`.
- The type signatures (`sig/redgem.rbs`): each class and method is annotated
  `%a{redgem:stable}` or `%a{redgem:experimental}`, which a type checker or a linter can read.
- `docs/scripting/api.json`, where the marks are written. A test fails when a class or method has
  none.

## Updating a game

Each release of the CHANGELOG has a **Migrating** section: what to change in a game made with the
release before, step by step. It lists every breaking change to an experimental call, and, in a
major release, to a stable one. When it says nothing, a game needs no change.
