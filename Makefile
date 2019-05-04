CLI = .build/contingent_cli
CMAKE ?= cmake
GIT ?= git

all: $(CLI) test

$(CLI): .build
	$(MAKE) -C $(dir $@)

.build:
	mkdir -p $@
	cd $@ && $(CMAKE) ../circuit/ || rm -rf ../$@

debug:
	mkdir -p .build && cd .build && $(CMAKE) -DCMAKE_BUILD_TYPE=Debug ../circuit/

release:
	mkdir -p .build && cd .build && $(CMAKE) -DCMAKE_BUILD_TYPE=Release ../circuit/

performance:
	mkdir -p .build && cd .build && $(CMAKE) -DCMAKE_BUILD_TYPE=Release -DPERFORMANCE=1 ../circuit/

git-submodules:
	$(GIT) submodule update --init --recursive

git-pull:
	$(GIT) pull --recurse-submodules
	$(GIT) submodule update --recursive --remote

clean:
	rm -rf .build solidity/node_modules

python-test:
	mkdir -p .keys
	$(MAKE) -C python test

test: python-test
