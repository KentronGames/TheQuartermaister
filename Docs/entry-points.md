# Two entry points, one implementation

Every operation this plugin performs lives in the `TheQuartermaster` module as a
plain C++ class. Nothing decides anything in an entry point. That is the rule
the two entry points exist under, and it is what keeps them from drifting: a
change to quarantine, closure, lint, placement or explanation is one change, and
both callers see it.

| | Commandlet | Toolset |
| --- | --- | --- |
| Module | `TheQuartermaster` | `TheQuartermasterToolset` |
| Started by | `-run=TheQuartermaster` | a live editor, through the toolset registry |
| Process | one cold launch per operation | one long-lived editor, many calls |
| Result | log markers (`THEQM_OK` / `THEQM_FAIL`) and an exit code | structured data |
| Needs | nothing beyond this plugin | the `ToolsetRegistry` engine plugin |

## When to use the commandlet

Use it whenever the answer has to be **evidence**. A fresh process reads the
project from disk with no accumulated editor state, exits with a code a script
can branch on, and leaves a log a person can re-read a week later. Verification
gates, CI, and any step whose result is going to be quoted as proof belong here.

It is also the fallback that always works: the commandlet has no dependency
outside this plugin, so a client project that has not enabled an experimental
engine plugin still gets every operation.

The cost is a cold editor launch per operation, and a commandlet's world is not
a full editor world — operations that need one (anything touching World
Partition) cannot run here at all.

## When to use the toolset

Use it when the work is a **conversation**: describe the structure, ask where
something belongs, ask what an existing path means, look at the answer, ask the
next question. Each call costs nothing beyond the call, and the result arrives
as data rather than as text to be parsed back out of a log.

It is also the only entry point running inside a real editor world, which is
what makes the operations a commandlet cannot host possible at all.

The cost is that a long-lived editor accumulates state. An answer from a session
that has been running for hours is a claim, not evidence; when it matters,
re-run the same operation as a commandlet and quote that.

## Asking the structure

Three questions, and the project's taxonomy file answers all of them:

- **`DescribeStructure`** — the content root, the contexts, the name-prefix
  table and the folder rule for each context. Ask this first; the rules name the
  exact facts the next question needs.
- **`PlaceAsset`** — where an asset belongs. Answers *placed*, *needs-facts*
  with the complete closed list of what is still missing, or *rejected*. Never a
  guess.
- **`ExplainPath`** — what an existing path already means: which rule it
  satisfies and what its segments stand for. Answers *matched*, *ambiguous* when
  the taxonomy lets two rules fit equally, or *unmatched*. An unmatched path is
  a finding about the content, not a failure — a rule that "nearly" fits would
  turn unmigrated content into compliant content on paper.

The same three are available from the commandlet as `-Describe`, `-Place=` and
`-Explain=`.

## Pointing the plugin at a taxonomy

`PlacementConfigPath` in the project's Editor settings names the file. A
relative path is resolved against the project directory. It ships empty and has
no default: a guessed taxonomy answers confidently with another project's
structure, and nothing in the answer would say so.

`-Config=<file>` overrides it for one commandlet run, for a lane checking a file
the project has not adopted yet.
