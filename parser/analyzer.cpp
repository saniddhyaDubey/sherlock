#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <sys/stat.h>
#include <errno.h>

#include "parser.cpp"
#include "json.hpp"

struct AnalyzedTransaction {
    std::string txid;
    std::unordered_map<std::string, nlohmann::json> heuristics;
    std::string classification;
};

struct FeeRateStats {
    double min_sat_vb;
    double max_sat_vb;
    double median_sat_vb;
    double mean_sat_vb;
};

struct AnalysisSummary {
    uint64_t total_transactions_analyzed;
    std::vector<std::string> heuristics_applied;
    uint64_t flagged_transactions;
    FeeRateStats fee_rate_stats;
    std::unordered_map<std::string, uint64_t> script_type_distribution;
};

struct AnalyzedBlock {
    std::string block_hash;
    uint64_t block_height;
    uint64_t tx_count;
    AnalysisSummary analysis_summary;
    std::vector<AnalyzedTransaction> transactions;
    std::unordered_map<std::string, uint64_t> heuristic_counts;
};

struct PeelingHop {
    std::string txid;
    uint64_t large_output_value;
    uint64_t small_output_value;
    std::string large_output_address;
    std::string small_output_address;
};

struct FileAnalysisResult {
    bool ok;
    std::string mode;
    std::string file;
    uint64_t block_count;
    AnalysisSummary analysis_summary;
    std::vector<AnalyzedBlock> blocks;
    std::vector<std::vector<PeelingHop>> peeling_chains; // each chain is list of txids
};

//Heuristic Implementation:
nlohmann::json applyCIOH(Transaction& tx){
    nlohmann::json j_op;
    bool flag = tx.vin.size() > 1;
    j_op["detected"] = flag;
    if(flag){
        uint64_t ip_cnt = tx.vin.size();
        j_op["ip_cnt"] = ip_cnt;

        // logarithmic confidence: saturates at 50 inputs
        double raw = std::log((double)ip_cnt) / std::log(50.0);
        double confidence_score = std::min(1.0, raw);
        j_op["confidence_score"] = std::round(confidence_score * 100.0) / 100.0;

        // confidence label
        if(confidence_score >= 0.75) j_op["confidence"] = "high";
        else if(confidence_score >= 0.45) j_op["confidence"] = "medium";
        else j_op["confidence"] = "low";

        std::vector<std::string> addresses;
        for(auto& in: tx.vin){
            if(in.address.has_value()) addresses.push_back(in.address.value());
        }
        j_op["addresses"] = addresses;
    }
    return j_op;
}

inline bool is_round_btc(uint64_t value_sats) {
    return (value_sats % 100000ULL == 0);
}

nlohmann::json applyChangeDetection(Transaction& tx){
    nlohmann::json j_op;

    //ignoring coinbase transaction:
    if(tx.vin.size() == 1 && tx.vin[0].txid == "0000000000000000000000000000000000000000000000000000000000000000"){
        j_op["detected"] = false;
        return j_op;
    }

    bool script_check=false, address_check=false, round_num=false, index_check=false;
    uint8_t count=0;
    int likely_change_index = -1;
    std::string confidence;
    std::unordered_map<std::string, std::string> ip_script;
    for(auto& in: tx.vin){
        ip_script[in.script_type] = in.address.has_value() ? in.address.value() : "";
    }
    //script_check
    for(auto& op: tx.vout){
        if(ip_script.find(op.script_type)!=ip_script.end()){
            likely_change_index = op.n;
            script_check = true;
            ++count;
            //address_check
            if(op.address.has_value()){
                if(ip_script[op.script_type] == op.address.value()){
                    address_check=true;
                    ++count;
                }
            }
            //round_num check
            if(!is_round_btc(op.value_sats)){
                round_num=true;
                ++count;
            }
            //index check
            if(op.n == 1){
                index_check=true;
                ++count;
            }
        }
    }

    if(count){
        if(count >= 4) confidence = "very_high";
        else if(count == 3) confidence = "high";
        else if(count == 2) confidence = "medium";
        else confidence = "low";
        j_op["detected"] = true;
        j_op["likely_change_index"] = likely_change_index;
        
        std::vector<std::string> methods;
        if(script_check) methods.push_back("script_type_match");
        if(address_check) methods.push_back("address_match");
        if(round_num) methods.push_back("round_number");
        if(index_check) methods.push_back("index_match");
        j_op["methods"] = methods;

        j_op["confidence"] = confidence;
    }else{
        j_op["detected"] = false;
    }
    return j_op;
}

nlohmann::json applyConsolidation(Transaction& tx){
    nlohmann::json j_op;
    bool flag = (tx.vin.size() >= 5 && tx.vout.size() < 3) ? true : false;
    std::string confidence;
    j_op["detected"] = flag;
    if(flag){
        std::unordered_set<std::string> scripts;
        for(auto& in: tx.vin) scripts.insert(in.script_type);
        for(auto& op: tx.vout){
            if(scripts.count(op.script_type)){
                confidence = "medium";        
            }
        }
        if(scripts.size() == 1) confidence = "very_high";
        if(confidence.empty()) confidence = "low";
        j_op["confidence"] = confidence;
        j_op["ip_cnt"] = tx.vin.size();
        j_op["op_cnt"] = tx.vout.size();
    }
    return j_op;
}

nlohmann::json applyAddressReuse(Transaction& tx, std::unordered_set<std::string>& seen_addresses){
    nlohmann::json j_op;
    j_op["detected"]=false;

    std::unordered_set<std::string> tx_addresses;
    //collect input addresses:
    for(auto& in: tx.vin){
        if(in.address.has_value()){
            tx_addresses.insert(in.address.value());
        }
    }
    //checking with previously seen addresses:
    for(const auto& addr: tx_addresses){
        if(seen_addresses.count(addr)){
            j_op["detected"] = true;
            break;
        }
    }
    //checking with output addresses:
    for(auto& op: tx.vout){
        if(op.address.has_value()){
            if(tx_addresses.find(op.address.value()) != tx_addresses.end()){
                j_op["detected"]=true;
                break;
            }
            tx_addresses.insert(op.address.value());
        }
    }
    //updating seen addresses:
    seen_addresses.insert(tx_addresses.begin(), tx_addresses.end());

    return j_op;
}

nlohmann::json applySelfTransfer(Transaction& tx){
    nlohmann::json j_op;
    
    //ignoring coinbase transaction:
    if(tx.vin.size() == 1 && tx.vin[0].txid == "0000000000000000000000000000000000000000000000000000000000000000"){
        j_op["detected"] = false;
        return j_op;
    }

    //collect input script types:
    std::unordered_set<std::string> in_scripts;
    for(auto& in: tx.vin) in_scripts.insert(in.script_type);

    //collect output script types:
    std::unordered_set<std::string> out_scripts;
    bool has_round = false;
    for(auto& op: tx.vout){
        out_scripts.insert(op.script_type);
        if(is_round_btc(op.value_sats)) has_round = true;
    }

    //all inputs same type, all outputs same type, types match, no round outputs:
    bool flag = (in_scripts.size() == 1 && 
                 out_scripts.size() == 1 && 
                 *in_scripts.begin() == *out_scripts.begin() &&
                 !has_round);

    j_op["detected"] = flag;
    if(flag){
        j_op["script_type"] = *in_scripts.begin();
        j_op["input_count"] = tx.vin.size();
        j_op["output_count"] = tx.vout.size();
    }
    return j_op;
}

nlohmann::json applyCoinJoin(Transaction& tx){
    nlohmann::json j_op;
    j_op["detected"] = false;

    if(tx.vin.size() == 1 && tx.vin[0].txid == "0000000000000000000000000000000000000000000000000000000000000000"){
        return j_op;
    }

    if(tx.vin.size() < 3){
        return j_op;
    }

    std::unordered_set<std::string> input_addresses;
    for(auto& in: tx.vin){
        if(in.address.has_value()) input_addresses.insert(in.address.value());
    }

    std::unordered_map<uint64_t, uint64_t> value_counts;
    for(auto& op: tx.vout) value_counts[op.value_sats]++;

    uint64_t equal_output_count = 0;
    for(auto& [v, cnt]: value_counts){
        if(cnt >= 2) equal_output_count += cnt;
    }

    bool flag = (equal_output_count >= 2 && input_addresses.size() >= 3);
    j_op["detected"] = flag;
    if(flag){
        j_op["input_count"] = tx.vin.size();
        j_op["equal_output_count"] = equal_output_count;
        j_op["unique_input_addresses"] = input_addresses.size();
    }
    return j_op;
}

nlohmann::json applyOpReturn(Transaction& tx){
    nlohmann::json j_op;
    j_op["detected"] = false;

    std::vector<std::string> protocols;
    for(auto& op: tx.vout){
        if(op.script_type == "op_return"){
            if(op.op_return_protocol.has_value())
                protocols.push_back(op.op_return_protocol.value());
        }
    }

    if(!protocols.empty()){
        j_op["detected"] = true;
        j_op["count"] = protocols.size();
        j_op["protocols"] = protocols;
    }
    return j_op;
}

nlohmann::json applyRoundNumberPayment(Transaction& tx){
    nlohmann::json j_op;
    j_op["detected"] = false;

    if(tx.vin.size() == 1 && tx.vin[0].txid == "0000000000000000000000000000000000000000000000000000000000000000")
        return j_op;

    std::vector<int> round_indices;
    for(auto& op: tx.vout){
        if(op.script_type != "op_return" && is_round_btc(op.value_sats))
            round_indices.push_back(op.n);
    }

    if(!round_indices.empty()){
        j_op["detected"] = true;
        j_op["round_output_indices"] = round_indices;
        j_op["round_output_count"] = round_indices.size();
    }
    return j_op;
}

nlohmann::json applyPeelingChain(Transaction& tx, std::unordered_map<std::string, uint64_t>& peeling_candidates){
    nlohmann::json j_op;
    j_op["detected"] = false;

    if(tx.vin.size() == 1 && tx.vin[0].txid == "0000000000000000000000000000000000000000000000000000000000000000")
        return j_op;

    // peeling chain pattern: 1 input, 2 outputs
    if(tx.vin.size() != 1 || tx.vout.size() != 2){
        return j_op;
    }

    uint64_t val0 = tx.vout[0].value_sats;
    uint64_t val1 = tx.vout[1].value_sats;
    uint64_t large_val = std::max(val0, val1);
    uint64_t small_val = std::min(val0, val1);
    uint64_t large_idx = (val0 > val1) ? 0 : 1;

    //checking 3x ratio:
    if(small_val == 0 || large_val < 3 * small_val)
        return j_op;

    //checking if this tx's input was a peeling candidate:
    std::string input_key = tx.vin[0].txid + ":" + std::to_string(tx.vin[0].vout);
    bool is_continuation = peeling_candidates.count(input_key) > 0;

    //adding large output as new candidate:
    std::string output_key = tx.txid + ":" + std::to_string(large_idx);
    peeling_candidates[output_key] = large_val;

    j_op["detected"] = true;
    j_op["is_continuation"] = is_continuation;
    j_op["large_output_value"] = large_val;
    j_op["small_output_value"] = small_val;
    j_op["large_output_index"] = large_idx;
    j_op["large_output_address"] = tx.vout[large_idx].address.value_or("");
    j_op["small_output_address"] = tx.vout[1-large_idx].address.value_or("");

    return j_op;
}
//-------------------------
std::string classify(std::unordered_map<std::string, nlohmann::json>& heuristics, Transaction& tx){
    //coinjoin -> highest priority
    if(heuristics.count("coinjoin") && heuristics["coinjoin"]["detected"] == true) return "coinjoin";
    
    //self transfer -> same script type in and out, no payment
    if(heuristics.count("self_transfer") && heuristics["self_transfer"]["detected"] == true) return "self_transfer";
    
    //consolidation -> many inputs, few outputs
    if(heuristics.count("consolidation") && heuristics["consolidation"]["detected"] == true) return "consolidation";
    
    //batch payment -> 1-2 inputs, many outputs
    if(tx.vin.size() <= 2 && tx.vout.size() >= 5) return "batch_payment";
    
    //simple payment -> 1-2 inputs, 1-2 outputs
    if(tx.vin.size() <= 2 && tx.vout.size() <= 2) return "simple_payment";
    
    return "unknown";
}

FeeRateStats computeFeeRateStats(std::vector<double>& fee_rates){
    FeeRateStats stats;
    if(fee_rates.empty()){
        stats.min_sat_vb = 0.0;
        stats.max_sat_vb = 0.0;
        stats.median_sat_vb = 0.0;
        stats.mean_sat_vb = 0.0;
        return stats;
    }

    std::sort(fee_rates.begin(), fee_rates.end());

    stats.min_sat_vb = fee_rates.front();
    stats.max_sat_vb = fee_rates.back();

    //calculating median:
    size_t n = fee_rates.size();
    if(n % 2 == 0)
        stats.median_sat_vb = (fee_rates[n/2 - 1] + fee_rates[n/2]) / 2.0;
    else
        stats.median_sat_vb = fee_rates[n/2];

    //calculating mean:
    double sum = 0.0;
    for(double r: fee_rates) sum += r;
    stats.mean_sat_vb = sum / n;

    return stats;
}

FileAnalysisResult analyzeBlocks(std::vector<Block>& all_blocks, std::string file_name, bool ui_mode = false){
    FileAnalysisResult far;
    far.ok = true;
    far.mode = "chain_analysis";
    far.file = file_name + ".dat";
    far.block_count = all_blocks.size();

    std::vector<double> all_fee_rates;

    std::unordered_map<std::string, uint64_t> peeling_candidates;
    std::unordered_map<std::string, size_t> txid_to_chain; // txid -> chain index in far.peeling_chains

    for(size_t bi = 0; bi < all_blocks.size(); bi++){
        Block& block = all_blocks[bi];
        AnalyzedBlock ab;
        ab.block_hash = block.block_header.block_hash;
        ab.block_height = block.coinbase.bip34_height;
        ab.tx_count = block.transactions.size();

        uint64_t flagged = 0;
        std::vector<double> fee_rates;

        std::unordered_set<std::string> seen_addresses;

        for(size_t ti = 0; ti < block.transactions.size(); ti++){
            Transaction& tx = block.transactions[ti];

            //running heuristics:
            nlohmann::json cioh = applyCIOH(tx);
            nlohmann::json change = applyChangeDetection(tx);
            nlohmann::json consolidation = applyConsolidation(tx);
            nlohmann::json address_reuse = applyAddressReuse(tx, seen_addresses);
            nlohmann::json self_transfer = applySelfTransfer(tx);
            nlohmann::json coin_join = applyCoinJoin(tx);
            nlohmann::json op_return = applyOpReturn(tx);
            nlohmann::json round_num_payments = applyRoundNumberPayment(tx);
            nlohmann::json peeling_chain = applyPeelingChain(tx, peeling_candidates);

            //CoinJoin interaction — downgrade CIOH confidence if CoinJoin detected:
            if(coin_join["detected"] && cioh["detected"]){
                cioh["confidence"] = "unreliable";
                cioh["confidence_score"] = 0.0;
                cioh["note"] = "CoinJoin detected — CIOH assumption does not apply";
            }

            if(peeling_chain["detected"]){
                PeelingHop hop;
                hop.txid = tx.txid;
                hop.large_output_value = peeling_chain["large_output_value"];
                hop.small_output_value = peeling_chain["small_output_value"];
                hop.large_output_address = peeling_chain["large_output_address"];
                hop.small_output_address = peeling_chain["small_output_address"];
                
                if(peeling_chain["is_continuation"] && txid_to_chain.count(tx.vin[0].txid)){
                    size_t chain_idx = txid_to_chain[tx.vin[0].txid];
                    far.peeling_chains[chain_idx].push_back(hop);
                    txid_to_chain[tx.txid] = chain_idx;
                } else {
                    far.peeling_chains.push_back({hop});
                    txid_to_chain[tx.txid] = far.peeling_chains.size() - 1;
                }
            }

            //checking if any heuristic fired:
            bool any_detected = cioh["detected"] || change["detected"] || consolidation["detected"] || address_reuse["detected"] || self_transfer["detected"] || coin_join["detected"] || op_return["detected"] || round_num_payments["detected"] || peeling_chain["detected"];
            if(any_detected) flagged++;

            if(cioh["detected"]) ab.heuristic_counts["cioh"]++;
            if(change["detected"]) ab.heuristic_counts["change_detection"]++;
            if(consolidation["detected"]) ab.heuristic_counts["consolidation"]++;
            if(address_reuse["detected"]) ab.heuristic_counts["address_reuse"]++;
            if(self_transfer["detected"]) ab.heuristic_counts["self_transfer"]++;
            if(coin_join["detected"]) ab.heuristic_counts["coinjoin"]++;
            if(op_return["detected"]) ab.heuristic_counts["op_return"]++;
            if(round_num_payments["detected"]) ab.heuristic_counts["round_num_payments"]++;
            if(peeling_chain["detected"]) ab.heuristic_counts["peeling_chain"]++;

            //collecting fee rates (skip coinbase):
            if(ti > 0) fee_rates.push_back(tx.fee_rate_sat_vb);

            //storing full tx details only for block 0:
            if(bi == 0 || ui_mode){
                AnalyzedTransaction at;
                at.txid = tx.txid;
                at.heuristics["cioh"] = cioh;
                at.heuristics["change_detection"] = change;
                at.heuristics["consolidation"] = consolidation;
                at.heuristics["address_reuse"] = address_reuse;
                at.heuristics["self_transfer"] = self_transfer;
                at.heuristics["coinjoin"] = coin_join;
                at.heuristics["op_return"] = op_return;
                at.heuristics["round_num_payments"] = round_num_payments;
                at.heuristics["peeling_chain"] = peeling_chain;
                at.classification = classify(at.heuristics, tx);
                ab.transactions.push_back(at);
            }

            for(auto& op: tx.vout){
                ab.analysis_summary.script_type_distribution[op.script_type]++;
            }
        }

        //building block summary:
        ab.analysis_summary.total_transactions_analyzed = ab.tx_count;
        ab.analysis_summary.flagged_transactions = flagged;
        ab.analysis_summary.heuristics_applied = {"cioh", "change_detection", "consolidation", "address_reuse", "self_transfer", "coinjoin", "op_return", "round_num_payments", "peeling_chain"};
        ab.analysis_summary.fee_rate_stats = computeFeeRateStats(fee_rates);
        all_fee_rates.insert(all_fee_rates.end(), fee_rates.begin(), fee_rates.end());

        far.blocks.push_back(ab);
    }

    //file level aggregation:
    uint64_t total_tx = 0, total_flagged = 0;
    for(auto& ab: far.blocks){
        total_tx += ab.tx_count;
        total_flagged += ab.analysis_summary.flagged_transactions;

        for(auto& [k,v]: ab.analysis_summary.script_type_distribution){
            far.analysis_summary.script_type_distribution[k] += v;
        }
    }
    far.analysis_summary.total_transactions_analyzed = total_tx;
    far.analysis_summary.flagged_transactions = total_flagged;
    far.analysis_summary.heuristics_applied = {"cioh", "change_detection", "consolidation", "address_reuse", "self_transfer", "coinjoin", "op_return", "round_num_payments", "peeling_chain"};
    far.analysis_summary.fee_rate_stats = computeFeeRateStats(all_fee_rates);

    //filtering out chains with atleast 2 hops (1-hop is a false postive):
    far.peeling_chains.erase(
        std::remove_if(far.peeling_chains.begin(), far.peeling_chains.end(),
            [](const std::vector<PeelingHop>& chain){ return chain.size() < 2; }),
        far.peeling_chains.end()
    );
    //sorting the chains based on number of hops to make it interesting on UI:
    std::sort(far.peeling_chains.begin(), far.peeling_chains.end(),
    [](const std::vector<PeelingHop>& a, const std::vector<PeelingHop>& b){
        return a.size() > b.size(); // descending
    });

    return far;
}

nlohmann::json serializeFileAnalysisResult(const FileAnalysisResult& far){
    nlohmann::json j;
    j["ok"] = far.ok;
    j["mode"] = far.mode;
    j["file"] = far.file;
    j["block_count"] = far.block_count;

    //file level summary:
    nlohmann::json j_summary;
    j_summary["total_transactions_analyzed"] = far.analysis_summary.total_transactions_analyzed;
    j_summary["heuristics_applied"] = far.analysis_summary.heuristics_applied;
    j_summary["flagged_transactions"] = far.analysis_summary.flagged_transactions;
    j_summary["fee_rate_stats"]["min_sat_vb"] = far.analysis_summary.fee_rate_stats.min_sat_vb;
    j_summary["fee_rate_stats"]["max_sat_vb"] = far.analysis_summary.fee_rate_stats.max_sat_vb;
    j_summary["fee_rate_stats"]["median_sat_vb"] = far.analysis_summary.fee_rate_stats.median_sat_vb;
    j_summary["fee_rate_stats"]["mean_sat_vb"] = far.analysis_summary.fee_rate_stats.mean_sat_vb;
    j_summary["script_type_distribution"] = far.analysis_summary.script_type_distribution;
    j["analysis_summary"] = j_summary;

    //blocks array:
    nlohmann::json blocks_array = nlohmann::json::array();
    for(size_t bi = 0; bi < far.blocks.size(); bi++){
        const AnalyzedBlock& ab = far.blocks[bi];
        nlohmann::json j_block;
        j_block["block_hash"] = ab.block_hash;
        j_block["block_height"] = ab.block_height;
        j_block["tx_count"] = ab.tx_count;

        //per block summary:
        nlohmann::json j_block_summary;
        j_block_summary["total_transactions_analyzed"] = ab.analysis_summary.total_transactions_analyzed;
        j_block_summary["heuristics_applied"] = ab.analysis_summary.heuristics_applied;
        j_block_summary["flagged_transactions"] = ab.analysis_summary.flagged_transactions;
        j_block_summary["fee_rate_stats"]["min_sat_vb"] = ab.analysis_summary.fee_rate_stats.min_sat_vb;
        j_block_summary["fee_rate_stats"]["max_sat_vb"] = ab.analysis_summary.fee_rate_stats.max_sat_vb;
        j_block_summary["fee_rate_stats"]["median_sat_vb"] = ab.analysis_summary.fee_rate_stats.median_sat_vb;
        j_block_summary["fee_rate_stats"]["mean_sat_vb"] = ab.analysis_summary.fee_rate_stats.mean_sat_vb;
        j_block_summary["script_type_distribution"] = ab.analysis_summary.script_type_distribution;
        // j_summary["script_type_distribution"] = far.analysis_summary.script_type_distribution;
        j_block["analysis_summary"] = j_block_summary;

        //transactions -> full for block 0, empty for rest:
        nlohmann::json tx_array = nlohmann::json::array();
        if(bi == 0){
            for(const AnalyzedTransaction& at: ab.transactions){
                nlohmann::json j_tx;
                j_tx["txid"] = at.txid;
                j_tx["heuristics"] = at.heuristics;
                j_tx["classification"] = at.classification;
                tx_array.push_back(j_tx);
            }
        }
        j_block["transactions"] = tx_array;

        blocks_array.push_back(j_block);
    }
    j_summary["script_type_distribution"] = far.analysis_summary.script_type_distribution;
    j["analysis_summary"] = j_summary;
    j["blocks"] = blocks_array;

    return j;
}

std::string serializeUIResult(const FileAnalysisResult& far){
    nlohmann::json j = serializeFileAnalysisResult(far);
    
    // add peeling chains
    nlohmann::json peeling_chains_array = nlohmann::json::array();
    for(const auto& chain: far.peeling_chains){
        nlohmann::json chain_array = nlohmann::json::array();
        for(const auto& hop: chain){
            nlohmann::json j_hop;
            j_hop["txid"] = hop.txid;
            j_hop["large_output_value"] = hop.large_output_value;
            j_hop["small_output_value"] = hop.small_output_value;
            j_hop["large_output_address"] = hop.large_output_address;
            j_hop["small_output_address"] = hop.small_output_address;
            chain_array.push_back(j_hop);
        }
        peeling_chains_array.push_back(chain_array);
    }
    j["peeling_chains"] = peeling_chains_array;
    
    return j.dump();
}

void writeBlockOutput(const FileAnalysisResult& far, const std::string& file_name){
    //creating out/ if it doesn't exist
    // system("mkdir -p out");
    mkdir("out", 0755);
    
    nlohmann::json j = serializeFileAnalysisResult(far);
    
    //JSON part:
    std::string json_path = "out/" + file_name + ".json";
    std::ofstream json_file(json_path);
    json_file << j.dump();
    json_file.close();
}

void generateMarkdownReport(const FileAnalysisResult& far, const std::vector<Block>& all_blocks, const std::string& file_name){
    std::ofstream md("out/" + file_name + ".md");
    if(!md.is_open()){
        std::cerr << "Failed to open markdown file\n";
        return;
    }
    md << "# Chain Analysis Report: " << file_name << ".dat\n\n";

    md << "## File Overview\n\n";
    md << "| Field | Value |\n";
    md << "|-------|-------|\n";
    md << "| Source File | " << far.file << " |\n";
    md << "| Total Blocks | " << far.block_count << " |\n";
    md << "| Total Transactions Analyzed | " << far.analysis_summary.total_transactions_analyzed << " |\n";
    md << "| Flagged Transactions | " << far.analysis_summary.flagged_transactions << " |\n\n";

    md << "## Summary Statistics\n\n";

    md << "### Fee Rate Distribution\n\n";
    md << "| Metric | Value (sat/vB) |\n";
    md << "|--------|---------------|\n";
    md << "| Min | " << far.analysis_summary.fee_rate_stats.min_sat_vb << " |\n";
    md << "| Max | " << far.analysis_summary.fee_rate_stats.max_sat_vb << " |\n";
    md << "| Median | " << far.analysis_summary.fee_rate_stats.median_sat_vb << " |\n";
    md << "| Mean | " << far.analysis_summary.fee_rate_stats.mean_sat_vb << " |\n\n";

    md << "### Script Type Breakdown\n\n";
    md << "| Script Type | Count |\n";
    md << "|-------------|-------|\n";
    for(auto& [k,v]: far.analysis_summary.script_type_distribution)
        md << "| " << k << " | " << v << " |\n";
    md << "\n";

    md << "### Heuristics Applied\n\n";
    md << "| Heuristic |\n";
    md << "|-----------|\n";
    for(const auto& h: far.analysis_summary.heuristics_applied){
        md << "| " << h << " |\n";
    }
    md << "\n";
    
    md << "## Per-Block Analysis\n\n";

    for(size_t bi = 0; bi < far.blocks.size(); bi++){
        const AnalyzedBlock& ab = far.blocks[bi];
        
        md << "### Block " << bi+1 << "\n\n";
        md << "| Field | Value |\n";
        md << "|-------|-------|\n";
        md << "| Block Hash | `" << ab.block_hash << "` |\n";
        md << "| Block Height | " << ab.block_height << " |\n";

        time_t ts = all_blocks[bi].block_header.timestamp;
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", std::gmtime(&ts));
        md << "| Timestamp | " << buf << " |\n";

        md << "| Transaction Count | " << ab.tx_count << " |\n";
        md << "| Flagged Transactions | " << ab.analysis_summary.flagged_transactions << " |\n\n";

        //heuristic findings
        md << "#### Heuristic Findings\n\n";
        md << "| Heuristic | Transactions Flagged |\n";
        md << "|-----------|---------------------|\n";
        for(const auto& h: ab.analysis_summary.heuristics_applied){
            uint64_t cnt = ab.heuristic_counts.count(h) ? ab.heuristic_counts.at(h) : 0;
            md << "| " << h << " | " << cnt << " |\n";
        }
        md << "\n";
        //script type distribution
        md << "#### Script Type Distribution\n\n";
        md << "| Script Type | Count |\n";
        md << "|-------------|-------|\n";
        for(auto& [k,v]: ab.analysis_summary.script_type_distribution)
            md << "| " << k << " | " << v << " |\n";
        md << "\n";
        //notable transactions
        if(!ab.transactions.empty()){
            md << "#### Notable Transactions\n\n";
            md << "| TxID | Classification |\n";
            md << "|------|---------------|\n";
            size_t notable_count = 0;
            for(const auto& at: ab.transactions){
                if(notable_count >= 5) break;
                if(at.classification == "coinjoin" || at.classification == "consolidation" || at.classification == "batch_payment"){
                    md << "| `" << at.txid.substr(0,16) << "...` | " << at.classification << " |\n";
                    notable_count++;
                }
            }
            md << "\n";
        }
    }

    md << "## Peeling Chains Detected\n\n";

    if(far.peeling_chains.empty()){
        md << "No peeling chains detected.\n\n";
    } else {
        md << "Total chains found: " << far.peeling_chains.size() << "\n\n";
        size_t chain_limit = std::min(far.peeling_chains.size(), (size_t)10);
        for(size_t ci = 0; ci < chain_limit; ci++){
            md << "### Chain " << ci+1 << " (" << far.peeling_chains[ci].size() << " hops)\n\n";
            md << "| Hop | TxID | Large Output (sats) | Small Output (sats) |\n";
            md << "|-----|------|--------------------|-----------------|\n";
            size_t hop_limit = std::min(far.peeling_chains[ci].size(), (size_t)20);
            for(size_t hi = 0; hi < hop_limit; hi++){
                const PeelingHop& hop = far.peeling_chains[ci][hi];
                md << "| " << hi+1 << " | `" << hop.txid.substr(0,16) << "...` | " 
                << hop.large_output_value << " | " << hop.small_output_value << " |\n";
            }
            if(far.peeling_chains[ci].size() > 20)
                md << "| ... | +" << far.peeling_chains[ci].size()-20 << " more hops | | |\n";
            md << "\n";
        }
        if(far.peeling_chains.size() > 10)
            md << "_...and " << far.peeling_chains.size()-10 << " more chains not shown._\n\n";
    }

    md.close();
}
