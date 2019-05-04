const Web3 = require('web3')
// import {Web3} from 'web3';
// var web3 = new Web3('rinkeby.infura.io/v3/d480e44fa4824ab881c3c940ad00dc17')
// var web3 = new Web3(new Web3.providers.HttpProvider("http://localhost:8545"));
// console.log(web3.version.api)

App = {
  web3Provider: null,
  contracts: {},
  account: '0x0',
  loading: false,
  tokenPrice: 10000000000000000,
  tokensClaimed: 0,
  totalSupply: 10000,
  remainingSupply: 10000,
  lastTransfers: [],
  lastTransactions: [],

  init: function() {
    console.log("App initialized...")
    return App.initWeb3();
  },
  
  initWeb3: function() {
    if (typeof web3 !== 'undefined') {
      // If a web3 instance is already provided by Meta Mask. Should be.
      console.log(web3.version.api);
      App.web3Provider = web3.currentProvider;
      web3 = new Web3(web3.currentProvider);
    } else {
      // Specify default instance if no web3 instance provided
      web3.setProvider(new Web3.providers.WebsocketProvider('ws://localhost:8545'));
      App.web3Provider = new Web3.providers.WebsocketProvider('ws://localhost:8545');
      web3 = new Web3(App.web3Provider);
    }
    return App.initContracts();
  },

  initContracts: function() {
    $.getJSON("ERC223Interface.json", function(erc) {
      App.contracts.ERC223Interface = TruffleContract(erc);
      App.contracts.ERC223Interface.setProvider(App.web3Provider);
    }).done(function() {
      $.getJSON("ContractReceiver.json", function(receiver) {
        App.contracts.ContractReceiver = TruffleContract(receiver);
        App.contracts.ContractReceiver.setProvider(App.web3Provider);
      }).done(function() {
        $.getJSON("Bagel.json", function(bagel) {
        App.contracts.Bagel = TruffleContract(bagel);
        App.contracts.Bagel.setProvider(App.web3Provider);
        App.contracts.Bagel.deployed().then(function(bagel) {
          console.log("Bagel Address:", bagel.address);
        });

        // App.listenForEvents();
        return App.render();
      });
    })
  })
},

  // Listen for events emitted from the contract
  listenForEvents: function() {
    App.contracts.Bagel.deployed().then(function(instance) {
      instance.Transfer({}, {
        fromBlock: 4070279,
        toBlock: 'latest',
      }).watch(function(error, event) {
        console.log("event triggered", event);
        // App.render();
      })
    })
  },

  render: function() {
    if (App.loading) {
      return;
    }
    App.loading = true;

    var loader  = $('#loader');
    var content = $('#content');

    loader.show();
    content.hide();

    // Load account data
    web3.eth.getCoinbase(function(err, account) {
      if(err === null) {
        App.account = account;
        $('#accountAddress').html("Your Account: " + account);
      }
    })
    
    // get last 5 function calls with arguments
    App.contracts.Bagel.deployed().then(function(instance) {
      return instance.address;
    }).then(function(contractAddress) {
    // var blockNum = web3.eth.getBlockNumber();4070279 4215490
    var blockNum = 4070430;
    var transactionSet = [];
    var block;
    for (var blockId = blockNum; blockId >= 4070279; blockId--) {      // var block = web3.eth.getBlock(blockId);
      web3.eth.getBlock(blockId,function(err, bk) {
        if (err === null) {
          block = bk;
          // console.log(block);
          var txSet = block.transactions;
          for (var txId = txSet.length - 1; txId >= 0; txId--) {
              web3.eth.getTransaction(txSet[txId], function(err, tx) {
                // contractAddress 0x44d936C8Ce21eb6f55e11c4cd782894c0D20ECF2
                if (tx.to == '0x44d936c8ce21eb6f55e11c4cd782894c0d20ecf2') {
                  transactionSet.push(web3.toAscii(tx.input));
                  console.log(transactionSet.length);
                  if (transactionSet.length == 5) {
                    App.lastTransactions = transactionSet;
                    console.log(App.lastTransactions);
                    $('.last-5-functions').html(App.lastTransactions);
                  }
                }

              });
              if (App.lastTransactions.length == 5) {break;}
            };
          }
      });
      if (App.lastTransactions.length == 5) {break;}
    };
    
  });

    // Load token contract app.remainingSupply.call().then(result => result.toNumber())
    App.contracts.Bagel.deployed().then(function(instance) {
      bagelInstance = instance;
      var tokenPrice = 10000000000000000;
      var totalSupply = 10000;
      var remainingSupply = 10000;
      App.tokenPrice = tokenPrice;
      $('.token-price').html(0.01);//web3.utils.fromWei(App.tokenPrice, "ether").toNumber()
      bagelInstance.totalSupply.call().then(function(result) { totalSupply = result.toNumber();});
      bagelInstance.remainingSupply.call().then(function(result) { remainingSupply = result.toNumber();});
      App.remainingSupply = remainingSupply;
      App.totalSupply = totalSupply;
      return (totalSupply - remainingSupply);
    }).then(function(tokensClaimed) {
      App.tokensClaimed = tokensClaimed;
      $('.tokens-claimed').html(App.tokensClaimed);
      $('.tokens-available').html(App.remainingSupply);
      var progressPercent = (Math.ceil(App.tokensClaimed) / App.remainingSupply) * 100;
      $('#progress').css('width', progressPercent + '%');
      var balance = 0;
      bagelInstance.balanceOf.call('riesling').then(function(result) {balance = result.toNumber();}); //App.account
      return balance;
    }).then(function(balance) {
        $('.bagel-balance').html(balance);//.toNumber()
        App.loading = false;
        loader.hide();
        content.show();
      });
    // get top 10 holders

    // get last 5 transfers
    App.contracts.Bagel.deployed().then(function(instance) {
      var lastTransfers = [];
      // instance.getPastEvents('Transfer', {
      //   fromBlock: 4070279,
      //   toBlock: 'latest',
      // }, (error,events) => {console.log(events); }).then((events) => {
      //   lastTransfers = events;
      // });
    //   var contractAddress = instance.address;
    //   return contractAddress;
    // }).then(function(address) {
    //   var bagelAbi = [{"constant":true,"inputs":[],"name":"totalSupply","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function","signature":"0x18160ddd"},{"constant":true,"inputs":[],"name":"owner","outputs":[{"name":"","type":"address"}],"payable":false,"stateMutability":"view","type":"function","signature":"0x8da5cb5b"},{"constant":true,"inputs":[],"name":"remainingSupply","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function","signature":"0xda0239a6"},{"inputs":[],"payable":false,"stateMutability":"nonpayable","type":"constructor","signature":"constructor"},{"anonymous":false,"inputs":[{"indexed":true,"name":"from","type":"string"},{"indexed":true,"name":"to","type":"string"},{"indexed":false,"name":"value","type":"uint256"},{"indexed":false,"name":"data","type":"bytes"}],"name":"Transfer","type":"event","signature":"0xe8cca054678291c72a36fdd68416aea936e6b2720caa266fa5070fbae858d191"},{"constant":false,"inputs":[{"name":"myName","type":"string"}],"name":"registerMe","outputs":[{"name":"","type":"bool"}],"payable":true,"stateMutability":"payable","type":"function","signature":"0x218add39"},{"constant":true,"inputs":[{"name":"name","type":"string"},{"name":"addr","type":"address"}],"name":"isRegistered","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"view","type":"function","signature":"0xbd290b81"},{"constant":true,"inputs":[{"name":"who","type":"string"}],"name":"balanceOf","outputs":[{"name":"","type":"uint256"}],"payable":false,"stateMutability":"view","type":"function","signature":"0x35ee5f87"},{"constant":false,"inputs":[{"name":"from","type":"string"},{"name":"to","type":"string"},{"name":"value","type":"uint256"}],"name":"transfer","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function","signature":"0x9b80b050"},{"constant":false,"inputs":[{"name":"from","type":"string"},{"name":"to","type":"string"},{"name":"value","type":"uint256"},{"name":"data","type":"bytes"}],"name":"transfer","outputs":[{"name":"","type":"bool"}],"payable":false,"stateMutability":"nonpayable","type":"function","signature":"0xacbae75e"}];
    //   var contract = web3.eth.contract(bagelAbi);
    //   var contractInstance = contract.at(address);
    //   var events = contractInstance.allEvents({fromBlock: 4070279, toBlock: 'latest'});
    var events = instance.allEvents({fromBlock: 4070279, toBlock: 'latest'});
      events.watch(function(error, result){
          // console.log(result);
      });
      // would get all past logs again.
      events.get(function(error, logs){
        for (var i = logs.length - 1; i >= 0; i--) {
          lastTransfers.push(logs[i].transactionHash);
        }
        console.log(lastTransfers);
        if (lastTransfers.length > 5) {
          lastTransfers = lastTransfers.slice(lastTransfers.length - 5);
        }
        App.lastTransfers = lastTransfers;
        $('.last-5-transfers').html(App.lastTransfers);
      });
    //   var lastTransfers = [];
    //   contract.getPastEvents('allEvents', {
    //     fromBlock: 4070279,
    //     toBlock: 'latest',
    //     }, (error,events) => {console.log(events); }).then((events) => {
    //       lastTransfers = events;
    //     });
    //   if (lastTransfers.length > 5) {
    //     lastTransfers = lastTransfers.slice(lastTransfers.length - 5);
    //   }
    //   App.lastTransfers = lastTransfers;
    //   $('.last-5-transfers').html(App.lastTransfers);
    });

  }
 }

$(function() {
  $(window).load(function() {
    App.init();
  })
});
