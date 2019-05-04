#ifndef CONTINGENT_HPP_
#define CONTINGENT_HPP_

#include <stddef.h>
#include <iostream>

using std::cerr;
using std::cout;
using std::endl;

#include "export.hpp"
#include "import.hpp"
#include "stubs.hpp"
#include "utils.hpp"

#include "gadgets/mimc.hpp"
#include "gadgets/sha256_many.hpp"

#include <libsnark/gadgetlib1/gadgets/basic_gadgets.hpp>

using ethsnarks::FieldT;
using ethsnarks::ppT;
using ethsnarks::ProofT;
using ethsnarks::ProvingKeyT;
using ethsnarks::PrimaryInputT;
using ethsnarks::ProtoboardT;
using libsnark::generate_boolean_r1cs_constraint;

namespace ethsnarks
{

// We define our own packing gadget to make integration with Python more easier
// For a bigint value 0x0102030405060708090a0b0c0d0e0f it converts to binary
// form: 0x0f0e0d0c0b0a09080706050403020100000000000000000000000000000000
class packing_gadget : public GadgetT
{
private:
    /* no internal variables */
public:
    const LinearCombinationArrayT bits;
    const LinearCombinationT packed;

    packing_gadget(ProtoboardT &in_pb,
                   const LinearCombinationArrayT &in_bits,
                   const LinearCombinationT &in_packed,
                   const std::string &annotation_prefix)
        : GadgetT(in_pb, annotation_prefix), bits(in_bits), packed(in_packed)
    {
        assert(bits.size() == 256);
    }

    void generate_r1cs_constraints(const bool enforce_bitness)
    /* adds constraint result = \sum  bits[i] * 2^i */
    {
        this->pb.add_r1cs_constraint(ConstraintT(FieldT::one(), packing_sum(), packed), FMT(annotation_prefix, ".packing_constraint"));

        if (enforce_bitness)
        {
            for (size_t i = 0; i < bits.size(); ++i)
            {
                generate_boolean_r1cs_constraint<FieldT>(this->pb, bits[i], FMT(annotation_prefix, ".bitness_%zu", i));
            }
        }
    }

    void generate_r1cs_witness_from_packed()
    {
        packed.evaluate(this->pb);
        const libff::bigint<FieldT::num_limbs> rint = this->pb.lc_val(packed).as_bigint();
        assert(rint.num_bits() <= bits.size()); // `bits` is large enough to represent this packed value
        for (size_t i = 0; i < 32; i++)
        {
            for (size_t j = 0; j < 8; j++)
            {
                this->pb.lc_val(bits[i * 8 + j]) = rint.test_bit(i * 8 + (7 - j)) ? FieldT::one() : FieldT::zero();
            }
        }
    }

    void generate_r1cs_witness_from_bits()
    {
        bits.evaluate(this->pb);
        FieldT result = FieldT::zero();
        for (size_t i = 32; i-- > 0;)
        {
            for (size_t j = 8; j > 0; j--)
            {
                const FieldT v = this->pb.lc_val(bits[i * 8 - j]);
                assert(v == FieldT::zero() || v == FieldT::one());
                result += result + v;
            }
        }
        this->pb.lc_val(packed) = result;
    }

    libsnark::linear_combination<FieldT> packing_sum()
    {
        FieldT twoi = FieldT::one(); // will hold 2^i entering each iteration
        std::vector<LinearTermT> all_terms;
        for (size_t i = 0; i < 32; i++)
        {
            for (size_t j = 8; j-- > 0;)
            {
                auto &lc = bits[i * 8 + j];
                for (auto &term : lc.terms)
                {
                    all_terms.emplace_back(twoi * term);
                }
                twoi += twoi;
            }
        }
        return libsnark::linear_combination<FieldT>(all_terms);
    }
};

/**
* This class implements the following circuit:
*
*            |---------- shared inputs -----------||-- secret inputs --|
* def circuit(key_hash, ciphertext, plaintext_root,   key,  plaintext   ):
*   assert key_hash == sha265(key)
*   assert ciphertext == mimc_enc(key, plaintext)
*   assert plaintext_root == mimc_merkle_root(plaintext)
*
* The input parameters are:
*
*  - `ciphertext` (shared) encrypted data for binding
*  - `plaintext_root` (shared): Merkeltree root hash of plaintext for binding
*  - `key_hash` (shared): encryption key hash for binding
*  - `key` (secret): prove correct encryption and bind the key
*  - `plaintext` (secret): verify correct encryption and bind the data
*
* We verify encryption instead of decryption for fewer constraints.
* 
*/
class contingent_gadget : public GadgetT
{
public:
    typedef packing_gadget PackT;
    typedef sha256_many HashT;
    typedef MiMC_encrypt_gadget EncrT;
    typedef MiMC_merkle_root_gadget RootT;

    const size_t m_num_blocks;

    // public inputs
    const VariableArrayT m_in_key_hash;
    const VariableArrayT m_in_ciphertext;
    const VariableT m_in_plaintext_root;

    // private inputs
    const VariableT m_in_key;
    const VariableArrayT m_in_plaintext;

    // intermediate values
    const VariableArrayT m_key_bits;

    // logic gadgets
    PackT m_key_pack_gadget;
    HashT m_key_hash_gadget;
    EncrT m_encrypt_gadget;
    RootT m_merkle_root_gadget;

    contingent_gadget(
        ProtoboardT &in_pb,
        const size_t num_blocks,
        const std::string &annotation_prefix)
        : GadgetT(in_pb, annotation_prefix),

          m_num_blocks(num_blocks),

          // public inputs
          m_in_key_hash(make_var_array(in_pb, 256, FMT(annotation_prefix, ".in_key_hash"))),
          m_in_ciphertext(make_var_array(in_pb, num_blocks, FMT(annotation_prefix, ".in_ciphertext"))),
          m_in_plaintext_root(make_variable(in_pb, FMT(annotation_prefix, ".in_plaintext_root"))),

          // secret inputs
          m_in_key(make_variable(in_pb, FMT(annotation_prefix, ".in_key"))),
          m_in_plaintext(make_var_array(in_pb, num_blocks, FMT(annotation_prefix, ".in_plaintext"))),

          // unpacked key bit array for SHA256 use, need to fill it in generate_r1cs_witness()
          // since key is an element in the alt_bn128 field, it's around 253~254 bits
          // for compatibility with Ethereum and other programs, we will use 256 bits / 32 bytes
          // to express it in LSB-first format, and the last few bits are always zeros.
          m_key_bits(make_var_array(in_pb, 256, FMT(annotation_prefix, ".key_bits"))),

          // to ensure that the prover correctly unpacked the key into bit array, we need
          // a packing gadget to reconstruct the key from the bit array variables with
          // constraints to verify this process, also we need to ensure all the values
          // in the bit array are either 0 or 1.
          m_key_pack_gadget(in_pb, m_key_bits, m_in_key, FMT(annotation_prefix, ".key_pack_gadget")),

          // key_hash = sha265(key)
          m_key_hash_gadget(in_pb, m_key_bits, FMT(annotation_prefix, ".key_hash_gadget")),

          // ciphertext = mimc_enc(key, plaintext)
          m_encrypt_gadget(in_pb, m_in_key, m_in_plaintext, FMT(annotation_prefix, ".mimc_encrypt_gadget")),

          // plaintext_root = mimc_merkle_root(plaintext)
          m_merkle_root_gadget(in_pb, m_in_plaintext, FMT(annotation_prefix, ".merkle_root_gadget"))

    {
        assert(m_num_blocks > 0);

        // public inputs are m_in_key_hash, in_ciphertext, and in_plaintext_root
        this->pb.set_input_sizes(256 + m_num_blocks + 1);
    }

    void generate_r1cs_constraints()
    {
        m_key_pack_gadget.generate_r1cs_constraints(true);
        m_key_hash_gadget.generate_r1cs_constraints();
        m_encrypt_gadget.generate_r1cs_constraints();
        m_merkle_root_gadget.generate_r1cs_constraints();

        // assert key_hash == sha265(key)
        // we have to compare them bit by bit
        const HashT::DigestT &digest = m_key_hash_gadget.result();
        assert(digest.bits.size() == m_in_key_hash.size());
        for (size_t i = 0; i < digest.bits.size(); i++)
        {
            this->pb.add_r1cs_constraint(
                ConstraintT(digest.bits[i], FieldT::one(), m_in_key_hash[i]),
                "key_hash == SHA256(key)");
        }

        // assert ciphertext == mimc_enc(key, plaintext)
        // we have to compare them block by block
        const VariableArrayT &out_ciphertext = m_encrypt_gadget.result();
        for (size_t i = 0; i < m_in_ciphertext.size(); i++)
        {
            this->pb.add_r1cs_constraint(
                ConstraintT(m_in_ciphertext[i], FieldT::one(), out_ciphertext[i]),
                "ciphertext == mimc_enc(key, plaintext)");
        }

        // assert plaintext_root == mimc_merkle_root(plaintext)
        this->pb.add_r1cs_constraint(
            ConstraintT(m_in_plaintext_root, FieldT::one(), m_merkle_root_gadget.result()),
            "plaintext_root == mimc_merkle_root(plaintext)");
    }

    void generate_r1cs_witness(
        const libff::bit_vector &in_key_hash, // 256 bits array
        const std::vector<FieldT> &in_ciphertext,
        const FieldT &in_plaintext_root,
        const FieldT &in_key,
        const std::vector<FieldT> &in_plaintext)
    {
        assert(in_ciphertext.size() == m_num_blocks);
        assert(in_plaintext.size() == m_num_blocks);

        m_in_key_hash.fill_with_bits(this->pb, in_key_hash);
        for (size_t i = 0; i < m_num_blocks; i++)
            this->pb.val(m_in_ciphertext[i]) = in_ciphertext[i];
        this->pb.val(m_in_plaintext_root) = in_plaintext_root;

        this->pb.val(m_in_key) = in_key;
        for (size_t i = 0; i < m_num_blocks; i++)
            this->pb.val(m_in_plaintext[i]) = in_plaintext[i];

        m_key_pack_gadget.generate_r1cs_witness_from_packed();
        m_key_hash_gadget.generate_r1cs_witness();
        m_encrypt_gadget.generate_r1cs_witness();
        m_merkle_root_gadget.generate_r1cs_witness();
    }
};

} // namespace ethsnarks

#ifdef __cplusplus
extern "C"
{
#endif

    int contingent_genkeys(
        const size_t num_blocks,
        const char *pk_file,
        const char *vk_file);

    char *contingent_prove(
        const char *pk_file,
        const size_t num_blocks,
        const char *in_key_hash,
        const char **in_ciphertext,
        const char *in_plaintext_root,
        const char *in_key,
        const char **in_plaintext);

    bool contingent_verify(
        const char *vk_file,
        const char *proof_json,
        const size_t num_blocks,
        const char *in_key_hash,
        const char **in_ciphertext,
        const char *in_plaintext_root);

#ifdef __cplusplus
} // extern "C" {
#endif

#endif
