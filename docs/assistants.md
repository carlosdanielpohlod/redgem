---
description: Making games with a coding assistant: the guide every new game gets, the commands to use, and how to keep it cheap.
---

# Making games with an AI assistant

A coding assistant that works in a terminal (one that reads files and runs commands) can make a
RedGem game with you: write behaviours, place objects in a level, play the game and read what went
wrong. RedGem gives it what it needs in text, so it does not have to guess an API it has never
seen, open a binary scene file or look at a window.

## What a new game comes with

`redgem new` (and the project hub) writes two files into every new game:

- **`AGENTS.md`**: the guide to the game, in the file most assistants look for: its layout, the
  commands below with examples, how to run its tests, its conventions (one behaviour per file,
  named after its class) and the 25 calls games make most, each with a line of Ruby. Under 120
  lines, so it costs little to read at the start of every session. The calls are taken from the
  same reference `redgem api` prints, so they are the engine's real ones.
- **`CLAUDE.md`**: one line that points at `AGENTS.md`, for the assistant that reads that file
  instead.

Edit `AGENTS.md` as the game grows: what an assistant should know about *your* game (its rules,
its scenes, what not to touch) belongs there.

## The commands it uses

Every one answers in a few lines and ends; each says more with `--help`. See
[The command line](command-line.md) for all of them.

| Need | Command |
|------|---------|
| What is in a level | `redgem scene list .` (a line per object: `#5 Player pos=0,1.1,0 scripts=Player tag=player`) |
| One object in full | `redgem scene get . Player` |
| Move, tune, attach | `redgem scene set . Coin1 position=1,2,3 prop.speed=2 script+=Spinner` |
| Add or remove | `redgem scene add . Coin6 --from Coin1 position=4,1,0`, `redgem scene remove . Coin6` |
| Is the game in order | `redgem check .` (and `--run-tests` to run its tests) |
| Does it play | `redgem run . --seconds 5`: each script error once, with its file and line |
| What it looks like | `redgem shot . shot.png --at 2`: a small picture of that moment |
| What a class can do | `redgem api`, `redgem api Transform`, `redgem api --search raycast` |

Scenes are changed through the same code the editor saves them with, so an edit made from the
command line is saved exactly as the editor would save it. Objects are named by a short id
(`#12`), a path or a name; a name several objects share is refused with each of them listed.

The common errors come with what to do about them: a mistyped method of one of the engine's
classes is answered with the names near it, a class no script defines with the file it is looked
for in, a scene the game lacks with the scenes it has.

```console
$ redgem run . --frames 30
error: scripts/goal.rb:3: Goal#start on 'Goal' (behaviour disabled): undefined method 'positon=' for Transform (NoMethodError) -> did you mean: position=, position (redgem api Transform)
ran main for 30 frames (0.5 s): 1 errors, 0 warnings
```

## A good loop

1. Ask for one thing at a time: "a coin that spins and adds to the score".
2. The assistant writes the behaviour in `scripts/`, attaches it with `redgem scene set`, then
   runs `redgem check .` and `redgem run .` and fixes what they report.
3. When it matters how it looks, `redgem shot` takes a picture to look at.
4. A system test in `tests/` (see [Testing your game](testing.md)) keeps it working:
   `redgem check . --run-tests`.
5. Open the editor (`redgem .`) to see and play it yourself.

## Keeping it cheap

What an assistant reads, it pays for. RedGem's answers are short on purpose; a few habits keep
them so:

- List a level with `--depth 0` or `--limit 20` first, then `get` the objects that matter.
- Ask `redgem api <Class>` or `redgem api --search <word>` instead of reading the whole reference.
- Prefer `redgem run` (text) to `redgem shot` (a picture) to find out whether something works; a
  picture is for how it looks. Shots are 512x288 unless asked otherwise.
- `redgem check` lists a screenful of problems; fix those before asking for `--all`.

## The manual for assistants

The manual's site has `llms.txt`, an index of its pages with a line about each, and
`llms-full.txt`, every page in one file to search. The docs build writes both
(`tools/docs/llms_txt.py`). The API reference is also data: `redgem api --json`, and
`docs/scripting/api.json` in the repository, which tests keep in step with the engine.
