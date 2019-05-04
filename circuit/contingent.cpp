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
#include <sstream>
#include "import.hpp"
#include "contingent.hpp"
#include <boost/property_tree/json_parser.hpp>

using std::stringstream;
using ethsnarks::PropertyTreeT;
using boost::property_tree::read_json;

std::string proof_to_json(ProofT &proof) {
    std::stringstream ss;

    ss << "{\n";
    ss << " \"A\" :[" << ethsnarks::outputPointG1AffineAsHex(proof.g_A) << "],\n";
    ss << " \"B\"  :[" << ethsnarks::outputPointG2AffineAsHex(proof.g_B)<< "],\n";
    ss << " \"C\"  :[" << ethsnarks::outputPointG1AffineAsHex(proof.g_C)<< "]\n";
    ss << "}";

    ss.rdbuf()->pubseekpos(0, std::ios_base::out);

    return(ss.str());
}

char *contingent_prove(
    const char *pk_file,
    const size_t num_blocks,
    const char *in_key_hash,       // SHA256(key) 32 bytes char array of binary data
    const char **in_ciphertext,    // null-terminated array of null-terminated ascii decimal values
    const char *in_plaintext_root, // null-terminated ascii decimal value
    const char *in_key,            // null-terminated ascii decimal values
    const char **in_plaintext      // null-terminated array of null-terminated ascii decimal value
)
{
    ppT::init_public_params();

    // convert the 32 bytes key hash into 256 bits array
    libff::bit_vector arg_key_hash = ethsnarks::bytes_to_bv((const uint8_t *)in_key_hash, 32);
    std::vector<FieldT> arg_ciphertext;
    arg_ciphertext.reserve(num_blocks);
    for (size_t i = 0; i < num_blocks; i++)
        arg_ciphertext.emplace_back(in_ciphertext[i]);
    FieldT arg_plaintext_root(in_plaintext_root);

    FieldT arg_key(in_key);
    std::vector<FieldT> arg_plaintext;
    arg_plaintext.reserve(num_blocks);
    for (size_t i = 0; i < num_blocks; i++)
        arg_plaintext.emplace_back(in_plaintext[i]);

    // Create protoboard with gadget
    ProtoboardT pb;
    ethsnarks::contingent_gadget gadget(pb, num_blocks, "contingent_gadget");
    gadget.generate_r1cs_constraints();
    gadget.generate_r1cs_witness(
        arg_key_hash, arg_ciphertext, arg_plaintext_root, arg_key, arg_plaintext);

    if (!pb.is_satisfied())
    {
        std::cerr << "Not Satisfied!" << std::endl;
        return nullptr;
    }

    std::cerr << pb.num_constraints() << " constraints" << std::endl;

    auto proving_key = ethsnarks::stub_load_pk_from_file(pk_file);
    auto proof = libsnark::r1cs_gg_ppzksnark_zok_prover<ethsnarks::ppT>(proving_key, pb.primary_input(), pb.auxiliary_input());
    const auto json = proof_to_json(proof);

    // Return proof as a JSON document, which must be destroyed by the caller
    return ::strdup(json.c_str());
}

int contingent_genkeys(const size_t num_blocks, const char *pk_file, const char *vk_file)
{
    ppT::init_public_params();

    ProtoboardT pb;
    ethsnarks::contingent_gadget gadget(pb, num_blocks, "contingent_gadget");
    gadget.generate_r1cs_constraints();

    return ethsnarks::stub_genkeys_from_pb(pb, pk_file, vk_file);
}

/**
* Parse the witness/proof from a property tree
*   {"A": g1,
*    "B": g2,
*    "C": g1}
*/
ProofT proof_from_tree(PropertyTreeT &in_tree)
{
    auto A = ethsnarks::create_G1_from_ptree(in_tree, "A");
    auto B = ethsnarks::create_G2_from_ptree(in_tree, "B");
    auto C = ethsnarks::create_G1_from_ptree(in_tree, "C");

    return ProofT(
        std::move(A),
        std::move(B),
        std::move(C));
}

/**
* Parse the proof from a stream of JSON encoded data
*/
ProofT proof_from_json(stringstream &in_json)
{
    PropertyTreeT root;
    read_json(in_json, root);
    return proof_from_tree(root);
}

bool contingent_verify(
    const char *vk_file,
    const char *proof_json,
    const size_t num_blocks,
    const char *in_key_hash,
    const char **in_ciphertext,
    const char *in_plaintext_root)
{
    ppT::init_public_params();

    std::stringstream vk_stream;
    std::ifstream vk_input(vk_file);
    if( ! vk_input ) {
        std::cerr << "Error: cannot open " << vk_file << std::endl;
        return false;
    }
    vk_stream << vk_input.rdbuf();
    vk_input.close();
    auto vk = ethsnarks::vk_from_json(vk_stream);

    std::stringstream proof_stream;
    proof_stream << proof_json;
    auto proof = proof_from_json(proof_stream);

    PrimaryInputT primary_input;
    primary_input.reserve(256 + num_blocks + 1);

    // convert the 32 bytes key hash into 256 bits array
    libff::bit_vector key_hash_bits = ethsnarks::bytes_to_bv((const uint8_t *)in_key_hash, 32);
    for (size_t i = 0; i < key_hash_bits.size(); ++i)
    {
        primary_input.emplace_back(key_hash_bits[i] ? FieldT::one() : FieldT::zero());
    }
    for (size_t i = 0; i < num_blocks; i++)
        primary_input.emplace_back(in_ciphertext[i]);
    primary_input.emplace_back(in_plaintext_root);

    return libsnark::r1cs_gg_ppzksnark_zok_verifier_strong_IC<ppT>(vk, primary_input, proof);
}
