import { useState, useMemo } from "react";
import { motion, AnimatePresence } from "framer-motion";
import {
  Users, ArrowLeftRight, RotateCcw, Shuffle, Layers, RefreshCw,
  Link2, FileCode, CircleDollarSign,
} from "lucide-react";
import { AnalysisResult, HeuristicKey, Transaction } from "@/types/analysis";
import { ClassificationBadge } from "./ClassificationBadge";
import { TransactionModal } from "./TransactionModal";
import { PeelingChains } from "./PeelingChains";

interface HeuristicsLabProps {
  data: AnalysisResult;
}

interface HeuristicDef {
  key: HeuristicKey;
  name: string;
  description: string;
  icon: typeof Users;
}

const heuristics: HeuristicDef[] = [
  { key: "cioh", name: "Common Input Ownership", description: "Identifies transactions where multiple inputs likely belong to the same entity based on co-spending patterns.", icon: Users },
  { key: "change_detection", name: "Change Detection", description: "Detects likely change outputs using script type matching, value analysis, and output position heuristics.", icon: ArrowLeftRight },
  { key: "address_reuse", name: "Address Reuse", description: "Flags transactions that reuse previously seen addresses, reducing privacy guarantees.", icon: RotateCcw },
  { key: "coinjoin", name: "CoinJoin Pattern", description: "Detects collaborative mixing transactions with equal-value outputs designed to obscure transaction graphs.", icon: Shuffle },
  { key: "consolidation", name: "Consolidation", description: "Identifies fan-in transactions where many inputs consolidate to a single output, typical of exchange UTXOs.", icon: Layers },
  { key: "self_transfer", name: "Self Transfer", description: "Detects transactions where inputs and outputs likely belong to the same wallet entity.", icon: RefreshCw },
  { key: "peeling_chain", name: "Peeling Chain", description: "Identifies chains of transactions that peel off small amounts, common in automated payment systems.", icon: Link2 },
  { key: "op_return", name: "OP_RETURN Usage", description: "Flags transactions containing OP_RETURN data outputs used for embedding arbitrary data on-chain.", icon: FileCode },
  { key: "round_num_payments", name: "Round Number Payment", description: "Detects outputs with suspiciously round values that may indicate the payment output vs change.", icon: CircleDollarSign },
];

function getHeuristicCounts(data: AnalysisResult): Record<string, number> {
  const counts: Record<string, number> = {};
  if (!data?.blocks) return counts;
  data.blocks.forEach(block => {
    if (!block?.transactions) return;
    block.transactions.forEach(tx => {
      if (!tx?.heuristics) return;
      Object.entries(tx.heuristics || {}).forEach(([key, val]) => {
        if (val && (val as any).detected) {
          counts[key] = (counts[key] || 0) + 1;
        }
      });
    });
  });
  return counts;
}

function getTransactionsForHeuristic(data: AnalysisResult, key: HeuristicKey): Transaction[] {
  const txs: Transaction[] = [];
  if (!data?.blocks) return txs;
  data.blocks.forEach(block => {
    if (!block?.transactions) return;
    block.transactions.forEach(tx => {
      if (!tx?.heuristics) return;
      const h = tx.heuristics[key as keyof typeof tx.heuristics];
      if (h && (h as any).detected) txs.push(tx);
    });
  });
  return txs;
}

export function HeuristicsLab({ data }: HeuristicsLabProps) {
  console.log("data:", data);
  console.log("peeling_chains:", data?.peeling_chains);

  const [selected, setSelected] = useState<HeuristicKey | null>(null);
  const [modalHeuristic, setModalHeuristic] = useState<HeuristicKey | null>(null);

  const counts = useMemo(() => getHeuristicCounts(data), [data]);

  const selectedDef = heuristics.find((h) => h.key === selected);
  const selectedTxs = useMemo(
    () => (selected ? getTransactionsForHeuristic(data, selected) : []),
    [data, selected]
  );

  const modalTxs = useMemo(
    () => (modalHeuristic ? getTransactionsForHeuristic(data, modalHeuristic) : []),
    [data, modalHeuristic]
  );

  return (
    <div className="space-y-6 animate-fade-in">
      <AnimatePresence mode="wait">
        {!selected ? (
          <motion.div
            key="grid"
            className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.2 }}
          >
            {heuristics.map((h) => (
              <HeuristicCard
                key={h.key}
                heuristic={h}
                count={counts[h.key] || 0}
                onClick={() => setSelected(h.key)}
              />
            ))}
          </motion.div>
        ) : (
          <motion.div
            key="expanded"
            className="grid grid-cols-1 lg:grid-cols-[2fr_1fr] gap-4"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.2 }}
          >
            {/* Expanded card */}
            <motion.div
              layout
              className="surface-card p-6 space-y-6"
              initial={{ opacity: 0, scale: 0.98 }}
              animate={{ opacity: 1, scale: 1 }}
              transition={{ type: "spring", stiffness: 300, damping: 30 }}
            >
              <div className="flex items-start justify-between">
                <div className="space-y-1">
                  <p className="text-label">HEURISTIC</p>
                  <h3 className="text-2xl font-medium tracking-tight">{selectedDef?.name}</h3>
                </div>
                <button
                  onClick={() => setSelected(null)}
                  className="text-muted-foreground hover:text-foreground transition-colors text-sm border border-border px-3 py-1"
                >
                  CLOSE
                </button>
              </div>

              <p className="text-sm text-muted-foreground leading-relaxed">{selectedDef?.description}</p>

              <div className="flex gap-6 flex-wrap">
                <div>
                  <p className="text-label">FLAGGED</p>
                  <p className="text-4xl font-mono-data font-light tracking-tighter mt-1">
                    {(counts[selected] || 0).toLocaleString()}
                  </p>
                </div>
                <div>
                  <p className="text-label">SAMPLE_SIZE</p>
                  <p className="text-4xl font-mono-data font-light tracking-tighter mt-1">
                    {(selectedTxs || []).length}
                  </p>
                </div>
              </div>

              {/* Top 5 examples */}
              <div>
                <p className="text-label mb-3">TOP_EXAMPLES</p>
                <div className="space-y-2">
                  {(selectedTxs || []).slice(0, 5).map((tx) => (
                    <TransactionRow key={tx.txid} tx={tx} heuristicKey={selected} />
                  ))}
                </div>
              </div>

              <button
                onClick={() => setModalHeuristic(selected)}
                className="border border-foreground px-4 py-2 text-sm font-medium hover:bg-foreground hover:text-background transition-colors"
              >
                VIEW_ALL →
              </button>
            </motion.div>

            {/* Sidebar cards */}
            <div className="space-y-2 overflow-y-auto max-h-[400px] lg:max-h-[700px]">
              {heuristics
                .filter((h) => h.key !== selected)
                .map((h) => (
                  <HeuristicCardMini
                    key={h.key}
                    heuristic={h}
                    count={counts[h.key] || 0}
                    onClick={() => setSelected(h.key)}
                  />
                ))}
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Peeling Chains */}
      {data.peeling_chains && Array.isArray(data.peeling_chains) && data.peeling_chains.length > 0 && (
        <PeelingChains chains={data.peeling_chains} />
      )}

      {/* Transaction Modal */}
      {modalHeuristic && (
        <TransactionModal
          heuristicKey={modalHeuristic}
          heuristicName={heuristics.find((h) => h.key === modalHeuristic)?.name || ""}
          transactions={modalTxs}
          onClose={() => setModalHeuristic(null)}
        />
      )}
    </div>
  );
}

function HeuristicCard({
  heuristic,
  count,
  onClick,
}: {
  heuristic: HeuristicDef;
  count: number;
  onClick: () => void;
}) {
  const Icon = heuristic.icon;
  return (
    <motion.button
      onClick={onClick}
      className="surface-card-hover p-6 text-left cursor-pointer"
      whileHover={{ borderColor: "hsl(0 0% 30%)" }}
    >
      <div className="flex justify-between items-start">
        <div className="space-y-1">
          <p className="text-label">HEURISTIC</p>
          <h3 className="text-lg font-medium tracking-tight">{heuristic.name}</h3>
        </div>
        <Icon className="w-5 h-5 text-muted-foreground" />
      </div>
      <div className="mt-8">
        <span className="text-4xl font-mono-data font-light tracking-tighter">
          {count.toLocaleString()}
        </span>
        <p className="text-xs text-muted-foreground mt-1">Flagged Transactions</p>
      </div>
    </motion.button>
  );
}

function HeuristicCardMini({
  heuristic,
  count,
  onClick,
}: {
  heuristic: HeuristicDef;
  count: number;
  onClick: () => void;
}) {
  const Icon = heuristic.icon;
  return (
    <button
      onClick={onClick}
      className="w-full surface-card-hover p-4 text-left flex items-center justify-between"
    >
      <div className="flex items-center gap-3">
        <Icon className="w-4 h-4 text-muted-foreground" />
        <span className="text-sm font-medium">{heuristic.name}</span>
      </div>
      <span className="font-mono-data text-sm text-muted-foreground">{count.toLocaleString()}</span>
    </button>
  );
}

function TransactionRow({ tx, heuristicKey }: { tx: Transaction; heuristicKey: HeuristicKey }) {
  const hData = tx.heuristics[heuristicKey as keyof typeof tx.heuristics];
  const confColors: Record<string, string> = {
    low: "border-red-500/30 bg-red-500/10 text-red-400",
    medium: "border-yellow-500/30 bg-yellow-500/10 text-yellow-400",
    high: "border-green-500/30 bg-green-500/10 text-green-400",
    very_high: "border-emerald-500/30 bg-emerald-500/10 text-emerald-400",
    unreliable: "border-gray-500/30 bg-gray-500/10 text-gray-400",
  };

  return (
    <div className="flex items-center gap-3 px-4 py-3 bg-secondary text-sm">
      <span className="font-mono-data truncate flex-1 text-xs">{tx.txid}</span>
      <ClassificationBadge classification={tx.classification} />
      {hData && "confidence" in hData && (hData as { confidence?: string }).confidence && (
        <span className={`px-2 py-0.5 text-[9px] font-bold uppercase tracking-wider border ${confColors[(hData as { confidence: string }).confidence] || "border-border bg-secondary text-foreground"}`}>
          {(hData as { confidence: string }).confidence}
        </span>
      )}
    </div>
  );
}
