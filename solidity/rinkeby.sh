geth --rinkeby --rpc --rpcapi "personal,eth,net,network,web3" --ipcpath "~/Library/Ethereum/geth.ipc"
// you cannot mine on rinkeby
// another terminal
geth attach

// create account
geth --rinkeby account new

// 
geth attach

eth.getBalance(eth.accounts[0])
personal.unlockAccount(eth.accounts[0], null, 1200) // unlock for 20 minutes
personal.unlockAccount(eth.accounts[1], null)
personal.lockAccount(eth.accounts[0])
eth.syncing()
//false
exit
clear

truffle migrate --reset --compile-all --network rinkeby
// might need to unlock accounts

truffle console --network rinkeby


geth attach
var admin = eth.accounts[0]
var remainingSupply = 10000
var bagelAddress = '' //from configuration file
// instance - use web3 library
var token_abi = []
var token_adress = ''
var tokenInstance = new web3.eth.Contract(token_abi, token_adress)
// import account on metamask

// ls -l ~/Library/Ethereum/rinkeby/keystore