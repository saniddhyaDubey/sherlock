#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include "../parser/analyzer.cpp"

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

void test_change_detection_script_type_match() {
    Transaction tx = makeTx(1, 2);
    tx.vin[0].script_type = "p2wpkh";
    tx.vout[0].script_type = "p2wpkh"; // matches input
    tx.vout[1].script_type = "p2pkh";  // different
    auto result = applyChangeDetection(tx);
    assert(result["detected"] == true);
    std::cout << "✓ Change detection: script type match fires\n";
}

void test_change_detection_no_script_match() {
    Transaction tx = makeTx(1, 2);
    tx.vin[0].script_type = "p2wpkh";
    tx.vout[0].script_type = "p2pkh";
    tx.vout[1].script_type = "p2sh";
    auto result = applyChangeDetection(tx);
    assert(result["detected"] == false);
    std::cout << "✓ Change detection: no script match not detected\n";
}

void test_change_detection_coinbase_skipped() {
    Transaction tx = makeTx(1, 2, true);
    auto result = applyChangeDetection(tx);
    assert(result["detected"] == false);
    std::cout << "✓ Change detection: coinbase tx skipped\n";
}

void test_change_detection_confidence_very_high() {
    Transaction tx = makeTx(1, 2);
    // script match
    tx.vin[0].script_type = "p2wpkh";
    tx.vin[0].address = "addr_match";
    tx.vout[0].script_type = "p2wpkh";
    tx.vout[0].address = "addr_match"; // address match
    tx.vout[0].value_sats = 87643;     // non-round (change signal)
    tx.vout[0].n = 1;                   // index 1
    auto result = applyChangeDetection(tx);
    assert(result["detected"] == true);
    assert(result["confidence"] == "very_high");
    std::cout << "✓ Change detection: very_high confidence with all conditions\n";
}

void test_change_detection_low_confidence() {
    Transaction tx;
    tx.txid = "test_low";
    Input in;
    in.txid = "prev0";
    in.address = "unique_input_addr";
    in.script_type = "p2wpkh";
    in.prevout.value_sats = 200000;
    tx.vin.push_back(in);

    Output out;
    out.n = 0;
    out.value_sats = 100000; //round — no round signal
    out.script_type = "p2wpkh"; //matches input — script fires
    out.address = std::nullopt; //no address match
    tx.vout.push_back(out);

    auto result = applyChangeDetection(tx);
    assert(result["detected"] == true);
    assert(result["confidence"] == "low");
    std::cout << "✓ Change detection: low confidence with only script match\n";
}

void test_change_detection_single_output() {
    Transaction tx = makeTx(1, 1);
    tx.vin[0].script_type = "p2wpkh";
    tx.vout[0].script_type = "p2wpkh";
    auto result = applyChangeDetection(tx);
    // single output — change detection may still fire but index won't match
    std::cout << "✓ Change detection: single output handled\n";
}

int main() {
    std::cout << "\n=== Change Detection Tests ===\n\n";
    test_change_detection_script_type_match();
    test_change_detection_no_script_match();
    test_change_detection_coinbase_skipped();
    test_change_detection_confidence_very_high();
    test_change_detection_low_confidence();
    test_change_detection_single_output();
    std::cout << "\n✓ All change detection tests passed!\n\n";
    return 0;
}
