#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include "../parser/analyzer.cpp"

Transaction makePeelingTx(uint64_t large_val, uint64_t small_val, std::string input_txid = "prev_tx", uint32_t input_vout = 0) {
    Transaction tx;
    tx.txid = "peel_tx_" + std::to_string(large_val);
    Input in;
    in.txid = input_txid;
    in.vout = input_vout;
    in.address = "input_addr";
    in.script_type = "p2wpkh";
    in.prevout.value_sats = large_val + small_val;
    tx.vin.push_back(in);

    Output large_out;
    large_out.n = 0;
    large_out.value_sats = large_val;
    large_out.script_type = "p2wpkh";
    large_out.address = "change_addr";
    tx.vout.push_back(large_out);

    Output small_out;
    small_out.n = 1;
    small_out.value_sats = small_val;
    small_out.script_type = "p2wpkh";
    small_out.address = "payment_addr";
    tx.vout.push_back(small_out);

    return tx;
}

void test_peeling_chain_detected() {
    Transaction tx = makePeelingTx(1000000, 10000);
    std::unordered_map<std::string, uint64_t> candidates;
    auto result = applyPeelingChain(tx, candidates);
    assert(result["detected"] == true);
    std::cout << "✓ Peeling chain: large/small ratio >= 3x detected\n";
}

void test_peeling_chain_ratio_too_small() {
    Transaction tx = makePeelingTx(100000, 50000); // ratio = 2x < 3x
    std::unordered_map<std::string, uint64_t> candidates;
    auto result = applyPeelingChain(tx, candidates);
    assert(result["detected"] == false);
    std::cout << "✓ Peeling chain: ratio < 3x not detected\n";
}

void test_peeling_chain_too_many_inputs() {
    Transaction tx = makePeelingTx(1000000, 10000);
    Input extra;
    extra.txid = "extra_prev";
    extra.address = "extra_addr";
    extra.script_type = "p2wpkh";
    extra.prevout.value_sats = 50000;
    tx.vin.push_back(extra); // now 2 inputs — should not be peeling
    std::unordered_map<std::string, uint64_t> candidates;
    auto result = applyPeelingChain(tx, candidates);
    assert(result["detected"] == false);
    std::cout << "✓ Peeling chain: multiple inputs not flagged\n";
}

void test_peeling_chain_too_many_outputs() {
    Transaction tx = makePeelingTx(1000000, 10000);
    Output extra;
    extra.n = 2;
    extra.value_sats = 5000;
    extra.script_type = "p2wpkh";
    tx.vout.push_back(extra); // now 3 outputs
    std::unordered_map<std::string, uint64_t> candidates;
    auto result = applyPeelingChain(tx, candidates);
    assert(result["detected"] == false);
    std::cout << "✓ Peeling chain: 3 outputs not flagged\n";
}

void test_peeling_chain_continuation_detected() {
    std::unordered_map<std::string, uint64_t> candidates;

    // First hop
    Transaction tx1 = makePeelingTx(1000000, 10000, "origin_tx", 0);
    tx1.txid = "hop1";
    applyPeelingChain(tx1, candidates);

    // Second hop — spends large output of tx1
    Transaction tx2 = makePeelingTx(990000, 10000, "hop1", 0);
    tx2.txid = "hop2";
    auto result = applyPeelingChain(tx2, candidates);
    assert(result["detected"] == true);
    assert(result["is_continuation"] == true);
    std::cout << "✓ Peeling chain: continuation across transactions detected\n";
}

void test_peeling_chain_candidates_map_updated() {
    std::unordered_map<std::string, uint64_t> candidates;
    Transaction tx = makePeelingTx(1000000, 10000);
    tx.txid = "test_tx";
    applyPeelingChain(tx, candidates);
    // large output should be added to candidates
    assert(candidates.count("test_tx:0") > 0);
    std::cout << "✓ Peeling chain: candidates map updated with large output\n";
}

int main() {
    std::cout << "\n=== Peeling Chain Tests ===\n\n";
    test_peeling_chain_detected();
    test_peeling_chain_ratio_too_small();
    test_peeling_chain_too_many_inputs();
    test_peeling_chain_too_many_outputs();
    test_peeling_chain_continuation_detected();
    test_peeling_chain_candidates_map_updated();
    std::cout << "\n✓ All peeling chain tests passed!\n\n";
    return 0;
}
