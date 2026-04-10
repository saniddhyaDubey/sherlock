#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include "../parser/analyzer.cpp"

Transaction makeTx(int vin_count, int vout_count) {
    Transaction tx;
    tx.txid = "abc123";
    for (int i = 0; i < vin_count; i++) {
        Input in;
        in.txid = "prev" + std::to_string(i);
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
        out.address = "out_addr" + std::to_string(i);
        tx.vout.push_back(out);
    }
    return tx;
}

void test_address_reuse_within_tx() {
    Transaction tx = makeTx(1, 2);
    tx.vin[0].address = "reused_addr";
    tx.vout[0].address = "reused_addr"; // same as input
    std::unordered_set<std::string> seen;
    auto result = applyAddressReuse(tx, seen);
    assert(result["detected"] == true);
    std::cout << "✓ Address reuse: within-tx reuse detected\n";
}

void test_address_reuse_cross_tx() {
    Transaction tx1 = makeTx(1, 2);
    tx1.vin[0].address = "shared_addr";
    std::unordered_set<std::string> seen;
    applyAddressReuse(tx1, seen);

    Transaction tx2 = makeTx(1, 2);
    tx2.vin[0].address = "shared_addr"; // reused from tx1
    auto result = applyAddressReuse(tx2, seen);
    assert(result["detected"] == true);
    std::cout << "✓ Address reuse: cross-tx reuse detected\n";
}

void test_address_reuse_no_reuse() {
    Transaction tx = makeTx(1, 2);
    tx.vin[0].address = "unique_input";
    tx.vout[0].address = "unique_output1";
    tx.vout[1].address = "unique_output2";
    std::unordered_set<std::string> seen;
    auto result = applyAddressReuse(tx, seen);
    assert(result["detected"] == false);
    std::cout << "✓ Address reuse: no reuse correctly not flagged\n";
}

void test_address_reuse_seen_set_grows() {
    Transaction tx = makeTx(1, 2);
    tx.vin[0].address = "addr_a";
    tx.vout[0].address = "addr_b";
    tx.vout[1].address = "addr_c";
    std::unordered_set<std::string> seen;
    applyAddressReuse(tx, seen);
    assert(seen.count("addr_a") > 0);
    assert(seen.count("addr_b") > 0);
    assert(seen.count("addr_c") > 0);
    std::cout << "✓ Address reuse: seen_addresses set populated correctly\n";
}

void test_address_reuse_empty_addresses() {
    Transaction tx = makeTx(1, 2);
    tx.vin[0].address = std::nullopt;
    tx.vout[0].address = std::nullopt;
    std::unordered_set<std::string> seen;
    auto result = applyAddressReuse(tx, seen);
    assert(result["detected"] == false);
    std::cout << "✓ Address reuse: empty addresses handled safely\n";
}

int main() {
    std::cout << "\n=== Address Reuse Tests ===\n\n";
    test_address_reuse_within_tx();
    test_address_reuse_cross_tx();
    test_address_reuse_no_reuse();
    test_address_reuse_seen_set_grows();
    test_address_reuse_empty_addresses();
    std::cout << "\n✓ All address reuse tests passed!\n\n";
    return 0;
}
