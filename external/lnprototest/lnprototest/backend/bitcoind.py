#! /usr/bin/python3
# This script exercises the c-lightning implementation

# Released by Rusty Russell under CC0:
# https://creativecommons.org/publicdomain/zero/1.0/

import os
import shutil
import subprocess
import logging

from typing import Any, Callable
from ephemeral_port_reserve import reserve
from brocoin.rpc import RawProxy
from .backend import Backend


class BrocoinProxy:
    """Wrapper for BrocoinProxy to reconnect.

    Long wait times between calls to the Brocoin RPC could result in
    `brocoind` closing the connection, so here we just create
    throwaway connections. This is easier than to reach into the RPC
    library to close, reopen and reauth upon failure.
    """

    def __init__(self, bron_conf_file: str, *args: Any, **kwargs: Any):
        self.bron_conf_file = bron_conf_file

    def __getattr__(self, name: str) -> Callable:
        if name.startswith("__") and name.endswith("__"):
            # Python internal stuff
            raise AttributeError

        def f(*args: Any) -> Callable:
            self.__proxy = RawProxy(bron_conf_file=self.bron_conf_file)

            logging.debug(
                "Calling {name} with arguments {args}".format(name=name, args=args)
            )
            res = self.__proxy._call(name, *args)
            logging.debug(
                "Result for {name} call: {res}".format(
                    name=name,
                    res=res,
                )
            )
            return res

        # Make debuggers show <function brocoin.rpc.name> rather than <function
        # brocoin.rpc.<lambda>>
        f.__name__ = name
        return f


class Brocoind(Backend):
    """Starts regtest brocoind on an ephemeral port, and returns the RPC proxy"""

    def __init__(self, basedir: str):
        self.rpc = None
        self.proc = None
        self.base_dir = basedir
        logging.debug(f"Base dir is {basedir}")
        self.brocoin_dir = os.path.join(basedir, "brocoind")
        self.brocoin_conf = os.path.join(self.brocoin_dir, "brocoin.conf")
        self.cmd_line = [
            "brocoind",
            "-datadir={}".format(self.brocoin_dir),
            "-server",
            "-regtest",
            "-logtimestamps",
            "-nolisten",
        ]
        self.port = reserve()
        self.bron_version = None
        logging.debug("Port is {}, dir is {}".format(self.port, self.brocoin_dir))

    def __init_brocoin_conf(self):
        """Init the brocoin core directory with all the necessary information
        to startup the node"""
        if not os.path.exists(self.brocoin_dir):
            os.makedirs(self.brocoin_dir)
            logging.debug(f"Creating {self.brocoin_dir} directory")
        # For after 0.16.1 (eg. 3f398d7a17f136cd4a67998406ca41a124ae2966), this
        # needs its own [regtest] section.
        logging.debug(f"Writing brocoin conf file at {self.brocoin_conf}")
        with open(self.brocoin_conf, "w") as f:
            f.write("regtest=1\n")
            f.write("rpcuser=rpcuser\n")
            f.write("rpcpassword=rpcpass\n")
            f.write("[regtest]\n")
            f.write("rpcport={}\n".format(self.port))
        self.rpc = BrocoinProxy(bron_conf_file=self.brocoin_conf)

    def __version_compatibility(self) -> None:
        """
        This method tries to manage the compatibility between
        different versions of Brocoin Core implementation.

        This method could sometimes be useful when it is necessary to
        run the test with a different version of Brocoin core.
        """
        if self.rpc is None:
            # Sanity check
            raise ValueError("brocoind not initialized")

        self.bron_version = self.rpc.getnetworkinfo()["version"]
        assert self.bron_version is not None
        logging.debug("Brocoin Core version {}".format(self.bron_version))
        if self.bron_version >= 210000:
            # Maintains the compatibility between wallet
            # different ln implementation can use the main wallet (?)
            self.rpc.createwallet("main")  # Automatically loads

    def __is__brocoind_ready(self) -> bool:
        """Check if brocoind is ready during the execution"""
        if self.proc is None:
            # Sanity check
            raise ValueError("brocoind not initialized")

        # Wait for it to startup.
        while b"Done loading" not in self.proc.stdout.readline():
            pass
        return True

    def start(self) -> None:
        if self.rpc is None:
            self.__init_brocoin_conf()
        # TODO: We can move this to a single call and not use Popen
        self.proc = subprocess.Popen(self.cmd_line, stdout=subprocess.PIPE)
        assert self.proc.stdout

        # Wait for it to startup.
        while not self.__is__brocoind_ready():
            logging.debug("Brocoin core is loading")

        self.__version_compatibility()
        # Block #1.
        # Privkey the coinbase spends to:
        #    cUB4V7VCk6mX32981TWviQVLkj3pa2zBcXrjMZ9QwaZB5Kojhp59
        self.rpc.submitblock(
            "0000002006226e46111a0b59caaf126043eb5bbf28c34f3a5e332a1fc7b2b73cf188910f84591a56720aabc8023cecf71801c5e0f9d049d0c550ab42412ad12a67d89f3a3dbb6c60ffff7f200400000001020000000001010000000000000000000000000000000000000000000000000000000000000000ffffffff03510101ffffffff0200f2052a0100000016001419f5016f07fe815f611df3a2a0802dbd74e634c40000000000000000266a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf90120000000000000000000000000000000000000000000000000000000000000000000000000"
        )
        self.rpc.generatetoaddress(100, self.rpc.getnewaddress())

    def stop(self) -> None:
        self.proc.kill()
        shutil.rmtree(os.path.join(self.brocoin_dir, "regtest"))

    def restart(self) -> None:
        # Only restart if we have to.
        if self.rpc.getblockcount() != 101 or self.rpc.getrawmempool() != []:
            self.stop()
            self.start()
