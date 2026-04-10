import { useState } from "react";
import { PeelingChainHop } from "@/types/analysis";
import { ChevronDown, ChevronRight, Link2, GitBranch } from "lucide-react";
import { PeelingChainGraph } from "./PeelingChainGraph";

interface PeelingChainsProps {
  chains: PeelingChainHop[][];
}

export function PeelingChains({ chains }: PeelingChainsProps) {
  const [expanded, setExpanded] = useState<number | null>(null);
  const [graphChain, setGraphChain] = useState<PeelingChainHop[] | null>(null);

  if (!chains || !Array.isArray(chains) || chains.length === 0) return null;

  const sortedChains = [...chains].sort((a, b) => b.length - a.length);
  const topChains = sortedChains.slice(0, 10);
  const longestLen = topChains[0]?.length || 0;
  

  return (
    <div className="surface-card p-6 space-y-4">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-3">
          <Link2 className="w-4 h-4 text-muted-foreground" />
          <p className="text-label">PEELING_CHAINS (TOP 10)</p>
        </div>
        <span className="text-sm font-mono-data text-muted-foreground">
          {chains.length} chains detected
        </span>
      </div>

      <div className="space-y-1">
        {topChains.map((chain, i) => {
          const isExpanded = expanded === i;
          const isLongest = chain.length === longestLen;

          return (
            <div key={i}>
              <div className={`w-full flex items-center justify-between px-4 py-3 text-sm transition-colors ${
                isLongest
                  ? "bg-[hsl(var(--coinjoin)/0.05)] border border-[hsl(var(--coinjoin)/0.2)]"
                  : "bg-secondary hover:bg-accent"
              }`}>
                <button
                  onClick={() => setExpanded(isExpanded ? null : i)}
                  className="flex items-center gap-3 flex-1 text-left"
                >
                  {isExpanded ? (
                    <ChevronDown className="w-4 h-4 text-muted-foreground" />
                  ) : (
                    <ChevronRight className="w-4 h-4 text-muted-foreground" />
                  )}
                  <span className="font-mono-data">CHAIN_{String(i + 1).padStart(2, "0")}</span>
                  {isLongest && (
                    <span className="badge-coinjoin px-2 py-0.5 text-[9px] font-bold uppercase tracking-wider">
                      LONGEST
                    </span>
                  )}
                </button>
                <div className="flex items-center gap-3">
                  <span className="font-mono-data text-muted-foreground">{chain.length} hops</span>
                  <button
                    onClick={() => setGraphChain(chain)}
                    className="flex items-center gap-1 text-xs border border-border px-2 py-1 font-mono-data hover:bg-foreground hover:text-background transition-colors"
                  >
                    <GitBranch className="w-3 h-3" />
                    GRAPH
                  </button>
                </div>
              </div>

              {isExpanded && (
                <div className="border-x border-b border-border p-4 space-y-1 animate-fade-in">
                  {chain.map((hop, hi) => (
                    <div key={hi} className="flex items-center gap-4 px-3 py-2 bg-background text-xs">
                      <span className="text-muted-foreground font-mono-data w-8">#{hi + 1}</span>
                      <span className="font-mono-data truncate flex-1">{hop.txid}</span>
                      <span className="text-muted-foreground">
                        <span className="text-label mr-1">LARGE:</span>
                        <span className="font-mono-data text-foreground">
                          {(hop.large_output_value / 100000000).toFixed(4)}
                        </span>
                      </span>
                      <span className="text-muted-foreground">
                        <span className="text-label mr-1">SMALL:</span>
                        <span className="font-mono-data text-foreground">
                          {(hop.small_output_value / 100000000).toFixed(4)}
                        </span>
                      </span>
                    </div>
                  ))}
                </div>
              )}
            </div>
          );
        })}
      </div>

      {graphChain && (
        <PeelingChainGraph chain={graphChain} onClose={() => setGraphChain(null)} />
      )}
    </div>
  );
}
