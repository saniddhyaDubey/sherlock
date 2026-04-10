import { useState, useMemo } from "react";
import { X, Search, Copy, Check } from "lucide-react";
import { Transaction, HeuristicKey } from "@/types/analysis";
import { ClassificationBadge } from "./ClassificationBadge";

interface TransactionModalProps {
  heuristicKey: HeuristicKey;
  heuristicName: string;
  transactions: Transaction[];
  onClose: () => void;
}

const PAGE_SIZE = 20;

export function TransactionModal({ heuristicKey, heuristicName, transactions, onClose }: TransactionModalProps) {
  const [search, setSearch] = useState("");
  const [page, setPage] = useState(0);
  const [copiedTxid, setCopiedTxid] = useState<string | null>(null);

  const filtered = useMemo(() => {
    if (!search) return transactions;
    return transactions.filter((tx) => tx.txid.includes(search.toLowerCase()));
  }, [transactions, search]);

  const totalPages = Math.ceil(filtered.length / PAGE_SIZE);
  const pageData = filtered.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE);

  const copyTxid = (txid: string) => {
    navigator.clipboard.writeText(txid);
    setCopiedTxid(txid);
    setTimeout(() => setCopiedTxid(null), 1500);
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-background/80 backdrop-blur-sm">
      <div className="surface-card w-full max-w-4xl max-h-[90vh] flex flex-col m-2 sm:m-4">
        {/* Header */}
        <div className="flex items-center justify-between p-4 sm:p-6 border-b border-border">
          <div>
            <p className="text-label">HEURISTIC // {heuristicKey.toUpperCase()}</p>
            <h2 className="text-lg font-medium tracking-tight mt-1">{heuristicName}</h2>
            <p className="text-sm text-muted-foreground mt-1">
              {filtered.length} transaction{filtered.length !== 1 ? "s" : ""}
            </p>
          </div>
          <button onClick={onClose} className="text-muted-foreground hover:text-foreground transition-colors">
            <X className="w-5 h-5" />
          </button>
        </div>

        {/* Search */}
        <div className="px-4 sm:px-6 py-3 border-b border-border">
          <div className="relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-muted-foreground" />
            <input
              type="text"
              placeholder="SEARCH_TXID..."
              value={search}
              onChange={(e) => { setSearch(e.target.value); setPage(0); }}
              className="w-full bg-secondary border border-border pl-10 pr-4 py-2 text-sm font-mono-data placeholder:text-muted-foreground focus:outline-none focus:border-foreground transition-colors"
            />
          </div>
        </div>

        {/* List */}
        <div className="flex-1 overflow-y-auto p-4 sm:p-6 space-y-2">
          {pageData.map((tx) => {
            const hData = tx.heuristics[heuristicKey as keyof typeof tx.heuristics];
            return (
              <div key={tx.txid} className="bg-secondary p-4 space-y-2">
                <div className="flex items-center gap-3">
                  <button
                    onClick={() => copyTxid(tx.txid)}
                    className="flex items-center gap-2 font-mono-data text-xs truncate flex-1 hover:text-muted-foreground transition-colors text-left"
                    title="Copy TXID"
                  >
                    {copiedTxid === tx.txid ? (
                      <Check className="w-3 h-3 text-[hsl(var(--simple-payment))] shrink-0" />
                    ) : (
                      <Copy className="w-3 h-3 shrink-0" />
                    )}
                    {tx.txid}
                  </button>
                  <ClassificationBadge classification={tx.classification} />
                </div>

                {/* Heuristic-specific fields */}
                {hData && (
                  <div className="flex flex-wrap gap-x-6 gap-y-1 text-xs text-muted-foreground">
                    {Object.entries(hData).map(([k, v]) => {
                      if (k === "detected" || v === undefined || v === null) return null;
                      const displayVal = Array.isArray(v)
                        ? v.length > 2 ? `[${v.slice(0, 2).join(", ")}…]` : `[${v.join(", ")}]`
                        : typeof v === "number" ? v.toLocaleString(undefined, { maximumFractionDigits: 4 })
                        : String(v);
                      return (
                        <span key={k}>
                          <span className="text-label mr-1">{k.toUpperCase()}:</span>
                          <span className="font-mono-data text-foreground">{displayVal}</span>
                        </span>
                      );
                    })}
                  </div>
                )}
              </div>
            );
          })}

          {pageData.length === 0 && (
            <p className="text-center text-muted-foreground py-12 text-sm font-mono-data">
              NO_RESULTS_FOUND
            </p>
          )}
        </div>

        {/* Pagination */}
        {totalPages > 1 && (
          <div className="flex items-center justify-between px-4 sm:px-6 py-3 border-t border-border">
            <button
              onClick={() => setPage(Math.max(0, page - 1))}
              disabled={page === 0}
              className="text-sm text-muted-foreground hover:text-foreground disabled:opacity-30 transition-colors font-mono-data"
            >
              ← PREV
            </button>
            <span className="text-sm font-mono-data text-muted-foreground">
              {page + 1} / {totalPages}
            </span>
            <button
              onClick={() => setPage(Math.min(totalPages - 1, page + 1))}
              disabled={page >= totalPages - 1}
              className="text-sm text-muted-foreground hover:text-foreground disabled:opacity-30 transition-colors font-mono-data"
            >
              NEXT →
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
