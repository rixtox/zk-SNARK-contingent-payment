var Migrations = artifacts.require("./Migrations.sol");
var HashedTimelock = artifacts.require("./HashedTimelock.sol");
var HashedTimelockERC20 = artifacts.require("./HashedTimelockERC20.sol");

module.exports = function(deployer) {
  deployer.deploy(Migrations);
  deployer.deploy(HashedTimelock);
  deployer.deploy(HashedTimelockERC20);
};
