export interface AnalysisResult {
  ok: boolean;
  mode: string;
  file: string;
  block_count: number;
  analysis_summary: AnalysisSummary;
  blocks: Block[];
  peeling_chains: PeelingChainHop[][];
}

export interface AnalysisSummary {
  total_transactions_analyzed: number;
  flagged_transactions: number;
  heuristics_applied: string[];
  fee_rate_stats: FeeRateStats;
  script_type_distribution: Record<string, number>;
}

export interface FeeRateStats {
  min_sat_vb: number;
  max_sat_vb: number;
  median_sat_vb: number;
  mean_sat_vb: number;
}

export interface Block {
  block_hash: string;
  block_height: number;
  tx_count: number;
  analysis_summary: BlockAnalysisSummary;
  transactions?: Transaction[];
}

export interface BlockAnalysisSummary {
  total_transactions_analyzed: number;
  flagged_transactions: number;
  heuristics_applied: string[];
  fee_rate_stats: FeeRateStats;
  script_type_distribution: Record<string, number>;
}

export interface Transaction {
  txid: string;
  classification: Classification;
  heuristics: TransactionHeuristics;
}

export type Classification =
  | "coinjoin"
  | "consolidation"
  | "self_transfer"
  | "simple_payment"
  | "batch_payment"
  | "unknown";

export interface TransactionHeuristics {
  cioh?: CIOHResult;
  change_detection?: ChangeDetectionResult;
  address_reuse?: AddressReuseResult;
  coinjoin?: CoinJoinResult;
  consolidation?: ConsolidationResult;
  self_transfer?: SelfTransferResult;
  peeling_chain?: PeelingChainResult;
  op_return?: OpReturnResult;
  round_num_payments?: RoundNumberResult;
}

export interface CIOHResult {
  detected: boolean;
  ip_cnt?: number;
  confidence?: string;
  confidence_score?: number;
  addresses?: string[];
  note?: string;
}

export interface ChangeDetectionResult {
  detected: boolean;
  likely_change_index?: number;
  methods?: string[];
  confidence?: string;
}

export interface AddressReuseResult {
  detected: boolean;
  reused_addresses?: string[];
  reuse_count?: number;
}

export interface CoinJoinResult {
  detected: boolean;
  equal_output_count?: number;
  unique_input_addresses?: number;
}

export interface ConsolidationResult {
  detected: boolean;
  input_count?: number;
  output_count?: number;
  confidence?: string;
}

export interface SelfTransferResult {
  detected: boolean;
}

export interface PeelingChainResult {
  detected: boolean;
  large_output_value?: number;
  small_output_value?: number;
  is_continuation?: boolean;
}

export interface OpReturnResult {
  detected: boolean;
  count?: number;
  protocols?: string[];
}

export interface RoundNumberResult {
  detected: boolean;
  round_output_indices?: number[];
  round_output_count?: number;
}

export interface PeelingChainHop {
  txid: string;
  large_output_value: number;
  small_output_value: number;
  large_output_address: string;
  small_output_address: string;
}

export type HeuristicKey =
  | "cioh"
  | "change_detection"
  | "address_reuse"
  | "coinjoin"
  | "consolidation"
  | "self_transfer"
  | "peeling_chain"
  | "op_return"
  | "round_num_payments";

export interface HeuristicInfo {
  key: HeuristicKey;
  name: string;
  description: string;
}
