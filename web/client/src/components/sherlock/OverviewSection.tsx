import { AnalysisResult } from "@/types/analysis";
import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer, Cell } from "recharts";
import { Blocks, FileSearch, Flag, TrendingDown, TrendingUp, Minus, Activity } from "lucide-react";

interface OverviewSectionProps {
  data: AnalysisResult;
}

const barColors = ["#ffffff", "#a1a1aa", "#71717a", "#52525b", "#3f3f46", "#27272a", "#1c1c1e"];

export function OverviewSection({ data }: OverviewSectionProps) {
  const { analysis_summary: summary } = data;

  const scriptData = Object.entries(summary.script_type_distribution || {})
    .map(([name, value]) => ({ name: name.toUpperCase(), value }))
    .sort((a, b) => b.value - a.value);

  const stats = [
    { label: "BLOCKS", value: data.block_count.toLocaleString(), icon: Blocks },
    { label: "TRANSACTIONS", value: summary.total_transactions_analyzed.toLocaleString(), icon: FileSearch },
    { label: "FLAGGED", value: summary.flagged_transactions.toLocaleString(), icon: Flag },
  ];

  const feeStats = [
    { label: "MIN", value: summary.fee_rate_stats.min_sat_vb.toFixed(1), icon: TrendingDown },
    { label: "MAX", value: summary.fee_rate_stats.max_sat_vb.toFixed(1), icon: TrendingUp },
    { label: "MEDIAN", value: summary.fee_rate_stats.median_sat_vb.toFixed(1), icon: Minus },
    { label: "MEAN", value: summary.fee_rate_stats.mean_sat_vb.toFixed(1), icon: Activity },
  ];

  const heuristicColors: Record<string, string> = {
    cioh: "border-blue-500/30 bg-blue-500/10 text-blue-400",
    change_detection: "border-yellow-500/30 bg-yellow-500/10 text-yellow-400",
    consolidation: "border-orange-500/30 bg-orange-500/10 text-orange-400",
    address_reuse: "border-red-500/30 bg-red-500/10 text-red-400",
    self_transfer: "border-cyan-500/30 bg-cyan-500/10 text-cyan-400",
    coinjoin: "border-purple-500/30 bg-purple-500/10 text-purple-400",
    op_return: "border-green-500/30 bg-green-500/10 text-green-400",
    round_num_payments: "border-pink-500/30 bg-pink-500/10 text-pink-400",
    peeling_chain: "border-indigo-500/30 bg-indigo-500/10 text-indigo-400",
  };

  return (
    <div className="space-y-6 animate-fade-in">
      {/* Hero Stats */}
      <div className="grid grid-cols-3 gap-4">
        {stats.map((s) => (
          <div key={s.label} className="surface-card p-6">
            <div className="flex items-center justify-between">
              <p className="text-label">{s.label}</p>
              <s.icon className="w-4 h-4 text-muted-foreground" />
            </div>
            <p className="text-3xl font-mono-data font-light tracking-tighter mt-4">{s.value}</p>
          </div>
        ))}
      </div>

      {/* Fee Rates */}
      <div className="surface-card p-6">
        <p className="text-label mb-4">FEE_RATE_STATS (sat/vB)</p>
        <div className="grid grid-cols-4 gap-4">
          {feeStats.map((f) => (
            <div key={f.label} className="flex items-center gap-3">
              <f.icon className="w-4 h-4 text-muted-foreground" />
              <div>
                <p className="text-label">{f.label}</p>
                <p className="text-xl font-mono-data tracking-tight">{f.value}</p>
              </div>
            </div>
          ))}
        </div>
      </div>

      <div className="grid grid-cols-2 gap-4">
        {/* Script Distribution Chart */}
        <div className="surface-card p-6">
          <p className="text-label mb-4">SCRIPT_TYPE_DISTRIBUTION</p>
          <p className="text-xs text-muted-foreground mb-2">{scriptData.length} types</p>
          <ResponsiveContainer width="100%" height={280}>
            <BarChart data={scriptData} layout="vertical" margin={{ left: 70, right: 20 }} style={{ background: 'transparent' }}>
              <XAxis
                type="number"
                stroke="#262626"
                tick={{ fill: "#a1a1aa", fontSize: 10, fontFamily: "'Geist Mono', monospace" }}
                tickFormatter={(v: number) => v >= 1000 ? `${(v / 1000).toFixed(0)}k` : String(v)}
              />
              <YAxis
                type="category"
                dataKey="name"
                stroke="#262626"
                tick={{ fill: "#a1a1aa", fontSize: 10, fontFamily: "'Geist Mono', monospace" }}
                width={65}
              />
              <Tooltip
                contentStyle={{
                  background: "#141414",
                  border: "1px solid #262626",
                  color: "#ffffff",
                  fontSize: 12,
                  fontFamily: "'Geist Mono', monospace",
                }}
                labelStyle={{ color: "#ffffff" }}
                itemStyle={{ color: "#a1a1aa" }}
              />
              <Bar dataKey="value" radius={[0, 2, 2, 0]}>
                {scriptData.map((_, i) => (
                  <Cell key={i} fill={barColors[i % barColors.length]} />
                ))}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        </div>

        {/* Heuristics Applied */}
        <div className="surface-card p-6">
          <p className="text-label mb-4">HEURISTICS_APPLIED</p>
          <div className="flex flex-wrap gap-2">
            {summary.heuristics_applied.map((h) => (
              <span
                key={h}
                className={`px-3 py-1.5 text-xs font-mono-data border ${heuristicColors[h] || "border-border bg-secondary text-foreground"}`}
              >
                {h.toUpperCase()}
              </span>
            ))}
          </div>
          <div className="mt-6 space-y-2">
            <p className="text-label">ANALYSIS_STATS</p>
            <div className="grid grid-cols-2 gap-2">
              <div className="flex justify-between px-3 py-2 bg-secondary text-sm">
                <span className="text-muted-foreground">Flag Rate</span>
                <span className="font-mono-data">{((summary.flagged_transactions / summary.total_transactions_analyzed) * 100).toFixed(2)}%</span>
              </div>
              <div className="flex justify-between px-3 py-2 bg-secondary text-sm">
                <span className="text-muted-foreground">Heuristics</span>
                <span className="font-mono-data">{summary.heuristics_applied.length}</span>
              </div>
              <div className="flex justify-between px-3 py-2 bg-secondary text-sm">
                <span className="text-muted-foreground">Flagged</span>
                <span className="font-mono-data">{summary.flagged_transactions.toLocaleString()}</span>
              </div>
              <div className="flex justify-between px-3 py-2 bg-secondary text-sm">
                <span className="text-muted-foreground">Clean</span>
                <span className="font-mono-data">{(summary.total_transactions_analyzed - summary.flagged_transactions).toLocaleString()}</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
