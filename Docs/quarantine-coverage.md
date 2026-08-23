# What the quarantine port is and is not proven to do

Its purpose is to stop "it built and the tests are green" from being mistaken
for "it is proven". Update it when coverage changes.

## Proven

| Behaviour | How |
| --- | --- |
| Move → verify → restore round trip | Automation test, and end to end on 33 real packages in OWH |
| Byte identity across the round trip | 33/33 SHA-256 hashes identical; git reported no change to Content |
| Redirect rebuilt after a restart | The live verify ran in a **fresh** editor process, so the redirect could only have come from the on-disk `.origin` marker |
| Delete removes the folder | Automation test |
| A second restore refuses instead of moving something | Automation test |
| Refusal on the quarantine root itself and on paths outside it | Automation test |
| Refusal on a non-`/Game` path and on something already parked | Automation test |
| Settings fallback when the configured root is unusable | Automation test, all four bad shapes |

## Not proven

**An outside reference resolving through the redirect.** This is the mechanism's
central promise — a parked pack keeps working because `/Game/Pack/...` still
resolves — and it is the one thing the live run did not demonstrate.
`Zombie_Pack` was chosen for being small and self-contained, so nothing outside
it pointed in. The redirect was registered and verified as *present*; no asset
was proven to *load through* it. A pack referenced from a retained map would
prove it, at proportionally more risk.

**The rollback path when the `.origin` marker cannot be written.** The code
rolls the move back and reports it, and the reporting distinguishes a clean
rollback from a failed one. Neither branch has been exercised; provoking them
needs an injected filesystem failure.

**The split-folder branch.** `MoveDirOnDisk` falls back to per-file moves when
the whole-directory move fails — across volumes, or on a partially locked tree
— and reports a folder left split between two paths if it cannot put the moved
files back. Every run so far took the fast whole-directory path, so the
fallback and its failure reporting are untested code.

**`bNeedsRestart`.** Set when a package could not be unloaded. Never observed:
every test and live run unloaded cleanly.

**Relocate-out.** `DropQuarantineBookkeepingAfterPackagesLeft` and
`HoldsNothingButTheOriginMarker` implement the third way out of quarantine —
moving packages onward rather than restoring or deleting them. Neither is
called by the commandlet or by any test. They are carried code, not working
code, until something drives them.

**Scale.** The largest exercised move is 33 packages. The packs this toolkit
exists for run to thousands, where the unload step and the registry rescans are
where cost and risk concentrate.
