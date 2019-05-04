/*
Copyright 2019 to the Miximus Authors

This file is part of Miximus.

Miximus is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Miximus is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Miximus.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <string>
#include <cstring>
#include <iostream> // cerr
#include <fstream>  // ofstream

#include "contingent.cpp"
#include "stubs.hpp"
#include "utils.hpp" // hex_to_bytes

using std::cerr;
using std::cout;
using std::endl;
using std::ofstream;

using ethsnarks::contingent_gadget;
using ethsnarks::stub_main_genkeys;
using ethsnarks::stub_main_verify;

int char2int(char input)
{
    if (input >= '0' && input <= '9')
        return input - '0';
    if (input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    throw std::invalid_argument("Invalid input string");
}

// This function assumes src to be a zero terminated sanitized string with
// an even number of [0-9a-f] characters, and target to be sufficiently large
void hex2bin(const char *src, char *target)
{
    while (*src && src[1])
    {
        *(target++) = char2int(*src) * 16 + char2int(src[1]);
        src += 2;
    }
}

int main_genkeys(const char *prog_name, int argc, const char **argv)
{
    if (argc < 3)
    {
        cerr << "Usage: " << prog_name << " " << argv[0] << " <pk.raw> <vk.json> <num_blocks>" << endl;
        return 1;
    }

    const char *pk_file = argv[1];
    const char *vk_file = argv[2];
    size_t num_blocks = std::stoi(argv[3]);

    if (0 != contingent_genkeys(num_blocks, pk_file, vk_file))
    {
        cerr << "Error: failed to generate proving and verifying keys" << endl;
        return 1;
    }

    return 0;
}

static int main_prove(const char *prog_name, int argc, const char **argv)
{
    if (argc < 4)
    {
    arg_error:
        cerr << "Usage: " << prog_name << " prove <pk.raw> <proof.json> <num_blocks> <public:key-hash> <public:ciphertext...> <public:plaintext-root> <secret:key> <secret:plaintext...>" << endl;
        cerr << "Args: " << endl;
        cerr << "\t<pk.raw>         Path to proving key" << endl;
        cerr << "\t<proof.json>     Write proof to this file" << endl;
        cerr << "\t<num_blocks>     Number of data blocks" << endl;
        cerr << "\t<key-hash>       SHA256 key hash (hex string)" << endl;
        cerr << "\t<ciphertext...>  Encrypted data blocks" << endl;
        cerr << "\t<plaintext-root> MiMC Merkle-Tree root hash" << endl;
        cerr << "\t<key>            MiMC encryption key" << endl;
        cerr << "\t<plaintext...>   Padded plaintext data blocks" << endl;
        return 1;
    }

    argv++;
    const char *pk_file = *argv++;
    const char *proof_filename = *argv++;
    size_t num_blocks = std::stoi(*argv++);

    if (num_blocks < 1)
    {
        cerr << "Invalid number of blocks: " << num_blocks << endl;
        return 1;
    }

    if (argc != (7 + num_blocks * 2))
        goto arg_error;

    char arg_key_hash[32];
    if (strlen(*argv) != 64)
    {
        cerr << "Invalid key hash length" << endl;
        return 1;
    }
    hex2bin(*argv++, arg_key_hash);

    std::vector<const char *> arg_ciphertext;
    arg_ciphertext.reserve(num_blocks);
    for (size_t i = 0; i < num_blocks; i++)
        arg_ciphertext.emplace_back(*argv++);

    const char *arg_plaintext_root = *argv++;
    const char *arg_key = *argv++;

    std::vector<const char *> arg_plaintext;
    arg_plaintext.reserve(num_blocks);
    for (size_t i = 0; i < num_blocks; i++)
        arg_plaintext.emplace_back(*argv++);

    auto json = contingent_prove(
        pk_file,
        num_blocks,
        arg_key_hash,
        arg_ciphertext.data(),
        arg_plaintext_root,
        arg_key,
        arg_plaintext.data());

    ofstream fh;
    fh.open(proof_filename, std::ios::binary);
    fh << json;
    fh.flush();
    fh.close();

    return 0;
}

static int main_verify(const char *prog_name, int argc, const char **argv)
{
    if (argc < 4)
    {
    arg_error:
        cerr << "Usage: " << prog_name << " verify <vk.json> <proof.json> <num_blocks> <public:key-hash> <public:ciphertext...> <public:plaintext-root>" << endl;
        cerr << "Args: " << endl;
        cerr << "\t<vk.json>        Path to verification key" << endl;
        cerr << "\t<proof.json>     Write proof to this file" << endl;
        cerr << "\t<num_blocks>     Number of data blocks" << endl;
        cerr << "\t<key-hash>       SHA256 key hash (hex string)" << endl;
        cerr << "\t<ciphertext...>  Encrypted data blocks" << endl;
        cerr << "\t<plaintext-root> MiMC Merkle-Tree root hash" << endl;
        return 1;
    }

    argv++;
    const char *vk_file = *argv++;
    const char *proof_file = *argv++;

    // Read proof file
    std::stringstream proof_stream;
    std::ifstream proof_input(proof_file);
    if( ! proof_input ) {
        std::cerr << "Error: cannot open " << proof_file << std::endl;
        return 2;
    }
    proof_stream << proof_input.rdbuf();
    proof_input.close();

    size_t num_blocks = std::stoi(*argv++);

    if (num_blocks < 1)
    {
        cerr << "Invalid number of blocks: " << num_blocks << endl;
        return 1;
    }

    if (argc != (6 + num_blocks))
        goto arg_error;

    char arg_key_hash[32];
    if (strlen(*argv) != 64)
    {
        cerr << "Invalid key hash length" << endl;
        return 1;
    }
    hex2bin(*argv++, arg_key_hash);

    std::vector<const char *> arg_ciphertext;
    arg_ciphertext.reserve(num_blocks);
    for (size_t i = 0; i < num_blocks; i++)
        arg_ciphertext.emplace_back(*argv++);

    const char *arg_plaintext_root = *argv++;

    bool result = contingent_verify(
        vk_file,
        proof_stream.str().c_str(),
        num_blocks,
        arg_key_hash,
        arg_ciphertext.data(),
        arg_plaintext_root);

    if (result)
        cout << "Verification Passed!" << endl;
    else
        cerr << "Verification Failed!" << endl;

    return 0;
}

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <genkeys|prove|verify> [...]" << endl;
        return 1;
    }

    const std::string arg_cmd(argv[1]);

    if (arg_cmd == "prove")
    {
        return main_prove(argv[0], argc - 1, (const char **)&argv[1]);
    }
    else if (arg_cmd == "genkeys")
    {
        return main_genkeys(argv[0], argc - 1, (const char **)&argv[1]);
    }
    else if (arg_cmd == "verify")
    {
        return main_verify(argv[0], argc - 1, (const char **)&argv[1]);
    }

    cerr << "Error: unknown sub-command " << arg_cmd << endl;
    return 2;
}
