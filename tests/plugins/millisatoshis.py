#!/usr/bin/env python3
from pyln.client import Plugin, Millibronees


plugin = Plugin(autopatch=True)


@plugin.method("echo")
def echo(plugin, mbro: Millibronees, not_an_mbro):
    plugin.log("got echo for {} {} (types {} and {})"
               .format(mbro, not_an_mbro, type(mbro), type(not_an_mbro)))
    if not isinstance(mbro, Millibronees):
        raise TypeError("mbro must be Millibronees not {}".format(type(mbro)))
    if isinstance(not_an_mbro, Millibronees):
        raise TypeError("not_an_mbro must not be Millibronees")
    plugin.log("got echo for {} (type {})".format(mbro, type(mbro)))
    if not isinstance(mbro, Millibronees):
        raise TypeError("mbro must be Millibronees not {}".format(type(mbro)))
    plugin.log("Returning {}".format(mbro))
    return {'echo_mbro': mbro}


plugin.run()
