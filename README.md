# The Quartermaster

An editor-only Unreal plugin for taking over a project nobody has tidied in a while: hold the inventory,
store what is out of use, and issue everything to its proper place.

A quartermaster does not throw things away. Neither does this: content goes into custody, keeps working
while it is there, and comes back byte-identical if you want it back.

## What it does

**Custody.** Park a folder of content without an engine rename: it moves on disk, and a redirect built
from a marker left beside it keeps every baked-in reference resolving. Restore it and the folder returns to
where it came from, byte for byte. Delete it and it is gone for good — but not while something outside the
custody area still references it: the delete refuses and names the referencers, because the alternative is a
package importing something that no longer exists. `bForce` deletes anyway, when the dangling reference is
the outcome you want.

**Placement.** A taxonomy in JSON says where an asset of a given kind belongs and what its name should
start with. Ask it where a new asset goes; ask it the inverse — which rule, if any, accounts for a path
that already exists. A path no rule explains is the finding the whole thing exists to produce.

**Structure lint.** Runs the taxonomy over a whole content tree and reports what does not fit, by
severity. It fails closed on the rules a project marks as errors; lowering a rule to a warning still counts
it in the report, so relaxing a rule records a decision instead of hiding one.

**Dependency graph.** Compute the closure of a set of roots — everything they pull in, transitively — or
survey the top-level roots of a project: package counts, bytes, inbound referencers, outbound dependencies.
This is what tells you whether a folder is a leaf you can park or a hub half the project stands on. There is
also a text scan for references that live in strings rather than in the dependency graph, which is where
soft paths hide.

## Two entry points, one implementation

Every operation is a plain C++ class in the `TheQuartermaster` module; nothing decides anything in an entry
point. The commandlet (`-run=TheQuartermaster`) runs one operation per cold process and answers with log
markers and an exit code — that is the one to use when the answer has to be evidence a script can branch on.
The toolset answers a live editor with structured data, for interactive and agent-driven use.

The split, and when each is the right one: [entry-points.md](entry-points.md).
What the custody port is and is not proven to do: [quarantine-coverage.md](quarantine-coverage.md).

## Installing

1. Copy `TheQuartermaster` into your project's `Plugins/` folder.
2. Restart the editor and enable **The Quartermaster**.
3. Point **Quarantine Root** (Project Settings → Plugins → The Quartermaster) at a `/Game/` path of your
   choosing, and add that path to the project's `.gitignore` — custody is a transient state, never a
   committed one.
4. To use placement or the lint, point **Placement Config Path** at your own taxonomy file. It ships empty
   on purpose: there is no sensible default, and a guessed taxonomy answers confidently with someone
   else's structure. Two example taxonomies live in `Config/` to copy from.

Requires Unreal Engine 5.8. The toolset entry point additionally needs the engine's `ToolsetRegistry`
plugin; the commandlet needs nothing beyond this plugin, so it works in a project that has not enabled it.

## What it does not depend on

Engine modules only — no third-party libraries and no content. It has no built-in opinion about your
folder names, asset prefixes or content layout: every rule it applies comes from a taxonomy file you write,
and the custody root is a setting. Nothing it does reaches a packaged build; the whole plugin is editor-only.

## Licence

© 2026 Kentron Cowboys. All rights reserved.
