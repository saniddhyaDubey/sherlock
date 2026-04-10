#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include "analyzer.cpp"

bool fileExists(const std::string& path){
    std::ifstream f(path);
    return f.good();
}

int main(int argc, char* argv[]){
    if(argc < 5 || (std::string(argv[1]) != "--block" && std::string(argv[1]) != "--ui")){
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"INVALID_ARGS\",\"message\":\"Usage: ./block_parser --block|--ui <blk.dat> <rev.dat> <xor.dat>\"}}\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string blk_file = argv[2];
    std::string rev_file = argv[3];
    std::string xor_file = argv[4];

    if(!fileExists(blk_file)){
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"FILE_NOT_FOUND\",\"message\":\"Block file not found: " + blk_file + "\"}}\n";
        return 1;
    }
    if(!fileExists(rev_file)){
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"FILE_NOT_FOUND\",\"message\":\"Rev file not found: " + rev_file + "\"}}\n";
        return 1;
    }
    if(!fileExists(xor_file)){
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"FILE_NOT_FOUND\",\"message\":\"XOR file not found: " + xor_file + "\"}}\n";
        return 1;
    }

    // ensure out/ directory exists
    if(mkdir("out", 0755) != 0 && errno != EEXIST){
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"DIR_ERROR\",\"message\":\"Failed to create out/ directory\"}}\n";
        return 1;
    }

    try {
        std::vector<Block> all_blocks = parseAllBlocks(blk_file, rev_file, xor_file);
        if(all_blocks.empty()){
            std::cout << "{\"ok\":false,\"error\":{\"code\":\"PARSE_ERROR\",\"message\":\"No blocks parsed from: " + blk_file + "\"}}\n";
            return 1;
        }
        std::cerr << "Total blocks parsed: " << all_blocks.size() << "\n";

        // std::string file_name = parseFileName(blk_file);
        
        bool ui_mode = (mode == "--ui");
        std::string file_name = ui_mode ? "upload" : parseFileName(blk_file);
        FileAnalysisResult result = analyzeBlocks(all_blocks, file_name, ui_mode);

        if(ui_mode){
            std::cout << serializeUIResult(result) << "\n";
        } else {
            writeBlockOutput(result, file_name);
            generateMarkdownReport(result, all_blocks, file_name);
        }

        // FileAnalysisResult result = analyzeBlocks(all_blocks, file_name);
        // writeBlockOutput(result, file_name);
        // generateMarkdownReport(result, all_blocks, file_name);

    } catch(const std::exception& e){
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"RUNTIME_ERROR\",\"message\":\"" + std::string(e.what()) + "\"}}\n";
        return 1;
    } catch(...){
        std::cout << "{\"ok\":false,\"error\":{\"code\":\"UNKNOWN_ERROR\",\"message\":\"An unexpected error occurred\"}}\n";
        return 1;
    }

    return 0;
}
