#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <cmath>

#include "json.hpp"
#include "opcodes.h"
#include "picosha2.h"
#include "bech32.h"
#include "bech32.cpp"
#include "utf8.h"

// Custom structures for different sections of the Transaction:
// Input
struct Prevout {
    uint64_t value_sats;
    std::string script_pubkey_hex;
};

struct RTL {
    bool enabled;
    std::optional<std::string> type;
    std::optional<uint32_t> value;
};

struct Input {
    std::string txid;
    uint32_t vout;
    uint32_t sequence;
    std::string script_sig_hex;
    std::string script_asm;
    std::string witness_script_asm;
    std::vector<std::string> witness;
    std::string script_type;
    std::optional<std::string> address;
    Prevout prevout;
    RTL relative_timelock;
};

// Output
struct Output {
    uint64_t n;
    uint64_t value_sats;
    std::string script_pubkey_hex;
    std::string script_asm;
    std::string script_type;
    std::optional<std::string> address;
    std::optional<std::string> op_return_data_hex;
    std::optional<std::string> op_return_data_utf8;
    std::optional<std::string> op_return_protocol;
};

// Transaction
struct SegwitSavings {
    uint64_t witness_bytes;
    uint64_t non_witness_bytes;
    uint64_t total_bytes;
    uint64_t weight_actual;
    uint64_t weight_if_legacy;
    double savings_pct;
};

struct Warnings {
    std::string code;
};

struct Error {
    std::string code;
    std::string message;
};

struct Transaction {
    bool ok;
    std::string network;
    bool segwit;
    std::string txid;
    std::optional<std::string> wtxid;
    uint32_t version;
    uint32_t locktime;
    uint64_t size_bytes;
    uint64_t weight;
    uint64_t vbytes;
    uint64_t total_input_sats;
    uint64_t total_output_sats;
    uint64_t fee_sats;
    double fee_rate_sat_vb;
    bool rbf_signaling;
    std::string locktime_type;
    uint64_t locktime_value;
    std::optional<SegwitSavings> segwit_savings;
    std::vector<Input> vin;
    std::vector<Output> vout;
    std::vector<Warnings> warnings;
    std::optional<Error> error;
};

struct RawTxData {
    Transaction tx;
    std::vector<uint8_t> raw_bytes;
    std::vector<uint8_t> stripped_bytes;
    uint64_t segwit_data;
};

// Json-Input File
struct InputPrevoutJson {
    std::string txid;
    uint32_t vout;
    uint64_t value_sats;
    std::string script_pubkey_hex;
};

struct TxInputJson {
    std::string network;
    std::string raw_tx;
    std::vector<InputPrevoutJson> prevouts;
    bool parse_success;
};

//Block Parsing
struct BlockHeader {
    uint32_t version;
    std::string prev_block_hash;
    std::string merkle_root;
    bool merkle_root_valid;
    uint64_t timestamp;
    std::string bits;
    uint64_t nonce;
    std::string block_hash;
};

struct Coinbase {
    uint64_t bip34_height;
    std::string coinbase_script_hex;
    uint64_t total_output_sats;
};

struct ScriptSummary {
    uint64_t p2wpkh;
    uint64_t p2tr;
    uint64_t p2sh;
    uint64_t p2pkh;
    uint64_t p2wsh;
    uint64_t op_return;
    uint64_t unknown;
};

struct BlockStats {
    uint64_t total_fees_sats;
    uint64_t total_weight;
    double avg_fee_rate_sat_vb;
    ScriptSummary script_type_summary;
};

struct Block {
    bool ok;
    std::string mode;
    BlockHeader block_header;
    uint64_t tx_count;
    Coinbase coinbase;
    std::vector<Transaction> transactions;
    BlockStats block_stats;
};

struct RawBlockData {
    BlockHeader header;
    uint64_t tx_count;
    std::vector<RawTxData> txs;
    uint64_t coinbase_height;
    const std::vector<std::vector<Prevout>>* matching_rev = nullptr;
};
//--------
std::vector<uint8_t> hexToBytes(const std::string& hex_string){
    std::vector<uint8_t> byte_array;
    for(size_t i=0;i<hex_string.size();i+=2){
        std::string hex_substr = hex_string.substr(i,2);
        uint8_t bytes = static_cast<uint8_t>(std::stoi(hex_substr, nullptr, 16));
        byte_array.push_back(bytes);
    }

    return byte_array;
}

std::string bytesToHex(const std::vector<uint8_t>& bytes_array){
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for(uint8_t byte: bytes_array){
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

// CompactSize (standard Bitcoin varint) — used for vector lengths
uint64_t readVarInt(const std::vector<uint8_t>& bytes_array, size_t& offset){
    size_t start_point = offset;
    uint64_t data_length = 0;
    int i=0, d=0;
    if(bytes_array[start_point] <= 0xfc){
        offset += 1;
        data_length = static_cast<uint64_t>(bytes_array[start_point]);
        return data_length;
    }else if(bytes_array[start_point] == 0xfd) d=2;
    else if(bytes_array[start_point] == 0xfe) d=4;
    else d=8;
    while(i<d){
        ++start_point;
        data_length += static_cast<uint64_t>(bytes_array[start_point]) << (i*8);
        ++i;
    }
    offset += (d+1);
    return data_length;
}

std::vector<uint8_t> readBytes(const std::vector<uint8_t>& bytes_array, size_t& offset, size_t n){
    std::vector<uint8_t> bytes;
    for(size_t i=offset; i<(offset+n); ++i){
        bytes.push_back(bytes_array[i]);
    }
    offset += n;
    return bytes;
}

uint32_t readUint32(const std::vector<uint8_t>& bytes_array, size_t& offset){
    int i=0, n=4;
    std::vector<uint8_t> bytes = readBytes(bytes_array, offset, n);
    uint32_t data = 0;
    while(i<n){
        data += static_cast<uint32_t>(bytes[i]) << (i*8);
        ++i;
    }
    return data;
}

uint64_t readUint64(const std::vector<uint8_t>& bytes_array, size_t& offset){
    int i=0, n=8;
    std::vector<uint8_t> bytes = readBytes(bytes_array, offset, 8);
    uint64_t data = 0;
    while(i<n){
        data += static_cast<uint64_t>(bytes[i]) << (i*8);
        ++i;
    }
    return data;
}

Transaction parseTransaction(const std::vector<uint8_t>& bytes_array, size_t& offset, uint64_t& segwit_data, std::vector<uint8_t>& stripped_tx){
    Transaction new_tx;
    //pre-filling few data:
    new_tx.ok = true;
    new_tx.network = "mainnet";
    new_tx.segwit = false;

    // Save the transaction's starting offset (may be non-zero in block mode)
    size_t tx_start = offset;

    //parsing the version of transaction:
    new_tx.version = readUint32(bytes_array, offset);

    //copying version bytes to stripped_tx (using tx_start, not 0):
    size_t stripped_offset = tx_start;
    stripped_tx.insert(stripped_tx.end(), bytes_array.begin() + stripped_offset, bytes_array.begin() + offset);

    //conditional checking for SegWit or Legacy transaction:
    if(bytes_array[offset] == 0x00){
        if(bytes_array[offset+1] != 0x01){
            new_tx.ok = false;
            new_tx.error = Error{"INVALID_TX", "Invalid SegWit flag"};
            return new_tx;
        }

        new_tx.segwit = true; //setting the segwit flag to true;
        segwit_data += 2;

        //SegWit transaction:
        uint8_t segwit_marker = bytes_array[offset++];
        uint8_t segwit_flag = bytes_array[offset++];
    }

    stripped_offset = offset;

    //parsing the input count:
    uint64_t input_count = readVarInt(bytes_array, offset);

    //parsing for each input:
    while(input_count--){
        Input new_input;

        //filling txid and vout:
        std::vector<uint8_t> txid_bytes = readBytes(bytes_array, offset, 32);
        std::reverse(txid_bytes.begin(), txid_bytes.end());
        new_input.txid = bytesToHex(txid_bytes);
        new_input.vout = readUint32(bytes_array, offset);

        //filling scriptSig:
        uint64_t scriptSig_size = readVarInt(bytes_array, offset);
        std::vector<uint8_t> scriptSig_bytes = readBytes(bytes_array, offset, scriptSig_size);
        new_input.script_sig_hex = bytesToHex(scriptSig_bytes);

        new_input.sequence = readUint32(bytes_array, offset);

        //pushing in the transaction's vin vector:
        new_tx.vin.push_back(new_input);
    }

    //parsing the output count:
    uint64_t output_count = readVarInt(bytes_array, offset);

    //parsing for each output:
    for(uint64_t i=0; i<output_count; ++i){
        Output new_output;
        new_output.n = i;
        new_output.value_sats = readUint64(bytes_array, offset);

        //filling scriptPubkey:
        uint64_t scriptPK_size = readVarInt(bytes_array, offset);
        std::vector<uint8_t> scriptPK_bytes = readBytes(bytes_array, offset, scriptPK_size);
        new_output.script_pubkey_hex = bytesToHex(scriptPK_bytes);

        new_tx.vout.push_back(new_output);
    }

    stripped_tx.insert(stripped_tx.end(), bytes_array.begin() + stripped_offset, bytes_array.begin() + offset);

    //checking if segwit transaction then parse witness:
    uint64_t witness_start = offset;
    if(new_tx.segwit){
        for(Input& ip: new_tx.vin){
            uint64_t stack_item_count = readVarInt(bytes_array, offset);
            while(stack_item_count--){
                uint64_t item_size = readVarInt(bytes_array, offset);
                std::vector<uint8_t> item_bytes = readBytes(bytes_array, offset, item_size);
                ip.witness.push_back(bytesToHex(item_bytes));
            }
        }
    }
    segwit_data += (offset - witness_start);
    stripped_offset = offset;

    new_tx.locktime = readUint32(bytes_array, offset);

    stripped_tx.insert(stripped_tx.end(), bytes_array.begin() + stripped_offset, bytes_array.begin() + offset);

    return new_tx;
}

TxInputJson parseJsonFile(std::string& json_file_path){
    TxInputJson tx_data;

    std::ifstream file(json_file_path);

    if(!file.is_open()){
        tx_data.parse_success = false;
        return tx_data;
    }

    nlohmann::json file_data = nlohmann::json::parse(file);
    tx_data.network = file_data["network"];
    tx_data.raw_tx = file_data["raw_tx"];
    for(auto& pv: file_data["prevouts"]){
        InputPrevoutJson pvo_data;
        pvo_data.txid = pv["txid"];
        pvo_data.vout = pv["vout"];
        pvo_data.value_sats = pv["value_sats"];
        pvo_data.script_pubkey_hex = pv["script_pubkey_hex"];

        tx_data.prevouts.push_back(pvo_data);
    }

    tx_data.parse_success = true;

    return tx_data;
}

void fillPrevout(Transaction& tnx, std::vector<InputPrevoutJson>& prevouts){
    if(tnx.vin.size() != prevouts.size()){
        tnx.ok = false;
        tnx.error = Error{"INVALID_TX", "Invalid prevouts provided, length doesn't match"};
        return;
    }

    std::set<std::string> dup_check;
    for(InputPrevoutJson& ip_json: prevouts){
        std::string key = "#" + std::to_string(ip_json.vout) + "#" +ip_json.txid + "#";
        if(dup_check.find(key) != dup_check.end()){
            tnx.ok = false;
            tnx.error = Error{"INVALID_TX", "Duplicate prevouts found."};
            return;
        }
        dup_check.insert(key);
    }

    for(Input& ip: tnx.vin){
        bool flag = false;
        for(InputPrevoutJson& ip_json: prevouts){
            if(ip.txid == ip_json.txid && ip.vout == ip_json.vout){
                flag = true;

                Prevout pv;
                pv.value_sats = ip_json.value_sats;
                pv.script_pubkey_hex = ip_json.script_pubkey_hex;
                ip.prevout = pv;

                break;
            }
        }
        if(!flag){
            tnx.ok = false;
            tnx.error = Error{"INVALID_TX", "No prevout found for a given input"};
            return;
        }
    }
    tnx.ok=true;
}

// Classifies output script types.
// Valid output types per spec: p2pkh, p2sh, p2wpkh, p2wsh, p2tr, op_return, unknown
std::string classifyScript(std::vector<uint8_t>& script_pubkey_bytes){
    if(script_pubkey_bytes.empty()) return "unknown";

    if(script_pubkey_bytes[0] == 0x6a) return "op_return";
    else if(script_pubkey_bytes[0] == 0x00){
        if(script_pubkey_bytes.size() == 22 && script_pubkey_bytes[1] == 0x14) return "p2wpkh";
        else if(script_pubkey_bytes.size() == 34 && script_pubkey_bytes[1] == 0x20) return "p2wsh";
    }else if(script_pubkey_bytes[0] == 0xa9){
        if(script_pubkey_bytes.size() == 23 && script_pubkey_bytes[1] == 0x14) return "p2sh";
    }else if(script_pubkey_bytes[0] == 0x51){
        if(script_pubkey_bytes.size() == 34 && script_pubkey_bytes[1] == 0x20) return "p2tr";
    }else if(script_pubkey_bytes[0] == 0x76){
        if(script_pubkey_bytes.size() == 25 && script_pubkey_bytes[1] == 0xa9 && script_pubkey_bytes[2] == 0x14) return "p2pkh";
    }
    return "unknown";
}

std::string scriptToAsm(const std::vector<uint8_t>& script_bytes){
    std::string script_string = "";
    size_t i = 0;
    while(i < script_bytes.size()){
        uint8_t byte = script_bytes[i++];
        if(opcodeTable.find(byte) != opcodeTable.end()){
            script_string += opcodeTable[byte] + " ";
        }else if(byte >= 0x01 && byte <= 0x4b){
            uint32_t len = byte;
            if(len > script_bytes.size() - i) break;
            script_string += "OP_PUSHBYTES_" + std::to_string(len) + " ";
            script_string += bytesToHex(std::vector<uint8_t>(script_bytes.begin()+i, script_bytes.begin()+i+len)) + " ";
            i += len;
        }else if(byte == 0x4c){
            if(i >= script_bytes.size()) break;
            uint32_t len = script_bytes[i++];
            if(len > script_bytes.size() - i) break;
            script_string += "OP_PUSHDATA1 ";
            script_string += bytesToHex(std::vector<uint8_t>(script_bytes.begin()+i, script_bytes.begin()+i+len)) + " ";
            i += len;
        }else if(byte == 0x4d){
            if(i+2 > script_bytes.size()) break;
            uint32_t len = script_bytes[i] | (script_bytes[i+1] << 8);
            i += 2;
            if(len > script_bytes.size() - i) break;
            script_string += "OP_PUSHDATA2 ";
            script_string += bytesToHex(std::vector<uint8_t>(script_bytes.begin()+i, script_bytes.begin()+i+len)) + " ";
            i += len;
        }else if(byte == 0x4e){
            if(i+4 > script_bytes.size()) break;
            uint32_t len = script_bytes[i] | (script_bytes[i+1]<<8) | (script_bytes[i+2]<<16) | (script_bytes[i+3]<<24);
            i += 4;
            if(len > script_bytes.size() - i) break;
            script_string += "OP_PUSHDATA4 ";
            script_string += bytesToHex(std::vector<uint8_t>(script_bytes.begin()+i, script_bytes.begin()+i+len)) + " ";
            i += len;
        }else{
            std::stringstream ss;
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
            script_string += "OP_UNKNOWN_0x" + ss.str() + " ";
        }
    }
    if(!script_string.empty()) script_string.pop_back();
    return script_string;
}

std::vector<uint8_t> getLastPushedItem(const std::vector<uint8_t>& script_bytes){
    std::vector<uint8_t> last_item;
    for(size_t i=0; i<script_bytes.size(); ++i){
        uint8_t byte = script_bytes[i];
        size_t offset = i+1;
        if(opcodeTable.find(byte) != opcodeTable.end()) continue;
        else if(byte >= 0x01 && byte <= 0x4b){
            unsigned int data_length = static_cast<unsigned int>(byte);
            last_item = readBytes(script_bytes, offset, data_length);
        }else if(byte == 0x4c){
            uint8_t data_length = script_bytes[offset];
            ++offset;
            last_item = readBytes(script_bytes, offset, data_length);
        }else if(byte == 0x4d){
            uint16_t data_length = script_bytes[offset] | (script_bytes[offset+1] << 8);
            offset += 2;
            last_item = readBytes(script_bytes, offset, data_length);
        }else if(byte == 0x4e){
            uint32_t data_length = readUint32(script_bytes, offset);
            last_item = readBytes(script_bytes, offset, data_length);
        }
        i=offset-1;
    }
    return last_item;
}

void decodeOpReturn(Output& op, std::vector<uint8_t>& script_pubkey_bytes){
    size_t offset = 1;  //at index-0 we have 0x6a

    if(script_pubkey_bytes.size() <= 1){
        op.op_return_data_hex = "";
        op.op_return_data_utf8 = "";
        op.op_return_protocol = "unknown";
        return;
    }

    uint16_t data_length = 0;
    std::vector<uint8_t> all_data_bytes;
    while(offset < script_pubkey_bytes.size()){
        uint8_t byte = script_pubkey_bytes[offset++];
        if(byte == 0x00){
            data_length = 0;
        }else if(byte >= 0x01 && byte <= 0x4b){
            data_length = byte;
        }else if(byte == 0x4c){
            data_length = script_pubkey_bytes[offset++];
        }else if(byte == 0x4d){
            data_length = script_pubkey_bytes[offset] | (script_pubkey_bytes[offset+1] << 8);
            offset += 2;
        }else if(byte == 0x4e){
            data_length = readUint32(script_pubkey_bytes, offset);
        }
        std::vector<uint8_t> data_bytes = readBytes(script_pubkey_bytes, offset, data_length);
        all_data_bytes.insert(all_data_bytes.end(), data_bytes.begin(), data_bytes.end());
    }
    op.op_return_data_hex = bytesToHex(all_data_bytes);
    if(utf8::is_valid(all_data_bytes.begin(), all_data_bytes.end())){
        op.op_return_data_utf8 = std::string(all_data_bytes.begin(), all_data_bytes.end());
    } else {
        op.op_return_data_utf8 = std::nullopt;
    }
    std::string hex = op.op_return_data_hex.has_value() ? op.op_return_data_hex.value() : "";
    if(hex.substr(0, 8) == "6f6d6e69"){
        op.op_return_protocol = "omni";
    } else if(hex.substr(0, 10) == "0109f91102"){
        op.op_return_protocol = "opentimestamps";
    } else {
        op.op_return_protocol = "unknown";
    }
}

std::vector<uint8_t> doubleSHA256(const std::vector<uint8_t>& data){
    std::vector<uint8_t> hash1(32), hash2(32);
    picosha2::hash256(data, hash1);
    picosha2::hash256(hash1, hash2);
    return hash2;
}

//Base-58 custom code:
static const std::string BASE58_CHARS =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::string base58Encode(const std::vector<uint8_t>& data){
    std::string result;
    int leading_zeros = 0;

    for(uint8_t byte: data){
        if(byte == 0) leading_zeros++;
        else break;
    }

    std::vector<uint8_t> temp(data.begin(), data.end());
    while(!temp.empty()){
        int remainder = 0;
        std::vector<uint8_t> next;
        for(uint8_t byte: temp){
            int value = remainder * 256 + byte;
            if(!next.empty() || value / 58 > 0)
                next.push_back(value / 58);
            remainder = value % 58;
        }
        result += BASE58_CHARS[remainder];
        temp = next;
    }

    for(int i=0; i<leading_zeros; ++i)
        result += BASE58_CHARS[0];

    std::reverse(result.begin(), result.end());
    return result;
}

std::string base58CheckEncode(uint8_t version, const std::vector<uint8_t>& hash){
    std::vector<uint8_t> data;
    data.push_back(version);
    data.insert(data.end(), hash.begin(), hash.end());

    std::vector<uint8_t> checksum = doubleSHA256(data);
    data.insert(data.end(), checksum.begin(), checksum.begin() + 4);

    return base58Encode(data);
}
//--

std::vector<uint8_t> convertbits(const std::vector<uint8_t>& data, int frombits, int tobits){
    std::vector<uint8_t> ret;
    int acc = 0, bits = 0;
    for(uint8_t value : data){
        acc = ((acc << frombits) | value) & 0x3fffffff;
        bits += frombits;
        while(bits >= tobits){
            bits -= tobits;
            ret.push_back((acc >> bits) & ((1 << tobits) - 1));
        }
    }
    if(bits > 0) ret.push_back((acc << (tobits - bits)) & ((1 << tobits) - 1));
    return ret;
}

std::string deriveAddress(const std::string& script_pubkey_hex, const std::string& script_type){
    if(script_pubkey_hex.empty()) return "";

    std::vector<uint8_t> script_bytes = hexToBytes(script_pubkey_hex);
    std::string address_string;
    if(script_type == "p2pkh"){
        size_t offset = 3;
        std::vector<uint8_t> pk_bytes = readBytes(script_bytes, offset, 20);
        address_string = base58CheckEncode(0x00, pk_bytes);
    }else if(script_type == "p2sh"){
        size_t offset = 2;
        std::vector<uint8_t> pk_bytes = readBytes(script_bytes, offset, 20);
        address_string = base58CheckEncode(0x05, pk_bytes);
    }else if(script_type == "p2wpkh"){
        size_t offset = 2;
        std::vector<uint8_t> pk_bytes = readBytes(script_bytes, offset, 20);
        std::vector<uint8_t> converted = convertbits(pk_bytes, 8, 5);
        converted.insert(converted.begin(), 0x00); // witness version
        address_string = bech32::encode("bc", converted, bech32::Encoding::BECH32);
    }else if(script_type == "p2wsh"){
        size_t offset = 2;
        std::vector<uint8_t> pk_bytes = readBytes(script_bytes, offset, 32);
        std::vector<uint8_t> converted = convertbits(pk_bytes, 8, 5);
        converted.insert(converted.begin(), 0x00);
        address_string = bech32::encode("bc", converted, bech32::Encoding::BECH32);
    }else if(script_type == "p2tr"){
        size_t offset = 2;
        std::vector<uint8_t> pk_bytes = readBytes(script_bytes, offset, 32);
        std::vector<uint8_t> converted = convertbits(pk_bytes, 8, 5);
        converted.insert(converted.begin(), 0x01);
        address_string = bech32::encode("bc", converted, bech32::Encoding::BECH32M);
    }
    return address_string;
}

void postProcessTransaction(Transaction& tx, const std::string& network, uint64_t segwit_data, const std::vector<uint8_t>& raw_tx_bytes, const std::vector<uint8_t>& stripped_tx){
    tx.rbf_signaling = false;
    for(size_t idx = 0; idx < tx.vin.size(); idx++){
        Input& ip = tx.vin[idx];

        // BIP125: RBF if any input has sequence < 0xFFFFFFFE
        if(ip.sequence < 0xFFFFFFFEU) tx.rbf_signaling = true;
        std::vector<uint8_t> script_pk_bytes = hexToBytes(ip.prevout.script_pubkey_hex);
        ip.script_type = classifyScript(script_pk_bytes);
        if(ip.script_type == "p2sh"){
            std::vector<uint8_t> script_sig_bytes = hexToBytes(ip.script_sig_hex);
            std::vector<uint8_t> redeem_script = getLastPushedItem(script_sig_bytes);
            if(!redeem_script.empty()){
                std::string inner_type = classifyScript(redeem_script);
                if(inner_type == "p2wpkh") ip.script_type = "p2sh-p2wpkh";
                else if(inner_type == "p2wsh") ip.script_type = "p2sh-p2wsh";
                else ip.script_type = "unknown";
            }else if(!ip.witness.empty()){
                std::vector<uint8_t> witness_script = hexToBytes(ip.witness.back());
                std::string inner_type = classifyScript(witness_script);
                if(inner_type == "p2wsh") ip.script_type = "p2sh-p2wsh";
                else ip.script_type = "p2sh-p2wpkh";
            }else{
                ip.script_type = "unknown";
            }
        }
        if(ip.script_type == "p2tr"){
            if(ip.witness.size() == 1) ip.script_type = "p2tr_keypath";
            else if(ip.witness.size() >= 2) ip.script_type = "p2tr_scriptpath";
        }

        ip.script_asm = ip.script_sig_hex.empty() ? "" : scriptToAsm(hexToBytes(ip.script_sig_hex));
        if(ip.script_type == "p2wsh" || ip.script_type == "p2sh-p2wsh"){
            if(!ip.witness.empty())
                ip.witness_script_asm = scriptToAsm(hexToBytes(ip.witness.back()));
        }

        RTL ip_rtl;
        if(ip.sequence & (1U << 31)){
            ip_rtl.enabled = false;
        }else{
            ip_rtl.enabled = true;
            //BIT 22: 0=blocks, 1=time
            ip_rtl.type = (ip.sequence & (1U << 22)) ? "time" : "blocks";
            ip_rtl.value = ip.sequence & 0xFFFFU;
        }
        ip.relative_timelock = ip_rtl;

        //Address from prevout script (use prevout script_type for input classification):
        std::string addr_script_type = ip.script_type;
        //For nested segwit, derive from the prevout (p2sh)
        if(addr_script_type == "p2sh-p2wpkh" || addr_script_type == "p2sh-p2wsh")
            addr_script_type = "p2sh";
        if(addr_script_type != "unknown" && addr_script_type != "op_return" &&
           addr_script_type != "p2tr_keypath" && addr_script_type != "p2tr_scriptpath"){
            ip.address = deriveAddress(ip.prevout.script_pubkey_hex, addr_script_type);
        }else if(addr_script_type == "p2tr_keypath" || addr_script_type == "p2tr_scriptpath"){
            ip.address = deriveAddress(ip.prevout.script_pubkey_hex, "p2tr");
        }
    }

    for(Output& op: tx.vout){
        std::vector<uint8_t> script_pk_bytes = hexToBytes(op.script_pubkey_hex);
        op.script_type = classifyScript(script_pk_bytes);
        if(op.script_type == "op_return") decodeOpReturn(op, script_pk_bytes);
        op.script_asm = scriptToAsm(script_pk_bytes);
        if(op.script_type != "unknown" && op.script_type != "op_return"){
            op.address = deriveAddress(op.script_pubkey_hex, op.script_type);
        }
    }

    uint64_t total_in = 0, total_out = 0;
    for(Input& ip: tx.vin) total_in += ip.prevout.value_sats;
    for(Output& op: tx.vout) total_out += op.value_sats;
    tx.total_input_sats = total_in;
    tx.total_output_sats = total_out;
    // Guard against unsigned underflow (e.g. coinbase)
    tx.fee_sats = (total_in >= total_out) ? (total_in - total_out) : 0;
    tx.size_bytes = raw_tx_bytes.size();
    uint64_t non_witness_size = tx.size_bytes - segwit_data;
    tx.weight = (non_witness_size * 4) + segwit_data;
    tx.vbytes = (tx.weight + 3) / 4;
    tx.fee_rate_sat_vb = (tx.vbytes > 0) ? std::round((double)tx.fee_sats / tx.vbytes * 100.0) / 100.0 : 0.0;
    if(tx.segwit){
        SegwitSavings ss;
        ss.witness_bytes = segwit_data;
        ss.non_witness_bytes = non_witness_size;
        ss.total_bytes = tx.size_bytes;
        ss.weight_actual = tx.weight;
        ss.weight_if_legacy = tx.size_bytes * 4;
        ss.savings_pct = std::round(((double)(ss.weight_if_legacy - ss.weight_actual) / ss.weight_if_legacy) * 10000.0) / 100.0;
        tx.segwit_savings = ss;
    }
    tx.locktime_type = tx.locktime == 0 ? "none" : (tx.locktime <= 499999999 ? "block_height" : "unix_timestamp");
    tx.locktime_value = tx.locktime;
    std::vector<uint8_t> stripped_hash = doubleSHA256(stripped_tx);
    std::reverse(stripped_hash.begin(), stripped_hash.end());
    tx.txid = bytesToHex(stripped_hash);
    if(tx.segwit){
        std::vector<uint8_t> full_hash = doubleSHA256(raw_tx_bytes);
        std::reverse(full_hash.begin(), full_hash.end());
        tx.wtxid = bytesToHex(full_hash);
    }

    // Warnings — all four required codes
    if(tx.fee_sats > 1000000 || tx.fee_rate_sat_vb > 200.0){
        tx.warnings.push_back(Warnings{"HIGH_FEE"});
    }

    bool has_dust = false;
    for(const Output& op: tx.vout){
        if(op.script_type != "op_return" && op.value_sats < 546){ has_dust = true; break; }
    }
    if(has_dust) tx.warnings.push_back(Warnings{"DUST_OUTPUT"});
    bool has_unknown_out = false;
    for(const Output& op: tx.vout){
        if(op.script_type == "unknown"){ has_unknown_out = true; break; }
    }
    if(has_unknown_out) tx.warnings.push_back(Warnings{"UNKNOWN_OUTPUT_SCRIPT"});
    if(tx.rbf_signaling) tx.warnings.push_back(Warnings{"RBF_SIGNALING"});
}

nlohmann::json serializeTransaction(const Transaction& tx){
    nlohmann::json j;
    j["ok"] = tx.ok;
    j["network"] = tx.network;
    j["segwit"] = tx.segwit;
    j["txid"] = tx.txid;
    j["wtxid"] = tx.wtxid.has_value() ? nlohmann::json(tx.wtxid.value()) : nlohmann::json(nullptr);
    j["version"] = tx.version;
    j["locktime"] = tx.locktime;
    j["size_bytes"] = tx.size_bytes;
    j["weight"] = tx.weight;
    j["vbytes"] = tx.vbytes;
    j["total_input_sats"] = tx.total_input_sats;
    j["total_output_sats"] = tx.total_output_sats;
    j["fee_sats"] = tx.fee_sats;
    j["fee_rate_sat_vb"] = tx.fee_rate_sat_vb;
    j["rbf_signaling"] = tx.rbf_signaling;
    j["locktime_type"] = tx.locktime_type;
    j["locktime_value"] = tx.locktime_value;

    if(tx.segwit_savings.has_value()){
        nlohmann::json j_ss;
        j_ss["witness_bytes"] = tx.segwit_savings->witness_bytes;
        j_ss["non_witness_bytes"] = tx.segwit_savings->non_witness_bytes;
        j_ss["total_bytes"] = tx.segwit_savings->total_bytes;
        j_ss["weight_actual"] = tx.segwit_savings->weight_actual;
        j_ss["weight_if_legacy"] = tx.segwit_savings->weight_if_legacy;
        j_ss["savings_pct"] = tx.segwit_savings->savings_pct;
        j["segwit_savings"] = j_ss;
    }else{
        j["segwit_savings"] = nlohmann::json(nullptr);
    }

    nlohmann::json vin_array = nlohmann::json::array();
    for(const Input& ip: tx.vin){
        nlohmann::json j_ip;
        j_ip["txid"] = ip.txid;
        j_ip["vout"] = ip.vout;
        j_ip["sequence"] = ip.sequence;
        j_ip["script_sig_hex"] = ip.script_sig_hex;
        j_ip["script_asm"] = ip.script_asm;
        // witness_script_asm only for P2WSH / P2SH-P2WSH inputs
        if(!ip.witness_script_asm.empty())
            j_ip["witness_script_asm"] = ip.witness_script_asm;
        j_ip["witness"] = ip.witness;
        j_ip["script_type"] = ip.script_type;
        j_ip["address"] = ip.address.has_value() ? nlohmann::json(ip.address.value()) : nlohmann::json(nullptr);

        nlohmann::json j_ip_prevout;
        j_ip_prevout["value_sats"] = ip.prevout.value_sats;
        j_ip_prevout["script_pubkey_hex"] = ip.prevout.script_pubkey_hex;
        j_ip["prevout"] = j_ip_prevout;

        nlohmann::json j_ip_rtl;
        j_ip_rtl["enabled"] = ip.relative_timelock.enabled;
        if(ip.relative_timelock.enabled){
            j_ip_rtl["type"] = ip.relative_timelock.type.has_value() ? nlohmann::json(ip.relative_timelock.type.value()) : nlohmann::json(nullptr);
            j_ip_rtl["value"] = ip.relative_timelock.value.has_value() ? nlohmann::json(ip.relative_timelock.value.value()) : nlohmann::json(nullptr);
        }
        j_ip["relative_timelock"] = j_ip_rtl;

        vin_array.push_back(j_ip);
    }
    j["vin"] = vin_array;

    nlohmann::json vout_array = nlohmann::json::array();
    for(const Output& op:tx.vout){
        nlohmann::json j_op;
        j_op["n"] = op.n;
        j_op["value_sats"] = op.value_sats;
        j_op["script_pubkey_hex"] = op.script_pubkey_hex;
        j_op["script_asm"] = op.script_asm;
        j_op["script_type"] = op.script_type;
        j_op["address"] = op.address.has_value() ? nlohmann::json(op.address.value()) : nlohmann::json(nullptr);
        if(op.script_type == "op_return"){
            j_op["op_return_data_hex"] = op.op_return_data_hex.has_value() ? nlohmann::json(op.op_return_data_hex.value()) : nlohmann::json("");
            j_op["op_return_data_utf8"] = op.op_return_data_utf8.has_value() ? nlohmann::json(op.op_return_data_utf8.value()) : nlohmann::json(nullptr);
            j_op["op_return_protocol"] = op.op_return_protocol.has_value() ? nlohmann::json(op.op_return_protocol.value()) : nlohmann::json("unknown");
        }
        vout_array.push_back(j_op);
    }
    j["vout"] = vout_array;

    nlohmann::json warnings_array = nlohmann::json::array();
    for(const Warnings& w: tx.warnings){
        nlohmann::json j_w;
        j_w["code"] = w.code;
        warnings_array.push_back(j_w);
    }
    j["warnings"] = warnings_array;

    return j;
}

BlockHeader parseBlockHeader(const std::vector<uint8_t>& bytes, size_t& offset){
    BlockHeader header;
    size_t header_start = offset;
    header.version = readUint32(bytes, offset);
    std::vector<uint8_t> prev_block_hash_bytes = readBytes(bytes, offset, 32);
    std::reverse(prev_block_hash_bytes.begin(), prev_block_hash_bytes.end());
    header.prev_block_hash = bytesToHex(prev_block_hash_bytes);
    std::vector<uint8_t> merkle_root_bytes = readBytes(bytes, offset, 32);
    std::reverse(merkle_root_bytes.begin(), merkle_root_bytes.end());
    header.merkle_root = bytesToHex(merkle_root_bytes);
    header.timestamp = readUint32(bytes, offset);
    //bits: 4 raw bytes, display as 8-char hex of the little-endian uint32 value
    uint32_t bits_val = readUint32(bytes, offset);
    std::stringstream bits_ss;
    bits_ss << std::hex << std::setfill('0') << std::setw(8) << bits_val;
    header.bits = bits_ss.str();
    header.nonce = readUint32(bytes, offset);
    std::vector<uint8_t> header_bytes(bytes.begin() + header_start, bytes.begin() + offset);
    std::vector<uint8_t> double_sha_header = doubleSHA256(header_bytes);
    std::reverse(double_sha_header.begin(), double_sha_header.end());
    header.block_hash = bytesToHex(double_sha_header);
    return header;
}

//VarInt-VI: Bitcoin Core's base-128 encoding used INSIDE Coin records.
//CRITICAL: adds 1 on each continuation byte — different from standard base-128.
uint64_t readVarIntVI(const std::vector<uint8_t>& bytes, size_t& offset){
    uint64_t n = 0;
    while(offset < bytes.size()){
        uint8_t ch = bytes[offset++];
        n = (n << 7) | (ch & 0x7F);
        if(ch & 0x80){
            n += 1;
        } else {
            break;
        }
    }
    return n;
}

uint64_t decompressAmount(uint64_t x){
    if(x == 0) return 0;
    x--;
    uint64_t e = x % 10;
    x /= 10;
    uint64_t n;
    if(e < 9){
        uint64_t d = (x % 9) + 1;
        x /= 9;
        n = x * 10 + d;
    } else {
        n = x + 1;
    }
    while(e--) n *= 10;
    return n;
}

std::string decompressScript(const std::vector<uint8_t>& bytes, size_t& offset){
    uint64_t nSize = readVarIntVI(bytes, offset);

    if(nSize == 0){
        //P2PKH: OP_DUP OP_HASH160 <20-byte-hash> OP_EQUALVERIFY OP_CHECKSIG
        std::vector<uint8_t> hash = readBytes(bytes, offset, 20);
        std::vector<uint8_t> script = {0x76, 0xa9, 0x14};
        script.insert(script.end(), hash.begin(), hash.end());
        script.push_back(0x88);
        script.push_back(0xac);
        return bytesToHex(script);
    } else if(nSize == 1){
        //P2SH: OP_HASH160 <20-byte-hash> OP_EQUAL
        std::vector<uint8_t> hash = readBytes(bytes, offset, 20);
        std::vector<uint8_t> script = {0xa9, 0x14};
        script.insert(script.end(), hash.begin(), hash.end());
        script.push_back(0x87);
        return bytesToHex(script);
    } else if(nSize == 2 || nSize == 3){
        //P2PK compressed: <33-byte-pubkey> OP_CHECKSIG
        //prefix = nSize (02 or 03)
        std::vector<uint8_t> xcoord = readBytes(bytes, offset, 32);
        std::vector<uint8_t> script = {0x21, (uint8_t)nSize};
        script.insert(script.end(), xcoord.begin(), xcoord.end());
        script.push_back(0xac);
        return bytesToHex(script);
    } else if(nSize == 4 || nSize == 5){
        //P2PK compressed (stored from uncompressed): prefix = 02 if nSize=4, 03 if nSize=5
        //formula: prefix = 2 + (nSize & 1)
        uint8_t prefix = 2 + (nSize & 1);
        std::vector<uint8_t> xcoord = readBytes(bytes, offset, 32);
        std::vector<uint8_t> script = {0x21, prefix};
        script.insert(script.end(), xcoord.begin(), xcoord.end());
        script.push_back(0xac);
        return bytesToHex(script);
    } else {
        //Raw script: length = nSize - 6
        uint64_t script_len = nSize - 6;
        std::vector<uint8_t> script = readBytes(bytes, offset, script_len);
        return bytesToHex(script);
    }
}

uint64_t decodeBIP34Height(const std::string& script_sig_hex){
    if(script_sig_hex.empty()) return 0;
    std::vector<uint8_t> bytes = hexToBytes(script_sig_hex);
    if(bytes.empty()) return 0;
    uint8_t len = bytes[0];
    if(len == 0 || len > 8 || (size_t)(len + 1) > bytes.size()) return 0;
    uint64_t height = 0;
    for(int i = 0; i < len; i++){
        height |= (uint64_t)bytes[1 + i] << (i * 8);
    }
    return height;
}

std::string computeMerkleRoot(const std::vector<Transaction>& txs){
    std::vector<std::vector<uint8_t>> hashes;
    for(const Transaction& tx: txs){
        std::vector<uint8_t> txid_bytes = hexToBytes(tx.txid);
        std::reverse(txid_bytes.begin(), txid_bytes.end());
        hashes.push_back(txid_bytes);
    }
    while(hashes.size() > 1){
        if(hashes.size() % 2 != 0) hashes.push_back(hashes.back());
        std::vector<std::vector<uint8_t>> new_hashes;
        for(size_t i = 0; i < hashes.size(); i += 2){
            std::vector<uint8_t> combined;
            combined.insert(combined.end(), hashes[i].begin(), hashes[i].end());
            combined.insert(combined.end(), hashes[i+1].begin(), hashes[i+1].end());
            new_hashes.push_back(doubleSHA256(combined));
        }
        hashes = new_hashes;
    }
    std::vector<uint8_t> root = hashes[0];
    std::reverse(root.begin(), root.end());
    return bytesToHex(root);
}

void computeBlockStats(Block& block){
    BlockStats stats;
    stats.total_fees_sats = 0;
    stats.total_weight = 0;
    stats.script_type_summary = {0,0,0,0,0,0,0};

    for(size_t i = 0; i < block.transactions.size(); i++){
        const Transaction& tx = block.transactions[i];
        //Include all transactions in total_weight; fees only from non-coinbase
        stats.total_weight += tx.weight;
        if(i == 0) continue; // skip coinbase for fees
        stats.total_fees_sats += tx.fee_sats;
        for(const Output& op: tx.vout){
            if(op.script_type == "p2wpkh") stats.script_type_summary.p2wpkh++;
            else if(op.script_type == "p2tr") stats.script_type_summary.p2tr++;
            else if(op.script_type == "p2sh") stats.script_type_summary.p2sh++;
            else if(op.script_type == "p2pkh") stats.script_type_summary.p2pkh++;
            else if(op.script_type == "p2wsh") stats.script_type_summary.p2wsh++;
            else if(op.script_type == "op_return") stats.script_type_summary.op_return++;
            else stats.script_type_summary.unknown++;
        }
    }

    uint64_t total_vbytes = (stats.total_weight + 3) / 4;
    stats.avg_fee_rate_sat_vb = total_vbytes > 0
        ? std::round((double)stats.total_fees_sats / total_vbytes * 100.0) / 100.0
        : 0.0;
    block.block_stats = stats;
}

nlohmann::json serializeBlock(const Block& block){
    nlohmann::json j;
    j["ok"] = true;
    j["mode"] = "block";

    //block header
    j["block_header"]["version"] = block.block_header.version;
    j["block_header"]["prev_block_hash"] = block.block_header.prev_block_hash;
    j["block_header"]["merkle_root"] = block.block_header.merkle_root;
    j["block_header"]["merkle_root_valid"] = block.block_header.merkle_root_valid;
    j["block_header"]["timestamp"] = block.block_header.timestamp;
    j["block_header"]["bits"] = block.block_header.bits;
    j["block_header"]["nonce"] = block.block_header.nonce;
    j["block_header"]["block_hash"] = block.block_header.block_hash;

    j["tx_count"] = block.tx_count;

    //coinbase
    j["coinbase"]["bip34_height"] = block.coinbase.bip34_height;
    j["coinbase"]["coinbase_script_hex"] = block.coinbase.coinbase_script_hex;
    j["coinbase"]["total_output_sats"] = block.coinbase.total_output_sats;

    //block stats
    j["block_stats"]["total_fees_sats"] = block.block_stats.total_fees_sats;
    j["block_stats"]["total_weight"] = block.block_stats.total_weight;
    j["block_stats"]["avg_fee_rate_sat_vb"] = block.block_stats.avg_fee_rate_sat_vb;
    j["block_stats"]["script_type_summary"]["p2wpkh"] = block.block_stats.script_type_summary.p2wpkh;
    j["block_stats"]["script_type_summary"]["p2tr"] = block.block_stats.script_type_summary.p2tr;
    j["block_stats"]["script_type_summary"]["p2sh"] = block.block_stats.script_type_summary.p2sh;
    j["block_stats"]["script_type_summary"]["p2pkh"] = block.block_stats.script_type_summary.p2pkh;
    j["block_stats"]["script_type_summary"]["p2wsh"] = block.block_stats.script_type_summary.p2wsh;
    j["block_stats"]["script_type_summary"]["op_return"] = block.block_stats.script_type_summary.op_return;
    j["block_stats"]["script_type_summary"]["unknown"] = block.block_stats.script_type_summary.unknown;

    //transactions
    j["transactions"] = nlohmann::json::array();
    for(const Transaction& tx: block.transactions){
        j["transactions"].push_back(serializeTransaction(tx));
    }

    return j;
}

std::vector<uint8_t> readFileBytes(const std::string& file_path){
    std::ifstream file(file_path, std::ios::binary);
    if(!file.is_open()){
        std::cerr << "Error opening file: " << file_path << "\n";
        return {};
    }
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

// XOR-decode data in place using the obfuscation key from xor.dat.
// If key is all zeros, no transformation occurs.
void xorDecodeInPlace(std::vector<uint8_t>& data, const std::vector<uint8_t>& xor_key){
    if(xor_key.empty()) return;
    bool all_zero = std::all_of(xor_key.begin(), xor_key.end(), [](uint8_t b){ return b == 0; });
    if(all_zero) return;
    for(size_t i = 0; i < data.size(); i++)
        data[i] ^= xor_key[i % xor_key.size()];
}

Prevout parseCoin(const std::vector<uint8_t>& bytes, size_t& offset){
    if(offset >= bytes.size()) return Prevout{};
    Prevout prevout;
    uint64_t nCode = readVarIntVI(bytes, offset);
    uint64_t height = nCode >> 1;
    //nVersion is present whenever height > 0 (always true for real blocks)
    if(height > 0) readVarIntVI(bytes, offset);
    uint64_t compressed_amount = readVarIntVI(bytes, offset);
    prevout.value_sats = decompressAmount(compressed_amount);
    prevout.script_pubkey_hex = decompressScript(bytes, offset);
    return prevout;
}

//Parse all undo records from XOR-decoded rev file.
//Returns: vector of blocks, each block is vector of txs, each tx is vector of Prevouts.
//Rev records are in block height order (ascending).
std::vector<std::vector<std::vector<Prevout>>> parseAllRevRecords(const std::vector<uint8_t>& rev_bytes){
    std::vector<std::vector<std::vector<Prevout>>> all_records;
    size_t offset = 0;
    while(offset + 8 <= rev_bytes.size()){
        //Check magic: 0xF9BEB4D9 (mainnet) stored as F9 BE B4 D9
        if(rev_bytes[offset] != 0xF9 || rev_bytes[offset+1] != 0xBE ||
           rev_bytes[offset+2] != 0xB4 || rev_bytes[offset+3] != 0xD9){
            //Skip padding or end
            offset++;
            continue;
        }
        offset += 4; //magic
        uint32_t size = readUint32(rev_bytes, offset);
        if(offset + size > rev_bytes.size()) break;
        size_t record_start = offset;
        std::vector<std::vector<Prevout>> record;
        //vtxundo_count uses CompactSize
        uint64_t tnx_count = readVarInt(rev_bytes, offset);
        bool valid = true;
        while(tnx_count--){
            std::vector<Prevout> pv_list;
            //coin_count uses CompactSize
            uint64_t vin_count = readVarInt(rev_bytes, offset);
            while(vin_count--){
                if(offset >= rev_bytes.size()){ valid = false; break; }
                Prevout pv = parseCoin(rev_bytes, offset);
                pv_list.push_back(pv);
            }
            if(!valid) break;
            record.push_back(pv_list);
        }
        //Advance to exactly record_start + size (skip remaining + 32-byte checksum)
        offset = record_start + size + 32;
        if(valid) all_records.push_back(record);
    }
    return all_records;
}

std::string parseFileName(std::string blk_file){
    auto start_index = blk_file.find("blk"), end_index = blk_file.rfind(".dat");
    return blk_file.substr(start_index, end_index - start_index);
}
 
std::vector<Block> parseAllBlocks(std::string blk_file, std::string rev_file, std::string xor_file){
    std::vector<uint8_t> xor_key = readFileBytes(xor_file);
    std::vector<uint8_t> blk_bytes = readFileBytes(blk_file);
    std::vector<uint8_t> rev_bytes = readFileBytes(rev_file);
    xorDecodeInPlace(blk_bytes, xor_key);
    xorDecodeInPlace(rev_bytes, xor_key);

    auto all_revs = parseAllRevRecords(rev_bytes);

    // Pass 1: parse all raw blocks
    struct RawBlock {
        BlockHeader header;
        uint64_t tx_count;
        uint64_t block_height;
        std::vector<RawTxData> raw_txs;
    };

    std::vector<RawBlock> raw_blocks;
    size_t blk_offset = 0;

    while(blk_offset + 8 < blk_bytes.size()){
        if(blk_bytes[blk_offset] != 0xF9 || blk_bytes[blk_offset+1] != 0xBE ||
            blk_bytes[blk_offset+2] != 0xB4 || blk_bytes[blk_offset+3] != 0xD9){
            blk_offset++;
            continue;
        }
        blk_offset += 4;
        readUint32(blk_bytes, blk_offset);

        BlockHeader header = parseBlockHeader(blk_bytes, blk_offset);
        uint64_t tx_count = readVarInt(blk_bytes, blk_offset);

        std::vector<RawTxData> raw_txs;
        for(uint64_t i = 0; i < tx_count; i++){
            size_t tx_start = blk_offset;
            uint64_t segwit_data = 0;
            std::vector<uint8_t> stripped;
            Transaction tx = parseTransaction(blk_bytes, blk_offset, segwit_data, stripped);
            RawTxData td;
            td.tx = std::move(tx);
            td.raw_bytes = std::vector<uint8_t>(blk_bytes.begin() + tx_start, blk_bytes.begin() + blk_offset);
            td.stripped_bytes = std::move(stripped);
            td.segwit_data = segwit_data;
            raw_txs.push_back(std::move(td));
        }

        RawBlock rb;
        rb.header = header;
        rb.tx_count = tx_count;
        rb.block_height = decodeBIP34Height(raw_txs[0].tx.vin[0].script_sig_hex);
        rb.raw_txs = std::move(raw_txs);
        raw_blocks.push_back(std::move(rb));
    }

    // Pass 2: sort by block height
    std::sort(raw_blocks.begin(), raw_blocks.end(),
        [](const RawBlock& a, const RawBlock& b){ return a.block_height < b.block_height; });

    // Pass 3: match rev records and assemble blocks
    std::vector<Block> all_blocks;
    size_t rev_search_start = 0;

    for(auto& rb : raw_blocks){
        const std::vector<std::vector<Prevout>>* matching_rev = nullptr;
        for(size_t ri = rev_search_start; ri < all_revs.size(); ri++){
            if(all_revs[ri].size() == (rb.tx_count - 1)){
                matching_rev = &all_revs[ri];
                rev_search_start = ri + 1;
                break;
            }
        }

        Block block;
        block.ok = true;
        block.mode = "block";
        block.block_header = rb.header;
        block.tx_count = rb.tx_count;

        for(size_t ti = 0; ti < rb.raw_txs.size(); ti++){
            RawTxData& td = rb.raw_txs[ti];

            if(ti == 0){
                for(auto& inp : td.tx.vin) inp.prevout = Prevout{0, ""};
            } else if(matching_rev != nullptr){
                const auto& coin_list = (*matching_rev)[ti - 1];
                for(size_t ii = 0; ii < td.tx.vin.size() && ii < coin_list.size(); ii++)
                    td.tx.vin[ii].prevout = coin_list[ii];
            } else {
                for(auto& inp : td.tx.vin) inp.prevout = Prevout{0, ""};
            }
            postProcessTransaction(td.tx, "mainnet", td.segwit_data, td.raw_bytes, td.stripped_bytes);
            block.transactions.push_back(std::move(td.tx));
        }

        block.coinbase.bip34_height = rb.block_height;
        block.coinbase.coinbase_script_hex = block.transactions[0].vin[0].script_sig_hex;
        uint64_t total_out = 0;
        for(auto& v : block.transactions[0].vout) total_out += v.value_sats;
        block.coinbase.total_output_sats = total_out;
        block.block_header.merkle_root_valid = (computeMerkleRoot(block.transactions) == block.block_header.merkle_root);
        computeBlockStats(block);

        all_blocks.push_back(std::move(block));
    }

    return all_blocks;
}
