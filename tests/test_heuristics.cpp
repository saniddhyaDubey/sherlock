#include <cassert>
#include <iostream>
#include <vector>
#include <string>

//including analyzer functions directly:
#include "../parser/analyzer.cpp"

//helpers ───────────────────────────────────────────────────────────────

Transaction makeTx(int vin_count, int vout_count, bool coinbase = false) {
    Transaction tx;
    tx.txid = coinbase ? std::string(64, '0') : "abc123";
    for (int i = 0; i < vin_count; i++) {
        Input in;
        in.txid = coinbase ? std::string(64, '0') : "prev" + std::to_string(i);
        in.address = "addr" + std::to_string(i);
        in.script_type = "p2wpkh";
        in.prevout.value_sats = 100000;
        tx.vin.push_back(in);
    }
    for (int i = 0; i < vout_count; i++) {
        Output out;
        out.n = i;
        out.value_sats = 50000;
        out.script_type = "p2wpkh";
        tx.vout.push_back(out);
    }
    return tx;
}

//tests ─────────────────────────────────────────────────────────────────

void test_cioh_single_input() {
    Transaction tx = makeTx(1, 2);
    auto result = applyCIOH(tx);
    assert(result["detected"] == false);
    std::cout << "✓ CIOH: single input not flagged\n";
}

void test_cioh_multiple_inputs() {
    Transaction tx = makeTx(3, 2);
    auto result = applyCIOH(tx);
    assert(result["detected"] == true);
    assert(result["ip_cnt"] == 3);
    assert(result.contains("confidence_score"));
    std::cout << "✓ CIOH: multiple inputs flagged with confidence\n";
}

void test_cioh_confidence_increases_with_inputs() {
    Transaction tx2 = makeTx(2, 1);
    Transaction tx10 = makeTx(10, 1);
    double conf2 = applyCIOH(tx2)["confidence_score"];
    double conf10 = applyCIOH(tx10)["confidence_score"];
    assert(conf10 > conf2);
    std::cout << "✓ CIOH: confidence increases with input count\n";
}

void test_consolidation_detected() {
    Transaction tx = makeTx(6, 1);
    auto result = applyConsolidation(tx);
    assert(result["detected"] == true);
    std::cout << "✓ Consolidation: 6 inputs, 1 output flagged\n";
}

void test_consolidation_not_detected() {
    Transaction tx = makeTx(2, 2);
    auto result = applyConsolidation(tx);
    assert(result["detected"] == false);
    std::cout << "✓ Consolidation: 2 inputs not flagged\n";
}

void test_round_number_payment_detected() {
    Transaction tx = makeTx(1, 2);
    tx.vout[0].value_sats = 100000; // round
    tx.vout[1].value_sats = 87643;  // not round
    auto result = applyRoundNumberPayment(tx);
    assert(result["detected"] == true);
    assert(result["round_output_count"] == 1);
    std::cout << "✓ Round number: 100000 sats detected\n";
}

void test_round_number_not_detected() {
    Transaction tx = makeTx(1, 2);
    tx.vout[0].value_sats = 87643;
    tx.vout[1].value_sats = 12357;
    auto result = applyRoundNumberPayment(tx);
    assert(result["detected"] == false);
    std::cout << "✓ Round number: no round outputs correctly not flagged\n";
}

void test_fee_rate_stats_ordering() {
    std::vector<double> rates = {5.0, 1.0, 3.0, 10.0, 2.0};
    auto stats = computeFeeRateStats(rates);
    assert(stats.min_sat_vb <= stats.median_sat_vb);
    assert(stats.median_sat_vb <= stats.max_sat_vb);
    assert(stats.min_sat_vb == 1.0);
    assert(stats.max_sat_vb == 10.0);
    std::cout << "✓ Fee rate stats: ordering invariant holds\n";
}

void test_fee_rate_stats_empty() {
    std::vector<double> rates;
    auto stats = computeFeeRateStats(rates);
    assert(stats.min_sat_vb == 0.0);
    assert(stats.max_sat_vb == 0.0);
    std::cout << "✓ Fee rate stats: empty vector returns zeros\n";
}

void test_coinjoin_detected() {
    Transaction tx = makeTx(4, 4);
    // set equal output values
    for (auto& out : tx.vout) out.value_sats = 50000;
    tx.vin[0].address = "addr_a";
    tx.vin[1].address = "addr_b";
    tx.vin[2].address = "addr_c";
    tx.vin[3].address = "addr_d";
    auto result = applyCoinJoin(tx);
    assert(result["detected"] == true);
    std::cout << "✓ CoinJoin: equal outputs with multiple inputs flagged\n";
}

//main ──────────────────────────────────────────────────────────────────

int main() {
    std::cout << "\n=== Sherlock Heuristic Unit Tests ===\n\n";

    test_cioh_single_input();
    test_cioh_multiple_inputs();
    test_cioh_confidence_increases_with_inputs();
    test_consolidation_detected();
    test_consolidation_not_detected();
    test_round_number_payment_detected();
    test_round_number_not_detected();
    test_fee_rate_stats_ordering();
    test_fee_rate_stats_empty();
    test_coinjoin_detected();

    std::cout << "\n✓ All tests passed!\n\n";
    return 0;
}
