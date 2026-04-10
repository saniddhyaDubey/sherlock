import { useState } from "react";
import { Block } from "@/types/analysis";
import { ChevronDown, ChevronRight } from "lucide-react";
import { ClassificationBadge } from "./ClassificationBadge";

interface BlockExplorerProps {
  blocks: Block[];
}

export function BlockExplorer({ blocks }: BlockExplorerProps) {
  const [expandedBlock, setExpandedBlock] = useState<number | null>(null);

  return (
    <div className="space-y-2 animate-fade-in">
      {/* Header — hidden on mobile, shown on sm+ */}
      <div className="hidden sm:grid surface-card p-4 grid-cols-[1fr_100px_80px_80px] gap-4 text-label">
        <span>BLOCK_HASH</span>
        <span>HEIGHT</span>
        <span>TX_COUNT</span>
        <span>FLAGGED</span>
      </div>

      {blocks.map((block, i) => {
        const isExpanded = expandedBlock === i;
        return (
          <div key={block.block_hash} className="surface-card-hover">
            {/* Row — stacked on mobile, grid on sm+ */}
            <button
              onClick={() => setExpandedBlock(isExpanded ? null : i)}
              className="w-full p-4 text-left"
            >
              {/* Mobile layout */}
              <div className="flex items-start gap-2 sm:hidden">
                {isExpanded ? (
                  <ChevronDown className="w-4 h-4 text-muted-foreground shrink-0 mt-0.5" />
                ) : (
                  <ChevronRight className="w-4 h-4 text-muted-foreground shrink-0 mt-0.5" />
                )}
                <div className="min-w-0 flex-1 space-y-1">
                  <p className="font-mono-data text-xs truncate text-muted-foreground">{block.block_hash}</p>
                  <div className="flex gap-4 text-sm">
                    <span className="font-mono-data">#{block.block_height.toLocaleString()}</span>
                    <span className="text-muted-foreground">{block.tx_count.toLocaleString()} txs</span>
                    <span className="text-muted-foreground">{block.analysis_summary.flagged_transactions} flagged</span>
                  </div>
                </div>
              </div>

              {/* Desktop layout */}
              <div className="hidden sm:grid grid-cols-[1fr_100px_80px_80px] gap-4 items-center text-sm">
                <div className="flex items-center gap-2">
                  {isExpanded ? (
                    <ChevronDown className="w-4 h-4 text-muted-foreground shrink-0" />
                  ) : (
                    <ChevronRight className="w-4 h-4 text-muted-foreground shrink-0" />
                  )}
                  <span className="font-mono-data truncate">{block.block_hash}</span>
                </div>
                <span className="font-mono-data">{block.block_height.toLocaleString()}</span>
                <span className="font-mono-data">{block.tx_count.toLocaleString()}</span>
                <span className="font-mono-data">{block.analysis_summary.flagged_transactions}</span>
              </div>
            </button>

            {isExpanded && (
              <div className="border-t border-border p-4 space-y-4 animate-fade-in">
                {/* Fee Rate Stats */}
                <div>
                  <p className="text-label mb-3">FEE_RATE_STATS (sat/vB)</p>
                  <div className="grid grid-cols-2 sm:grid-cols-4 gap-2">
                    {[
                      { label: "MIN", value: block.analysis_summary.fee_rate_stats.min_sat_vb },
                      { label: "MEDIAN", value: block.analysis_summary.fee_rate_stats.median_sat_vb },
                      { label: "MEAN", value: block.analysis_summary.fee_rate_stats.mean_sat_vb },
                      { label: "MAX", value: block.analysis_summary.fee_rate_stats.max_sat_vb },
                    ].map(f => (
                      <div key={f.label} className="flex justify-between items-center px-3 py-2 bg-secondary text-sm">
                        <span className="font-mono-data text-muted-foreground">{f.label}</span>
                        <span className="font-mono-data">{f.value.toFixed(1)}</span>
                      </div>
                    ))}
                  </div>
                </div>

                <div>
                  <p className="text-label mb-3">SCRIPT_DISTRIBUTION</p>
                  <div className="grid grid-cols-2 sm:grid-cols-4 gap-2">
                    {Object.entries(block.analysis_summary.script_type_distribution || {}).map(([key, count]) => (
                      <div key={key} className="flex justify-between items-center px-3 py-2 bg-secondary text-sm">
                        <span className="font-mono-data text-muted-foreground">{key.toUpperCase()}</span>
                        <span className="font-mono-data">{count.toLocaleString()}</span>
                      </div>
                    ))}
                  </div>
                </div>

                {block.transactions && (
                  <div>
                    <p className="text-label mb-3">NOTABLE_TRANSACTIONS ({block.transactions.length})</p>
                    <div className="space-y-1 max-h-64 overflow-y-auto">
                      {block.transactions.slice(0, 20).map((tx) => (
                        <div key={tx.txid} className="flex items-center gap-2 px-3 py-2 bg-secondary text-sm">
                          <span className="font-mono-data truncate flex-1 text-xs">{tx.txid}</span>
                          <ClassificationBadge classification={tx.classification} />
                        </div>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>
        );
      })}
    </div>
  );
}
