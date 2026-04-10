<div align="center">

# 🔍 Sherlock

### A high-performance Bitcoin chain analysis engine

*Raw block binary parsing in C++17 · 9 privacy heuristics · Full-stack React visualizer*

[![C++17](https://img.shields.io/badge/C++-17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![Bitcoin](https://img.shields.io/badge/Bitcoin-Chain%20Analysis-F7931A?style=flat-square&logo=bitcoin&logoColor=white)](https://bitcoin.org/)
[![React](https://img.shields.io/badge/React-TypeScript-61DAFB?style=flat-square&logo=react&logoColor=black)](https://react.dev/)
[![Node.js](https://img.shields.io/badge/Node.js-Express-339933?style=flat-square&logo=node.js&logoColor=white)](https://nodejs.org/)
[![Tailwind CSS](https://img.shields.io/badge/Tailwind-CSS-06B6D4?style=flat-square&logo=tailwindcss&logoColor=white)](https://tailwindcss.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)](LICENSE)

[**Live Demo**](https://youtu.be/UB5uUEHEzj4) · [**Approach & Heuristics**](APPROACH.md) · [**Analysis Reports**](out/)

</div>

---

## What is Sherlock?

Sherlock ingests raw Bitcoin mainnet block files (`blk*.dat`, `rev*.dat`, `xor.dat`) straight off disk — no node, no RPC, no external API — and applies **9 chain-analysis heuristics** to every transaction it finds. It produces structured JSON reports, human-readable Markdown summaries, and surfaces everything through an interactive full-stack web visualizer.

The name fits: it reads the raw binary evidence and figures out what happened.

---

## Numbers that matter

| Metric | Value |
|--------|-------|
| Transactions analyzed (committed fixtures) | **341,792** |
| Heuristics applied per transaction | **9** |
| Blocks parsed per run | 84 |
| Parser language | C++17 (`-O3`) |
| External dependencies (runtime) | **0** — pure binary parsing |
| JSON output for `blk04330` | ~120 MB |

---

## Architecture

The system is split into three layers that communicate across process boundaries — the C++ engine never touches a network socket; the Node.js server is the only glue.

```
┌─────────────────────────────────────────────────────────────────┐
│                         USER / BROWSER                          │
│              React + TypeScript + Tailwind + shadcn/ui          │
│   ┌──────────────┐  ┌───────────────────┐  ┌─────────────────┐  │
│   │   Overview   │  │  Block Explorer   │  │ Heuristics Lab  │  │
│   │  (stats +    │  │  (block → tx      │  │ (9-heuristic    │  │
│   │   charts)    │  │   drill-down)     │  │  grid + chains) │  │
│   └──────────────┘  └───────────────────┘  └─────────────────┘  │
└────────────────────────────┬────────────────────────────────────┘
                             │  HTTP/JSON  (REST API)
┌────────────────────────────▼────────────────────────────────────┐
│                    Node.js  +  Express                           │
│                                                                  │
│   GET  /api/results/:stem  →  serve committed out/*.json         │
│   POST /api/analyze        →  receive .dat uploads via multer    │
│                               spawn C++ binary as subprocess     │
│                               stream stdout → parse → respond    │
└────────────────────────────┬────────────────────────────────────┘
                             │  child_process.spawn()
                             │  stdin: /dev/null
                             │  stdout: JSON (streamed)
                             │  stderr: progress / errors
┌────────────────────────────▼────────────────────────────────────┐
│                      C++17  Binary  (block_parser)               │
│                                                                  │
│  ① XOR deobfuscation  (xor.dat key applied to blk + rev)         │
│  ② Rev record parsing  →  prevout UTXO lookup table              │
│  ③ Block iteration     →  80-byte header, coinbase BIP34 height  │
│  ④ Transaction parsing →  inputs, outputs, witness data          │
│  ⑤ Script classification → P2PKH / P2SH / P2WPKH / P2WSH /      │
│                             P2TR / OP_RETURN / unknown           │
│  ⑥ Address derivation  →  Base58Check + Bech32 / Bech32m        │
│  ⑦ Fee + weight        →  sat/vB computed from prevout values    │
│  ⑧ 9 heuristics        →  applied per-transaction, O(n)          │
│  ⑨ JSON + Markdown     →  written to out/                        │
└─────────────────────────────────────────────────────────────────┘
```

### How C++ talks to React

The bridge is dead simple and intentionally so. When a user uploads `.dat` files in the browser:

1. **React** `POST`s a `multipart/form-data` request to `/api/analyze`
2. **Express** saves the files to a temp directory via `multer`, then calls:
   ```js
   const proc = child_process.spawn('./block_parser', ['--ui', blkPath, revPath, xorPath])
   ```
3. **block_parser** writes a single JSON object to `stdout` and exits `0` or `1`
4. **Express** buffers stdout, parses the JSON, and returns it as the HTTP response
5. **React** receives the typed JSON and renders it — no page reload, no polling

For pre-committed data (`/api/results`), the server just reads from `out/` — the C++ engine already ran at development time and the results are shipped with the repo.

---

## Heuristics

All 9 heuristics run in a **single O(n) pass** per transaction. Each is independent — redundancy is accepted for clarity and debuggability.

| # | ID | What it detects | Confidence model |
|---|-----|----------------|-----------------|
| 1 | `cioh` | Common Input Ownership — multi-input txs likely controlled by one entity | Logarithmic (scales with input count) |
| 2 | `change_detection` | Likely change output via script-type match, round-number analysis, output index | Additive (methods stack) |
| 3 | `consolidation` | Many inputs (≥5) + few outputs (<3) of the same script type | Binary + script consistency check |
| 4 | `address_reuse` | Same address appearing in inputs and outputs of the same or adjacent txs | Binary per match |
| 5 | `self_transfer` | All inputs/outputs same script type, no round-number payment component | Binary |
| 6 | `coinjoin` | 3+ inputs, 2+ equal-value outputs, 3+ unique addresses | Multi-condition threshold |
| 7 | `op_return` | OP_RETURN outputs — classified by protocol (Omni, OpenTimestamps, …) | Protocol fingerprint match |
| 8 | `round_number_payment` | Outputs divisible by 100,000 sats — payment vs change signal | Divisibility check |
| 9 | `peeling_chain` | Cross-block 1-in-2-out chains where the larger output is spent in the next tx | UTXO graph traversal |

Transaction classification priority (highest wins):

```
coinjoin  >  consolidation  >  self_transfer  >  batch_payment  >  simple_payment  >  unknown
```

Confidence is never uniform — each heuristic has its own model. See [APPROACH.md](APPROACH.md) for the full reasoning, limitations, and known false-positive cases.

---

## Tech stack

```
┌── C++17 ─────────────────────────────────────────────────────┐
│   parser.cpp      Raw binary block/transaction parsing        │
│   analyzer.cpp    9 heuristics + JSON + Markdown output       │
│   bech32.cpp      Bech32 / Bech32m address encoding           │
│   opcodes.h       Bitcoin script opcode classification        │
│   picosha2.h      SHA256 for txid / block hash computation    │
│   json.hpp        nlohmann/json — single-header serialization │
│   utf8.h          OP_RETURN data UTF-8 validation             │
└───────────────────────────────────────────────────────────────┘
┌── Node.js + Express ─────────────────────────────────────────┐
│   server.js       REST API, subprocess bridge, file upload    │
│   multer          Multipart .dat file handling                │
└───────────────────────────────────────────────────────────────┘
┌── React + TypeScript ────────────────────────────────────────┐
│   Vite            Build tooling                               │
│   Tailwind CSS    Utility-first styling                       │
│   shadcn/ui       Component primitives                        │
│   Recharts        Fee distribution + script type charts       │
│   Components      Overview · BlockExplorer · HeuristicsLab    │
│                   PeelingChainGraph · TransactionModal        │
└───────────────────────────────────────────────────────────────┘
```

---

## Project structure

```
sherlock/
├── src/
│   ├── main.cpp          Entry point, arg parsing, orchestration
│   ├── parser.cpp        XOR deobfuscation, block/tx binary parsing
│   ├── analyzer.cpp      9 heuristics, aggregation, JSON + MD output
│   ├── bech32.cpp/h      SegWit + Taproot address encoding
│   └── opcodes.h         Script classification constants
├── lib/
│   ├── picosha2.h        Header-only SHA256
│   ├── json.hpp          nlohmann/json (single header)
│   └── utf8.h            UTF-8 validation
├── web/
│   ├── server.js         Express backend (94 lines)
│   └── client/           React frontend (Vite + TS + Tailwind)
│       ├── Overview.tsx
│       ├── BlockExplorer.tsx
│       ├── HeuristicsLab.tsx
│       ├── PeelingChainGraph.tsx
│       ├── TransactionModal.tsx
│       └── FileUpload.tsx
├── tests/
│   ├── test_heuristics.cpp
│   ├── test_change_detection.cpp
│   ├── test_address_reuse.cpp
│   └── test_peeling_chain.cpp
├── fixtures/             blk*.dat.gz  rev*.dat.gz  xor.dat.gz
├── out/                  Committed JSON + Markdown reports
│   ├── blk04330.json     84 blocks · 341,792 transactions
│   ├── blk04330.md
│   ├── blk05051.json
│   └── blk05051.md
├── setup.sh              Decompress fixtures, compile C++, install npm
├── cli.sh                CLI wrapper
└── web.sh                Start web server
```

---

## Getting started

### 1. Setup (once)

```bash
./setup.sh
```

This decompresses the Bitcoin block fixtures, compiles the C++17 binary with `-O3`, installs Node.js dependencies, and builds the React bundle. No CMake, no package manager for C++.

### 2. CLI analysis

```bash
./cli.sh --block fixtures/blk04330.dat fixtures/rev04330.dat fixtures/xor.dat
```

Outputs:
- `out/blk04330.json` — full machine-readable analysis
- `out/blk04330.md` — human-readable Markdown report

Error output is always structured JSON:
```json
{ "ok": false, "error": { "code": "FILE_NOT_FOUND", "message": "..." } }
```

### 3. Web visualizer

```bash
./web.sh
# → http://127.0.0.1:3000
```

Open the URL. You can explore the pre-committed analysis instantly or upload your own `.dat` files for live parsing.

---

## JSON output shape

```jsonc
{
  "ok": true,
  "mode": "chain_analysis",
  "file": "blk04330.dat",
  "block_count": 84,
  "analysis_summary": {
    "total_transactions_analyzed": 341792,
    "heuristics_applied": ["cioh", "change_detection", "consolidation",
                           "address_reuse", "self_transfer", "coinjoin",
                           "op_return", "round_number_payment", "peeling_chain"],
    "flagged_transactions": 327089,
    "script_type_distribution": { "p2wpkh": 198432, "p2tr": 74201, ... },
    "fee_rate_stats": { "min_sat_vb": 1.0, "median_sat_vb": 28.0, "max_sat_vb": 800.0 }
  },
  "blocks": [
    {
      "block_hash": "000000000000...",
      "block_height": 800000,
      "tx_count": 3891,
      "transactions": [
        {
          "txid": "a1b2c3...",
          "heuristics": {
            "cioh": { "detected": true },
            "change_detection": { "detected": true, "likely_change_index": 1,
                                  "method": "script_type_match", "confidence": "high" }
          },
          "classification": "simple_payment"
        }
      ]
    }
  ]
}
```

---

## Design decisions worth reading

- **No CMake, no Conan.** The C++ build is a single compiler invocation in `setup.sh`. Every header is bundled. Zero dependency resolution at build time.
- **Single-pass heuristics.** All 9 heuristics + all stats are computed in one traversal — O(n) in transaction count, not O(9n).
- **Peeling chains are cross-block.** The UTXO tracker persists across block boundaries within a file run, catching chains that span multiple blocks.
- **Transactions array scoped to blocks[0].** Writing full per-transaction data for every block would push JSON output past 1 GB. Aggregated summaries cover all blocks; the first block gets full detail for interactive exploration.
- **Conservative thresholds everywhere.** False negatives are preferred over false positives. Chain analysis is probabilistic — overclaiming is worse than missing a pattern.
- **subprocess over FFI.** The Node.js server spawns the C++ binary as a child process rather than binding it via N-API or WASM. This keeps the C++ build dead simple, makes the boundary explicit, and means the analyzer can be used standalone from the CLI without any JS runtime present.

---

## References

- Meiklejohn et al. — *A Fistful of Bitcoins: Characterizing Payments Among Men with No Names* (2013)
- Bitcoin Wiki — Script, Transaction, Block structure
- BIP 141 (SegWit), BIP 173 (Bech32), BIP 341 (Taproot), BIP 34 (Block height in coinbase)
- Chainalysis, OXT Research — Public heuristic documentation
- CryptoQuant — Fee rate analysis methodology

---

<div align="center">

Built by [Saniddhya Dubey](https://github.com/saniddhyadubey)

</div>
