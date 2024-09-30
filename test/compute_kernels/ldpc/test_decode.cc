#include <cstdint>
#include <memory>
#include "encoder.h"
#include "memory_manage.h"
#include "phy_ldpc_decoder_5gnr.h"
#include "symbols.h"
#include "utils_ldpc.h"
#include "scrambler.h"
#include <random>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>
#include <sstream>

#define VALUE_DELIMITER ","

// const char input0[] = "0x7F817F81, 0x7F81817F, 0x7F7F7F7F, 0x81818181, 0x7F81817F, 0x817F8181, 0x817F7F7F, 0x7F817F81, 0x817F7F7F, 0x8181817F, 0x8181817F";
const char input0[] = "0x04030201, 0x08070605, 0x0A09";
const char output0[] = "0x8C4DEB9F, 0x52";

size_t calculateArraySize(const char input[]) {
    size_t count = 0;
    const char *prefix = "0x";
    const char *position = input;

    while ((position = strstr(position, prefix)) != NULL) {
        count++;
        position += strlen(prefix); // Move past the current "0x"
    }

    return count;
}

std::vector<uint32_t> convert_to_uint32(const int8_t* buffer, size_t length) {
    std::vector<uint32_t> values;
    for (size_t i = 0; i < length; i += 4) {
        uint32_t value = (buffer[i] & 0xFF) |
                 ((buffer[i + 1] & 0xFF) << 8) |
                 ((buffer[i + 2] & 0xFF) << 16) |
                 ((buffer[i + 3] & 0xFF) << 24);
        values.push_back(value);
    }
    return values;
}

void print_buffer(const char* buffer_name, const std::vector<uint32_t> &buffer) {
    std::cout << buffer_name << "=[";
    for (size_t i = 0; i < buffer.size(); i++) {
        std::cout << "0x" << std::hex << std::uppercase << buffer[i];
        if (i < buffer.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]\n" << std::endl;
}

std::string convertToHex(const uint8_t* buffer, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i) {
        if (i != 0) oss << ", ";
        oss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(buffer[i]);
    }
    return oss.str();
}

static int parse_values(const char *tokens, uint32_t *data, uint32_t *data_length, size_t max_data_size) {
    uint32_t n_tokens = 0;
    char *error = NULL;
    char *temp_tokens = strdup(tokens);
    if (temp_tokens == NULL)
        return -1;

    char *tok = strtok(temp_tokens, VALUE_DELIMITER);
    if (tok == NULL)
        return -1; 

    while (tok != NULL && n_tokens < max_data_size) {
        data[n_tokens] = (uint32_t) strtoul(tok, &error, 0);
        if (error == NULL || *error != '\0') {
            printf("Failed to convert '%s'\n", tok);
            free(temp_tokens);
            return -1;
        }

        *data_length = *data_length + (strlen(tok) - strlen("0x")) / 2;
        tok = strtok(NULL, VALUE_DELIMITER);
        n_tokens++;
    }

    free(temp_tokens);
    return 0;
}

static constexpr size_t kVarNodesSize = 1024 * 1024 * sizeof(int16_t);

int main() {
    srand(42);
    size_t max_data_size = calculateArraySize(input0);
    printf("Max data size: %zu\n", max_data_size);
    uint32_t input_buf[max_data_size];
    uint32_t data_length = 0;

    // zero out input_buf
    for (size_t i = 0; i < max_data_size; ++i) {
        input_buf[i] = 0;
    }

    int result = parse_values(input0, input_buf, &data_length, max_data_size);
    if (result == 0) {
        printf("Data length: %u bytes\n\n", data_length);
    } else {
        printf("Error parsing values\n");
    }

    print_buffer("Input data", std::vector<uint32_t>(input_buf, input_buf + max_data_size));

    std::cout << "input_buf_uint32: [";
    for (size_t i = 0; i < max_data_size; ++i) {
        std::printf("%u, ", input_buf[i]);
    }
    std::cout << "]\n" << std::endl;

    // Conversion in little-endian format
    std::vector<int8_t> llr_data;
    for (const uint32_t value : input_buf) {
        llr_data.push_back(static_cast<int8_t>(value & 0xFF));            
        llr_data.push_back(static_cast<int8_t>((value >> 8) & 0xFF));     
        llr_data.push_back(static_cast<int8_t>((value >> 16) & 0xFF));
        llr_data.push_back(static_cast<int8_t>((value >> 24) & 0xFF));
    }

    assert(llr_data.size() == data_length);

    size_t data_num = data_length; // each value is 1 byte so dim==bytes
    int8_t* llr_ptr;
    AllocBuffer1d(&llr_ptr, data_num, Agora_memory::Alignment_t::kAlign64, 1);
    std::memcpy(llr_ptr, llr_data.data(), data_num);

    std::cout << "LLR data: [";
    for (size_t i = 0; i < data_num; ++i) {
        std::printf("%i, ", llr_ptr[i]);
    }
    std::cout << "]\n" << std::endl;

    // convert llr_data back to hex for verification
    std::vector<uint32_t> llr_back = convert_to_uint32(llr_ptr, data_num);
    print_buffer("llr_back", llr_back);


    int16_t* resp_var_nodes_ = static_cast<int16_t*>(Agora_memory::PaddedAlignedAlloc(
      Agora_memory::Alignment_t::kAlign64, kVarNodesSize));

    size_t zc = 7;
    size_t basegraph = 2;
    size_t nRows = 42;
    size_t n_cb = 350;
    size_t numChannellrs = n_cb;
    size_t numFillerBits =  30;
    size_t maxIter = 8;
    size_t enableEarlyTerm = true; // RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE 

    struct bblib_ldpc_decoder_5gnr_request ldpc_decoder_5gnr_request {};
    struct bblib_ldpc_decoder_5gnr_response ldpc_decoder_5gnr_response {};

    int8_t* decode_buf;
    size_t decode_num = data_num;
    AllocBuffer1d(&decode_buf, decode_num, Agora_memory::Alignment_t::kAlign64,1);

    ldpc_decoder_5gnr_request.numChannelLlrs = numChannellrs;
    ldpc_decoder_5gnr_request.numFillerBits = numFillerBits;
    ldpc_decoder_5gnr_request.maxIterations = maxIter;
    ldpc_decoder_5gnr_request.enableEarlyTermination = enableEarlyTerm;
    ldpc_decoder_5gnr_request.Zc = zc;
    ldpc_decoder_5gnr_request.baseGraph = basegraph;
    ldpc_decoder_5gnr_request.nRows = nRows;
    ldpc_decoder_5gnr_request.varNodes = llr_ptr;

    size_t num_message_bits = 22 * zc - numFillerBits;
    ldpc_decoder_5gnr_response.numMsgBits = num_message_bits;
    ldpc_decoder_5gnr_response.varNodes = resp_var_nodes_;
    ldpc_decoder_5gnr_response.compactedMessageBytes = (uint8_t*) decode_buf;

    bblib_ldpc_decoder_5gnr(&ldpc_decoder_5gnr_request, &ldpc_decoder_5gnr_response);

    std::unique_ptr<AgoraScrambler::Scrambler> scrambler = std::make_unique<AgoraScrambler::Scrambler>();
    scrambler->Descramble((uint8_t*) decode_buf, decode_num);

    // print decoded data
    std::cout << "Decoded data: [";
    for (size_t i = 0; i < decode_num; ++i) {
        std::printf("%i, ", decode_buf[i]);
    }
    std::cout << "]\n" << std::endl;

    std::vector<uint32_t> decoded_data = convert_to_uint32(decode_buf, decode_num*1);
    print_buffer("decoded_data_hex", decoded_data);

    // Clean up
    free(llr_ptr);
    free(resp_var_nodes_);
    free(decode_buf);

}