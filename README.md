# zk-SNARK Contingent Payment on Blockchain

This project implements a zk-SNARK circuit that allows Hash Time Locked Contract (HTLC) to be used for contingent payment for trading digital goods with a known Merkle Tree root hash value.

## Building

We recommend using Docker to build this project. You can build our provided Docker image and start a new container by running

```bash
./start-docker
```

Inside the Docker container, execute the following to compile the project.

```bash
make git-submodules
make
```

## Testing

To run the test cases, execute the following inside the Docker container:

```bash
make test
```

# Authors

* Naiwei Zheng (zheng248@purdue.edu)
* Tiantian Gong (gong146@purdue.edu)
