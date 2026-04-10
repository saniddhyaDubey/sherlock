import { useState } from "react";
import { PeelingChainHop } from "@/types/analysis";

interface Props {
  chain: PeelingChainHop[];
  onClose: () => void;
}

interface Node {
  id: string;
  x: number;
  y: number;
  label: string;
  sublabel: string;
  type: "tx" | "payment" | "change";
}

interface Edge {
  from: string;
  to: string;
  label: string;
}

const NODE_W = 220;
const NODE_H = 60;
const V_GAP = 120;
const H_GAP = 280;

export function PeelingChainGraph({ chain, onClose }: Props) {
  const [revealed, setRevealed] = useState(1);

  const nodes: Node[] = [];
  const edges: Edge[] = [];

  for (let i = 0; i < Math.min(revealed, chain.length); i++) {
    const hop = chain[i];
    const txId = `tx_${i}`;
    const payId = `pay_${i}`;
    const changeId = `change_${i}`;
    const txX = 400;
    const txY = i * (NODE_H + V_GAP) + 40;

    nodes.push({ id: txId, x: txX, y: txY, label: hop.txid.slice(0, 16) + "...", sublabel: "TRANSACTION", type: "tx" });
    nodes.push({ id: payId, x: txX - H_GAP, y: txY + NODE_H + 40, label: (hop.small_output_value / 1e8).toFixed(4) + " BTC", sublabel: hop.small_output_address?.slice(0, 16) + "..." || "PAYMENT", type: "payment" });
    edges.push({ from: txId, to: payId, label: "payment" });

    if (i < chain.length - 1) {
      nodes.push({ id: changeId, x: txX, y: txY + NODE_H + 40, label: (hop.large_output_value / 1e8).toFixed(4) + " BTC", sublabel: "CHANGE → NEXT TX", type: "change" });
      edges.push({ from: txId, to: changeId, label: "change" });
    }
  }

  const getNodeCenter = (n: Node) => ({ x: n.x + NODE_W / 2, y: n.y + NODE_H / 2 });

  return (
    <div className="fixed inset-0 z-50 bg-background/95 flex flex-col">
      <div className="flex items-center justify-between px-6 py-4 border-b border-border shrink-0">
        <div>
          <p className="text-label">PEELING_CHAIN_GRAPH</p>
          <p className="text-sm font-mono-data text-muted-foreground mt-1">
            {revealed} / {chain.length} hops revealed — click CHANGE node to expand
          </p>
        </div>
        <button onClick={onClose} className="text-xs border border-foreground px-3 py-1 font-mono-data hover:bg-foreground hover:text-background">
          CLOSE
        </button>
      </div>

      <div className="flex-1 overflow-y-auto">
        <svg
          width="900"
          height={Math.max(800, revealed * (NODE_H + V_GAP) + 300)}
          style={{ display: "block", margin: "0 auto" }}
        >
          {edges.map((edge, i) => {
            const from = nodes.find(n => n.id === edge.from);
            const to = nodes.find(n => n.id === edge.to);
            if (!from || !to) return null;
            const fc = getNodeCenter(from);
            const tc = getNodeCenter(to);
            const isChange = edge.label === "change";
            return (
              <g key={i}>
                <line
                  x1={fc.x} y1={fc.y + NODE_H / 2}
                  x2={tc.x} y2={tc.y - NODE_H / 2}
                  stroke={isChange ? "#ffffff40" : "#a855f740"}
                  strokeWidth={isChange ? 2 : 1}
                  strokeDasharray={isChange ? "none" : "4 4"}
                />
                <text x={(fc.x + tc.x) / 2 + 8} y={(fc.y + NODE_H / 2 + tc.y - NODE_H / 2) / 2} fill="#52525b" fontSize={10} fontFamily="monospace">
                  {edge.label.toUpperCase()}
                </text>
              </g>
            );
          })}

          {nodes.map(node => {
            const isChange = node.type === "change";
            const isClickable = isChange && revealed < chain.length;
            return (
              <g key={node.id} onClick={() => isClickable && setRevealed(r => r + 1)} style={{ cursor: isClickable ? "pointer" : "default" }}>
                <rect x={node.x} y={node.y} width={NODE_W} height={NODE_H} rx={4}
                  fill={node.type === "tx" ? "#1a1a1a" : node.type === "change" ? "#1a1a2e" : "#1a0a1a"}
                  stroke={node.type === "tx" ? "#3f3f46" : node.type === "change" ? "#3b3b6b" : "#4b1a4b"}
                  strokeWidth={isClickable ? 2 : 1}
                />
                {isClickable && (
                  <rect x={node.x} y={node.y} width={NODE_W} height={NODE_H} rx={4}
                    fill="transparent" stroke="#6366f1" strokeWidth={1} strokeDasharray="4 4" opacity={0.5}
                  />
                )}
                <text x={node.x + 12} y={node.y + 22} fill="#ffffff" fontSize={11} fontFamily="monospace" fontWeight="500">
                  {node.label}
                </text>
                <text x={node.x + 12} y={node.y + 40} fill="#71717a" fontSize={9} fontFamily="monospace">
                  {node.sublabel}
                </text>
                {isClickable && (
                  <text x={node.x + NODE_W - 50} y={node.y + 35} fill="#6366f1" fontSize={9} fontFamily="monospace">
                    + EXPAND
                  </text>
                )}
              </g>
            );
          })}
        </svg>
      </div>
    </div>
  );
}
