# Branch history — `spectranet-demo`

**2026-06-10: this branch was repointed.** It now tracks the **aerial-compatible
JSON dialect** lineage (`main` @ `bc2fd15` + `fa187a0`) and replaced the original
2026-04/05 **runtime dual-encoding** lineage (old tip `aa46f6b`). This file
records what the old lineage contained so nothing is lost with the rewrite.

## What this branch is now

- libe3 `main` plus one commit, `fa187a0`: the JSON encoder/decoder speaks the
  NVIDIA Aerial camelCase wire dialect — flat envelope with a `"type"` key
  (PascalCase and `"data"`-wrapped forms rejected), payloads accepted as inline
  JSON / `{"__hex__": "…"}` / plain hex / UTF-8, structured `ranFunctionData`
  passed inline, plus additive optional fields (`SetupResponse.message`,
  `SubscriptionResponse` granted-list/periodicity/message echoes,
  `IndicationMessage.subscription_id`).
- Exactly one encoder per build: the spectranet deployment uses
  `-DLIBE3_ENABLE_JSON=ON -DLIBE3_ENABLE_ASN1=OFF`.
- Pairs with the `spectranet-demo` branches of `spear-openairinterface5g`
  (gNB built with `-DE3_ENCODING_FORMAT=JSON`) and of the dApp repo.

## The replaced lineage (runtime dual-encoding, old tip `aa46f6b`)

The old branch served ASN.1 (spear-dApp) and JSON (aerial dApps)
**simultaneously** over two channel sets inside one agent. It was superseded by
the build-time single-encoding model above. Its commits up to `8afc87f` remain
reachable through the branch `aerial-oai-integration`; the last six commits
existed only on the old `spectranet-demo` and are summarized here so they can
be re-implemented if ever needed:

| Commit | What it did | Still relevant? |
|---|---|---|
| `aa46f6b` | Reliable gNB→dApp xApp-control relay: per-message ack + retransmit on the agent side | The current stack relays xApp controls fire-and-forget; the dApp side has its own ack/retransmit for dApp→gNB. Re-implement if xApp→dApp delivery must be guaranteed under load. |
| `088b03d` | Link `libe3_sanitizers` PUBLIC so consumers inherit the flags | Build hygiene; re-do if sanitizer builds return. |
| `454874d` | Rate-limit + clarify malformed-setup warnings in the setup loop | Cosmetic; logs can flood if a wrong-encoding dApp hammers setup. |
| `5449543` | **Send an empty reply on setup-decode failure so the ZMQ REP socket never wedges** | The known sharp edge of `main`: a malformed/wrong-encoding setupRequest leaves the REP socket stuck (reply expected before next recv). Re-implement this if mixed-encoding dApps ever share a deployment. |
| `b458ba4` | Reduce SubscriptionManager reader pressure under emit load | Possibly superseded by `main`'s multi-dApp fan-out rework; re-evaluate before porting. |
| `774a043` | Per-dApp encoding query API for SMs | Dual-encoding only; obsolete under the single-encoding model. |

Also notable inside `aerial-oai-integration` (still reachable there):
`e3fa80c` — ASN.1 setupResponse encode fails when an SM registers with empty
`ranFunctionData`; substituted a 1-byte placeholder. Needed only on **ASN1**
builds; current mitigation is to enable only SMs that provide
`ranFunctionData`. `5dbacba` — accept plain hex-string `actionData`
(now part of the `fa187a0` payload handling). `383ae08` — SubscriptionDetails
use-after-free fix (superseded by the `json-format` branch's SubscriptionDetails
rework, `ea62ba9`).

## Related branches

- `json-format`: an independent camelCase/flat-envelope implementation plus a
  SubscriptionDetails C API. Largely overlaps `fa187a0`; its gaps for Aerial
  compatibility are `ranFunctionData` always hex-encoded, no
  `IndicationMessage.subscription_id`, no subscriptionResponse echoes, and a
  strict `json::parse` on indication payloads. Natural upstream target: merge
  it to `main`, then layer the `fa187a0` deltas on top.
