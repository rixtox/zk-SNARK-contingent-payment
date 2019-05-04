"""
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
"""

__all__ = ('Contingent',)

import os
import re
import sys
import ctypes
import argparse

from ethsnarks.verifier import Proof, VerifyingKey


class Contingent(object):
    def __init__(self, native_library_path, num_blocks):
        assert isinstance(num_blocks, int)
        assert num_blocks > 0

        self.num_blocks = num_blocks

        lib = ctypes.cdll.LoadLibrary(native_library_path)

        lib_genkeys = lib.contingent_genkeys
        lib_genkeys.argtypes = [ctypes.c_size_t, ctypes.c_char_p, ctypes.c_char_p]
        lib_genkeys.restype = ctypes.c_int
        self._genkeys = lib_genkeys

        lib_prove = lib.contingent_prove
        lib_prove.argtypes = \
            [ctypes.c_char_p] + \
            [ctypes.c_size_t] + \
            [ctypes.c_char_p] + \
            [(ctypes.c_char_p * num_blocks)] + \
            ([ctypes.c_char_p] * 2) + \
            [(ctypes.c_char_p * num_blocks)]
        lib_prove.restype = ctypes.c_char_p
        self._prove = lib_prove

        lib_verify = lib.contingent_verify
        lib_verify.argtypes = \
            [ctypes.c_char_p, ctypes.c_char_p] + \
            [ctypes.c_size_t] + \
            [ctypes.c_char_p] + \
            [(ctypes.c_char_p * num_blocks)] + \
            ([ctypes.c_char_p])
        lib_verify.restype = ctypes.c_bool
        self._verify = lib_verify

    def genkeys(self, pk_file, vk_file):
        assert isinstance(vk_file, str)
        assert isinstance(pk_file, str)

        num_blocks = ctypes.c_size_t(self.num_blocks)
        arg_pk_file = ctypes.c_char_p(pk_file.encode('ascii'))
        arg_vk_file = ctypes.c_char_p(vk_file.encode('ascii'))

        return self._genkeys(num_blocks, arg_pk_file, arg_vk_file)

    def prove(self, pk_file, key_hash, ciphertext, plaintext_root, key, plaintext):
        assert os.path.exists(pk_file)
        assert isinstance(key_hash, bytes)
        assert len(key_hash) == 32
        assert isinstance(ciphertext, (list, tuple))
        assert len(ciphertext) == self.num_blocks
        assert isinstance(plaintext_root, int)
        assert isinstance(key, int)
        assert isinstance(plaintext, (list, tuple))
        assert len(plaintext) == self.num_blocks

        arg_pk_file = ctypes.c_char_p(str(pk_file).encode('ascii'))
        arg_num_blocks = ctypes.c_size_t(self.num_blocks)

        # Public parameters
        arg_key_hash = ctypes.c_char_p(key_hash)
        arg_ciphertext = (ctypes.c_char_p * len(ciphertext))()
        arg_ciphertext[:] = [ctypes.c_char_p(str(_).encode('ascii')) for _ in ciphertext]
        arg_plaintext_root = ctypes.c_char_p(str(plaintext_root).encode('ascii'))
    
        # Private parameters
        arg_key = ctypes.c_char_p(str(key).encode('ascii'))
        arg_plaintext = (ctypes.c_char_p * len(plaintext))()
        arg_plaintext[:] = [ctypes.c_char_p(str(_).encode('ascii')) for _ in plaintext]

        proof = self._prove(arg_pk_file, arg_num_blocks, arg_key_hash, arg_ciphertext, arg_plaintext_root, arg_key, arg_plaintext)
        if proof is None:
            raise RuntimeError("Could not prove!")
        return proof.decode('ascii')

    def verify(self, vk_file, proof_json, key_hash, ciphertext, plaintext_root):
        assert os.path.exists(vk_file)
        assert isinstance(proof_json, str)
        assert isinstance(key_hash, bytes)
        assert len(key_hash) == 32
        assert isinstance(ciphertext, (list, tuple))
        assert len(ciphertext) == self.num_blocks
        assert isinstance(plaintext_root, int)

        arg_vk_file = ctypes.c_char_p(vk_file.encode('ascii'))
        arg_proof_json = ctypes.c_char_p(proof_json.encode('ascii'))
        arg_num_blocks = ctypes.c_size_t(self.num_blocks)
        arg_key_hash = ctypes.c_char_p(key_hash)
        arg_ciphertext = (ctypes.c_char_p * len(ciphertext))()
        arg_ciphertext[:] = [ctypes.c_char_p(str(_).encode('ascii')) for _ in ciphertext]
        arg_plaintext_root = ctypes.c_char_p(str(plaintext_root).encode('ascii'))

        return self._verify(arg_vk_file, arg_proof_json, arg_num_blocks, arg_key_hash, arg_ciphertext, arg_plaintext_root)


class Main(object):

    def __init__(self):
        parser = argparse.ArgumentParser(
            description='Contingent Payment on Blockchain with zk-SNARK',
            usage='''contingent <command> [<args>]

The most commonly used git commands are:
encrypt    Encrypt data file with randomly generated key
genkeys    Generate zk-SNARK proving and verification keys
prove      Generate zk-SNARK proof
verify     Verify a given zk-SNARK proof
''')
        parser.add_argument('command', help='Subcommand to run')
        args = parser.parse_args(sys.argv[1:2])
        if not hasattr(self, args.command):
            print('Unrecognized command')
            parser.print_help()
            exit(1)
        getattr(self, args.command)()

        parser = argparse.ArgumentParser()
        group = parser.add_mutually_exclusive_group()
        group.add_argument('-e', '--encrypt', action='store_true')
        group.add_argument('-d', '--decrypt', action='store_true')
        parser.add_argument('-i', '--input', type=argparse.FileType('r'))
        parser.add_argument('-o', '--output', type=argparse.FileType('w'))
        parser.add_argument('-k', '--key', type=str)
        args = parser.parse_args()

        text_string = ' '.join(args.text)
        key = args.key
        if args.decrypt:
            key = -key
        cyphertext = encrypt(text_string, key)
    
    def encrypt(self):
        parser = argparse.ArgumentParser(
            description='Encrypt data file with optional randomly generated key')
        parser.add_argument('-i', '--in', type=argparse.FileType('r'))
        parser.add_argument('-o', '--out', type=argparse.FileType('w'))
        parser.add_argument('-k', '--key', type=str, nargs='?', default=None, help='key vaule in decimal format')
        parser.add_argument('--keyout', type=argparse.FileType('w'), nargs='?', default=None)
        parser.add_argument('--keyhashout', type=argparse.FileType('w'), nargs='?', default=None)
        args = parser.parse_args(sys.argv[2:])

if __name__ == '__main__':
    Main()
