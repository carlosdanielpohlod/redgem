---
description: "Making games with a coding assistant: the guide every new game gets, the commands to use, and how to keep it cheap."
---

# Making games with an AI assistant

For an MCP client, [the stdio server](mcp.md) exposes the same developer commands through
eight tools, with a read-only mode, an allow list and cursor-based process events.

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

## Repeating a headless session

`redgem run` uses fixed steps of 1/60 s and a random seed of 0. Use `--seed N` to repeat another
Ruby and particle random stream, including random values read while scripts first load. A seed
is a whole number from 0 to 2147483647. Scatter requests keep their own explicit seeds. Network
traffic, web responses and scripts that read external clocks or files are outside this replay.

Keyboard input comes from a JSON file. Each event replaces the whole set of held keys before
its zero-based frame; keys remain held until the next event. An empty list releases them:

```json
[
  {"frame": 0, "keys": ["w"]},
  {"frame": 30, "keys": ["w", "space"]},
  {"frame": 31, "keys": ["w"]},
  {"frame": 120, "keys": []}
]
```

```sh
redgem run my-game --seed 42 --frames 180 --input input.json \
  --assert "Scene.find('Player').transform.position.y > -10" \
  --print "Scene.find('Player').transform.position" --every 60
```

`--assert` evaluates a Ruby expression after every frame and fails the run at its first false or
nil result. `--until` stops successfully when its expression becomes true; reaching the frame
limit or quitting without satisfying it fails. Both expressions use ordinary Ruby truthiness
(0 is true). `--print` evaluates an expression every `--every` frames (60 by default) and on the
last frame, showing its Ruby `inspect` value with a one-based frame number. Values and total
output are capped; increase `--every` to keep a longer run concise.

Expressions run in the game's interpreter with its script timeout, and can call game APIs.
Use them to read state: a mutation or a call to `rand` changes the session too. The order is
input, game frame, assertion, periodic print, stop condition. On an early stop, a final print
runs after the stop condition. A syntax error, exception or timeout in any expression fails the
run with its frame and option. The command exits 0 on success, 1 for a failed run, and 2 for
invalid options or an invalid input file. Input currently covers keyboard state only.

## Object identities

An object's `#id` identifies that object across saves, renames and changes to its parent. Removing
another object does not renumber it. Objects made, duplicated, pasted or imported in the editor
get new identities; undo and redo restore the same identities with the objects.

A scene that has never been saved by the editor starts with numeric identities (`#0`, `#1`, and
so on). Its next save persists them in the scene. New objects use compact hexadecimal identities.
Always take an identity from `redgem scene list` or the result of `scene add`, and pass it back
unchanged; it is not an array index. Names and hierarchy paths remain valid selectors too.

The file keeps identities in the private `REDGEM_object_id` glTF extension, with an `id` string
on each node. It is listed in `extensionsUsed` so other glTF tools can ignore it. The engine
repairs invalid or duplicate identities with a warning. An external exporter that drops this
extension creates a scene without those persistent identities; list its objects again before
editing it. Identical names in different scenes do not identify the same object.

Before editing a scene, keep a copy of its `.glb`. Afterwards, use
`redgem scene diff . --against /path/to/before.glb` to inspect the changes by persistent identity.
It reports object additions, removals and changed fields without writing either file. `--scene`
chooses another scene and `--limit` caps the details; see [the command line](command-line.md).

## The Agent panel

The editor has a chat with a coding agent: the **Agent** tab beside the Inspector (View > Agent
shows or hides it). You talk to the agent there as you would in its own terminal chat. It works in
the game's folder, and it gets the editor's MCP tools ([MCP tools](mcp.md#live-editor)), so it can
list, select, move, add and remove objects in the open scene, play and stop the game, read the
editor's events and errors, and look at the viewport.

The editor speaks the [Agent Client Protocol](https://agentclientprotocol.com) (ACP) to agents. Each
agent is its own program, an open-source adapter that the editor starts:

| Agent | Adapter | Install |
|-------|---------|---------|
| Claude Code | `claude-agent-acp` | `npm i -g @agentclientprotocol/claude-agent-acp` |
| Codex | `codex-acp` | `npm i -g @agentclientprotocol/codex-acp` |

Both need [Node.js](https://nodejs.org). If the adapter is not installed but `npx` is, the editor
runs it with `npx --yes <package>`, which downloads it the first time. If neither is there, the
panel says what to install.

The adapters log in the way their own command line tools do, and the editor never asks for a
password. Log in once in a terminal (`claude`, then `/login`; or `codex login`, or an
`OPENAI_API_KEY`), then start a new session in the panel. If an agent asks for a login, the panel
shows its message and how to log in.

### Using it

1. Pick an agent at the top of the panel. Your first message starts a session (the **+** button
   starts a new one).
2. Write in the box at the bottom. **Enter** sends, **Shift+Enter** starts a new line.
3. The answer streams in as markdown. Code blocks have a **Copy** button. Thinking is folded under
   *Thinking*, and each tool call is a card with its status; open it to see its input, its output
   and file changes as `-`/`+` lines. The agent's plan shows as a checklist.
4. Before the agent acts, it may ask: the question shows its options (allow once, allow always,
   reject). **Stop** ends the turn and answers any open question "cancelled".
5. The paperclip attaches the selected objects to your next message, as `redgem scene get`
   describes them, so you can ask about "these" without naming them.

The agent reads and writes files through the editor only inside the game's folder. Paths that
leave it are refused, and so are paths through symbolic links. Its first session turns on the
editor's local bridge ([MCP tools](mcp.md#live-editor)) and hands the agent
`redgem mcp <game> --editor <connection>` as its MCP server. Its edits to the scene are undo steps
like yours, and they stay unsaved until you or the agent save.

### Your own agents

The gear button opens the list of agents. Each one has a name, a command, arguments (one per line)
and environment variables (`NAME=value`). Any program that speaks ACP over its standard streams can
be added. The list is kept in `agents.json` in your RedGem settings folder (`~/.config/redgem/` on
Linux). Variables are stored there as plain text, so an API key there can be read by anyone who can
read that file. **Restore** brings back the Claude Code and Codex presets.
