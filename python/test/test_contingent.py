import hashlib
import unittest

from ethsnarks.field import FQ
from ethsnarks.mimc import mimc_encrypt
from ethsnarks.utils import native_lib_path
from ethsnarks.merkletree2 import merkle_root
from contingent import Contingent


NATIVE_LIB_PATH = native_lib_path('../.build/libcontingent')
VK_PATH = '../.keys/contingent.vk.json'
PK_PATH = '../.keys/contingent.pk.raw'
PROOF_PATH = '../.keys/contingent.proof.json'

class TestContingent(unittest.TestCase):
	def test_make_proof(self):
		num_blocks = 32

		wrapper = Contingent(NATIVE_LIB_PATH, num_blocks)

		# Generate proving and verification keys
		result = wrapper.genkeys(PK_PATH, VK_PATH)
		self.assertTrue(result == 0)

		print('Keygen done!')

		plaintext = [int(FQ.random()) for n in range(0, num_blocks)]
		plaintext_root = merkle_root(plaintext)
		key = int(FQ.random())
		key_bytes = key.to_bytes(32, 'little')
		sha256 = hashlib.sha256()
		sha256.update(key_bytes)
		key_hash = sha256.digest()
		ciphertext = mimc_encrypt(plaintext, key)

		print('Number of blocks = {}'.format(num_blocks))
		print('Plaintext = {}'.format(plaintext))
		print('Plaintext Root = {}'.format(plaintext_root))
		print('Key = {}'.format(key))
		print('Key Hex = {}'.format(key_bytes.hex()))
		print('Key Hash = {}'.format(key_hash.hex()))
		print('Ciphertext = {}'.format(ciphertext))

		# Generate proof
		proof = wrapper.prove(
			PK_PATH,
			key_hash,
			ciphertext,
			plaintext_root,
			key,
			plaintext)
		
		with open(PROOF_PATH, 'w') as f:
			f.write(proof)

		print('Prove done!')

		# Verify proof
		self.assertTrue(wrapper.verify(
			VK_PATH,
			proof,
			key_hash,
			ciphertext,
			plaintext_root))

		print('Verify done!')


if __name__ == "__main__":
	unittest.main()
