import { useState } from "react";

export function HowToUse() {
  const [showHelp, setShowHelp] = useState(false);

  return (
    <div className="surface-card p-4 mb-4">
      <button
        onClick={() => setShowHelp(!showHelp)}
        className="flex items-center gap-2 text-sm font-mono-data text-muted-foreground hover:text-foreground transition-colors"
      >
        <span>{showHelp ? "▼" : "▶"}</span>
        <span>HOW_TO_USE</span>
      </button>
      {showHelp && (
        <div className="mt-4 space-y-3 text-sm text-muted-foreground font-mono-data border-t border-border pt-4">
          <div>
            <p className="text-foreground font-medium mb-1">MODE_1: Pre-analyzed data (instant)</p>
            <p>Use the dropdown in the top-right to select a pre-analyzed block file (blk04330.dat or blk05051.dat). Results load instantly from committed analysis.</p>
          </div>
          <div>
            <p className="text-foreground font-medium mb-1">MODE_2: Live analysis (upload your own)</p>
            <p>Upload your own raw Bitcoin block files (.dat format) using the three file inputs below. Click RUN_ANALYSIS to trigger the full C++ chain analysis engine in real time. Analysis takes 30-60 seconds depending on block size.</p>
          </div>
          <div>
            <p className="text-foreground font-medium mb-1">EXPLORING_RESULTS</p>
            <p>Use the top tabs to navigate: Overview → block stats, Explorer → per-block breakdown, Heuristics → interactive heuristic lab with transaction drill-down.</p>
          </div>
          <div>
            <p className="text-foreground font-medium mb-1">PEELING_CHAIN_GRAPH</p>
            <p>Available in MODE_2 only — peeling chain data requires live analysis. After uploading and running analysis, scroll down in the Heuristics tab to the Peeling Chains section. Click GRAPH on any chain to open an interactive graph. Click the CHANGE node to expand hop by hop.</p>
          </div>
        </div>
      )}
    </div>
  );
}
