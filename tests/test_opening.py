from fixtures import *  # noqa: F401,F403
from fixtures import TEST_NETWORK
from pyln.client import RpcError, Millibronees
from utils import (
    only_one, wait_for, sync_blockheight, first_channel_id, calc_lease_fee
)

import pytest
import re
import unittest


def find_next_feerate(node, peer):
    chan = only_one(only_one(node.rpc.listpeers(peer.info['id'])['peers'])['channels'])
    return chan['next_feerate']


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
@pytest.mark.developer("requres 'dev-queryrates'")
def test_queryrates(node_factory, brocoind):
    l1, l2 = node_factory.get_nodes(2)

    amount = 10 ** 6

    l1.fundwallet(amount * 10)
    l2.fundwallet(amount * 10)

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    with pytest.raises(RpcError, match=r'not advertising liquidity'):
        l1.rpc.dev_queryrates(l2.info['id'], amount, amount * 10)

    l2.rpc.call('funderupdate', {'policy': 'match',
                                 'policy_mod': 100,
                                 'per_channel_max_mbro': '1bron',
                                 'fuzz_percent': 0,
                                 'lease_fee_base_mbro': '2bro',
                                 'funding_weight': 1000,
                                 'lease_fee_basis': 140,
                                 'channel_fee_max_base_mbro': '3bro',
                                 'channel_fee_max_proportional_thousandths': 101})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    result = l1.rpc.dev_queryrates(l2.info['id'], amount, amount)
    assert result['our_funding_mbro'] == Millibronees(amount * 1000)
    assert result['their_funding_mbro'] == Millibronees(amount * 1000)
    assert result['funding_weight'] == 1000
    assert result['lease_fee_base_mbro'] == Millibronees(2000)
    assert result['lease_fee_basis'] == 140
    assert result['channel_fee_max_base_mbro'] == Millibronees(3000)
    assert result['channel_fee_max_proportional_thousandths'] == 101


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.developer("uses dev-disconnect")
@pytest.mark.openchannel('v1')  # Mixed v1 + v2, v2 manually turned on
def test_multifunding_v2_best_effort(node_factory, brocoind):
    '''
    Check that best_effort flag works.
    '''
    disconnects = ["-WIRE_INIT",
                   "-WIRE_ACCEPT_CHANNEL",
                   "-WIRE_FUNDING_SIGNED"]
    l1 = node_factory.get_node(options={'experimental-dual-fund': None},
                               allow_warning=True,
                               may_reconnect=True)
    l2 = node_factory.get_node(options={'experimental-dual-fund': None},
                               allow_warning=True,
                               may_reconnect=True)
    l3 = node_factory.get_node(disconnect=disconnects)
    l4 = node_factory.get_node()

    l1.fundwallet(2000000)

    destinations = [{"id": '{}@localhost:{}'.format(l2.info['id'], l2.port),
                     "amount": 50000},
                    {"id": '{}@localhost:{}'.format(l3.info['id'], l3.port),
                     "amount": 50000},
                    {"id": '{}@localhost:{}'.format(l4.info['id'], l4.port),
                     "amount": 50000}]

    for i, d in enumerate(disconnects):
        failed_sign = d == "-WIRE_FUNDING_SIGNED"
        # Should succeed due to best-effort flag.
        min_channels = 1 if failed_sign else 2
        l1.rpc.multifundchannel(destinations, minchannels=min_channels)

        brocoind.generate_block(6, wait_for_mempool=1)

        # l3 should fail to have channels; l2 also fails on last attempt
        node_list = [l1, l4] if failed_sign else [l1, l2, l4]
        for node in node_list:
            node.daemon.wait_for_log(r'to CHANNELD_NORMAL')

        # There should be working channels to l2 and l4 for every run
        # but the last
        working_chans = [l4] if failed_sign else [l2, l4]
        for ldest in working_chans:
            inv = ldest.rpc.invoice(5000, 'i{}'.format(i), 'i{}'.format(i))['bolt11']
            l1.rpc.pay(inv)

        # Function to find the SCID of the channel that is
        # currently open.
        # Cannot use LightningNode.get_channel_scid since
        # it assumes the *first* channel found is the one
        # wanted, but in our case we close channels and
        # open again, so multiple channels may remain
        # listed.
        def get_funded_channel_scid(n1, n2):
            peers = n1.rpc.listpeers(n2.info['id'])['peers']
            assert len(peers) == 1
            peer = peers[0]
            channels = peer['channels']
            assert channels
            for c in channels:
                state = c['state']
                if state in ('DUALOPEND_AWAITING_LOCKIN', 'CHANNELD_AWAITING_LOCKIN', 'CHANNELD_NORMAL'):
                    return c['short_channel_id']
            assert False

        # Now close channels to l2 and l4, for the next run.
        if not failed_sign:
            l1.rpc.close(get_funded_channel_scid(l1, l2))
        l1.rpc.close(get_funded_channel_scid(l1, l4))

        for node in node_list:
            node.daemon.wait_for_log(r'to CLOSINGD_COMPLETE')

    # With 2 down, it will fail to fund channel
    l2.stop()
    l3.stop()
    with pytest.raises(RpcError, match=r'(Connection refused|Bad file descriptor)'):
        l1.rpc.multifundchannel(destinations, minchannels=2)

    # This works though.
    l1.rpc.multifundchannel(destinations, minchannels=1)


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.developer("uses dev-disconnect")
@pytest.mark.openchannel('v2')
def test_v2_open_sigs_restart(node_factory, brocoind):
    disconnects_1 = ['-WIRE_TX_SIGNATURES']
    disconnects_2 = ['+WIRE_TX_SIGNATURES']

    l1, l2 = node_factory.get_nodes(2,
                                    opts=[{'disconnect': disconnects_1,
                                           'may_reconnect': True},
                                          {'disconnect': disconnects_2,
                                           'may_reconnect': True}])

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    # Fund the channel, should appear to finish ok even though the
    # peer fails
    with pytest.raises(RpcError):
        l1.rpc.fundchannel(l2.info['id'], chan_amount)

    chan_id = first_channel_id(l1, l2)
    log = l1.daemon.is_in_log('{} psbt'.format(chan_id))
    assert log
    psbt = re.search("psbt (.*)", log).group(1)

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    l1.daemon.wait_for_log('Peer has reconnected, state DUALOPEND_OPEN_INIT')
    with pytest.raises(RpcError):
        l1.rpc.openchannel_signed(chan_id, psbt)

    l2.daemon.wait_for_log('Broadcasting funding tx')
    txid = l2.rpc.listpeers(l1.info['id'])['peers'][0]['channels'][0]['funding_txid']
    brocoind.generate_block(6, wait_for_mempool=txid)

    # Make sure we're ok.
    l1.daemon.wait_for_log(r'to CHANNELD_NORMAL')
    l2.daemon.wait_for_log(r'to CHANNELD_NORMAL')


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.developer("uses dev-disconnect")
@pytest.mark.openchannel('v2')
def test_v2_open_sigs_restart_while_dead(node_factory, brocoind):
    # Same thing as above, except the transaction mines
    # while we're asleep
    disconnects_1 = ['-WIRE_TX_SIGNATURES']
    disconnects_2 = ['+WIRE_TX_SIGNATURES']

    l1, l2 = node_factory.get_nodes(2,
                                    opts=[{'disconnect': disconnects_1,
                                           'may_reconnect': True,
                                           'may_fail': True},
                                          {'disconnect': disconnects_2,
                                           'may_reconnect': True,
                                           'may_fail': True}])

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    # Fund the channel, should appear to finish ok even though the
    # peer fails
    with pytest.raises(RpcError):
        l1.rpc.fundchannel(l2.info['id'], chan_amount)

    chan_id = first_channel_id(l1, l2)
    log = l1.daemon.is_in_log('{} psbt'.format(chan_id))
    assert log
    psbt = re.search("psbt (.*)", log).group(1)

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    l1.daemon.wait_for_log('Peer has reconnected, state DUALOPEND_OPEN_INIT')
    with pytest.raises(RpcError):
        l1.rpc.openchannel_signed(chan_id, psbt)

    l2.daemon.wait_for_log('Broadcasting funding tx')
    l2.daemon.wait_for_log('sendrawtx exit 0')

    l1.stop()
    l2.stop()
    brocoind.generate_block(6)
    l1.restart()
    l2.restart()

    # Make sure we're ok.
    l2.daemon.wait_for_log(r'to CHANNELD_NORMAL')
    l1.daemon.wait_for_log(r'to CHANNELD_NORMAL')


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
def test_v2_rbf_single(node_factory, brocoind, chainparams):
    l1, l2 = node_factory.get_nodes(2, opts={'wumbo': None})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount)
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    next_feerate = find_next_feerate(l1, l2)

    # Check that feerate info is correct
    info_1 = only_one(only_one(l1.rpc.listpeers(l2.info['id'])['peers'])['channels'])
    assert info_1['initial_feerate'] == info_1['last_feerate']
    rate = int(info_1['last_feerate'][:-5])
    assert int(info_1['next_feerate'][:-5]) == rate * 65 // 64

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    # Do the bump
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])

    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
    assert update['commitments_secured']

    # Check that feerate info has incremented
    info_2 = only_one(only_one(l1.rpc.listpeers(l2.info['id'])['peers'])['channels'])
    assert info_1['initial_feerate'] == info_2['initial_feerate']
    assert info_1['next_feerate'] == info_2['last_feerate']

    rate = int(info_2['last_feerate'][:-5])
    assert int(info_2['next_feerate'][:-5]) == rate * 65 // 64

    # Sign our inputs, and continue
    signed_psbt = l1.rpc.signpsbt(update['psbt'])['signed_psbt']

    # Fails because we didn't put enough feerate in.
    with pytest.raises(RpcError, match=r'insufficient fee'):
        l1.rpc.openchannel_signed(chan_id, signed_psbt)

    # Do it again, with a higher feerate
    info_2 = only_one(only_one(l1.rpc.listpeers(l2.info['id'])['peers'])['channels'])
    assert info_1['initial_feerate'] == info_2['initial_feerate']
    assert info_1['next_feerate'] == info_2['last_feerate']
    rate = int(info_2['last_feerate'][:-5])
    assert int(info_2['next_feerate'][:-5]) == rate * 65 // 64

    # We 4x the feerate to beat the min-relay fee
    next_rate = '{}perkw'.format(rate * 65 // 64 * 4)
    # Gotta unreserve the psbt and re-reserve with higher feerate
    l1.rpc.unreserveinputs(initpsbt['psbt'])
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_rate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)
    # Do the bump+sign
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'],
                                   funding_feerate=next_rate)
    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
    assert update['commitments_secured']
    signed_psbt = l1.rpc.signpsbt(update['psbt'])['signed_psbt']
    l1.rpc.openchannel_signed(chan_id, signed_psbt)

    brocoind.generate_block(1)
    sync_blockheight(brocoind, [l1])
    l1.daemon.wait_for_log(' to CHANNELD_NORMAL')

    # Check that feerate info is gone
    info_1 = only_one(only_one(l1.rpc.listpeers(l2.info['id'])['peers'])['channels'])
    assert 'initial_feerate' not in info_1
    assert 'last_feerate' not in info_1
    assert 'next_feerate' not in info_1

    # Shut l2 down, force close the channel.
    l2.stop()
    resp = l1.rpc.close(l2.info['id'], unilateraltimeout=1)
    assert resp['type'] == 'unilateral'
    l1.daemon.wait_for_log(' to CHANNELD_SHUTTING_DOWN')
    l1.daemon.wait_for_log('sendrawtx exit 0')


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
def test_v2_rbf_liquidity_ad(node_factory, brocoind, chainparams):

    opts = {'funder-policy': 'match', 'funder-policy-mod': 100,
            'lease-fee-base-bro': '100bro', 'lease-fee-basis': 100,
            'may_reconnect': True}
    l1, l2 = node_factory.get_nodes(2, opts=opts)

    # what happens when we RBF?
    feerate = 2000
    amount = 500000
    l1.fundwallet(20000000)
    l2.fundwallet(20000000)

    # l1 leases a channel from l2
    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    rates = l1.rpc.dev_queryrates(l2.info['id'], amount, amount)
    l1.daemon.wait_for_log('disconnect')
    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    chan_id = l1.rpc.fundchannel(l2.info['id'], amount, request_amt=amount,
                                 feerate='{}perkw'.format(feerate),
                                 compact_lease=rates['compact_lease'])['channel_id']

    vins = [x for x in l1.rpc.listfunds()['outputs'] if x['reserved']]
    assert only_one(vins)
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['output'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    est_fees = calc_lease_fee(amount, feerate, rates)

    # This should be the accepter's amount
    fundings = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['funding']
    assert Millibronees(est_fees + amount * 1000) == Millibronees(fundings['remote_mbro'])

    # rbf the lease with a higher amount
    rate = int(find_next_feerate(l1, l2)[:-5])
    # We 4x the feerate to beat the min-relay fee
    next_feerate = '{}perkw'.format(rate * 4)

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)['psbt']

    # do the bump
    bump = l1.rpc.openchannel_bump(chan_id, amount, initpsbt,
                                   funding_feerate=next_feerate)
    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
    assert update['commitments_secured']
    # Sign our inputs, and continue
    signed_psbt = l1.rpc.signpsbt(update['psbt'])['signed_psbt']
    l1.rpc.openchannel_signed(chan_id, signed_psbt)

    # what happens when the channel opens?
    brocoind.generate_block(6)
    l1.daemon.wait_for_log('to CHANNELD_NORMAL')

    # This should be the accepter's amount
    fundings = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['funding']
    # FIXME: The lease goes away :(
    assert Millibronees(0) == Millibronees(fundings['remote_mbro'])

    wait_for(lambda: [c['active'] for c in l1.rpc.listchannels(l1.get_channel_scid(l2))['channels']] == [True, True])

    # send some payments, mine a block or two
    inv = l2.rpc.invoice(10**4, '1', 'no_1')
    l1.rpc.pay(inv['bolt11'])

    # l2 attempts to close a channel that it leased, should succeed
    # (channel isnt leased)
    l2.rpc.close(l1.get_channel_scid(l2))
    l1.daemon.wait_for_log('State changed from CLOSINGD_SIGEXCHANGE to CLOSINGD_COMPLETE')


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
def test_v2_rbf_multi(node_factory, brocoind, chainparams):
    l1, l2 = node_factory.get_nodes(2,
                                    opts={'may_reconnect': True,
                                          'allow_warning': True})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount)
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    # Attempt to do abort, should fail since we've
    # already gotten an inflight
    with pytest.raises(RpcError):
        l1.rpc.openchannel_abort(chan_id)

    rate = int(find_next_feerate(l1, l2)[:-5])
    # We 4x the feerate to beat the min-relay fee
    next_feerate = '{}perkw'.format(rate * 4)

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    # Do the bump
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount,
                                   initpsbt['psbt'],
                                   funding_feerate=next_feerate)

    # Abort this open attempt! We will re-try
    aborted = l1.rpc.openchannel_abort(chan_id)
    assert not aborted['channel_canceled']

    # Do the bump, again, same feerate
    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount,
                                   initpsbt['psbt'],
                                   funding_feerate=next_feerate)

    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
    assert update['commitments_secured']

    # Sign our inputs, and continue
    signed_psbt = l1.rpc.signpsbt(update['psbt'])['signed_psbt']
    l1.rpc.openchannel_signed(chan_id, signed_psbt)

    # We 2x the feerate to beat the min-relay fee
    rate = int(find_next_feerate(l1, l2)[:-5])
    next_feerate = '{}perkw'.format(rate * 2)

    # Initiate another RBF, double the channel amount this time
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount * 2, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    # Do the bump
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount * 2,
                                   initpsbt['psbt'],
                                   funding_feerate=next_feerate)

    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
    assert update['commitments_secured']

    # Sign our inputs, and continue
    signed_psbt = l1.rpc.signpsbt(update['psbt'])['signed_psbt']
    l1.rpc.openchannel_signed(chan_id, signed_psbt)

    brocoind.generate_block(1)
    sync_blockheight(brocoind, [l1])
    l1.daemon.wait_for_log(' to CHANNELD_NORMAL')


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.developer("uses dev-disconnect")
@pytest.mark.openchannel('v2')
def test_rbf_reconnect_init(node_factory, brocoind, chainparams):
    disconnects = ['-WIRE_INIT_RBF',
                   '+WIRE_INIT_RBF']

    l1, l2 = node_factory.get_nodes(2,
                                    opts=[{'disconnect': disconnects,
                                           'may_reconnect': True},
                                          {'may_reconnect': True}])

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount)
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    next_feerate = find_next_feerate(l1, l2)

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    # Do the bump!?
    for d in disconnects:
        l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
        with pytest.raises(RpcError):
            l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])
        assert l1.rpc.getpeer(l2.info['id']) is not None

    # This should succeed
    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.developer("uses dev-disconnect")
@pytest.mark.openchannel('v2')
def test_rbf_reconnect_ack(node_factory, brocoind, chainparams):
    disconnects = ['-WIRE_ACK_RBF',
                   '+WIRE_ACK_RBF']

    l1, l2 = node_factory.get_nodes(2,
                                    opts=[{'may_reconnect': True},
                                          {'disconnect': disconnects,
                                           'may_reconnect': True}])

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount)
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    next_feerate = find_next_feerate(l1, l2)

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    # Do the bump!?
    for d in disconnects:
        l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
        with pytest.raises(RpcError):
            l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])
        assert l1.rpc.getpeer(l2.info['id']) is not None

    # This should succeed
    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.developer("uses dev-disconnect")
@pytest.mark.openchannel('v2')
def test_rbf_reconnect_tx_construct(node_factory, brocoind, chainparams):
    disconnects = ['=WIRE_TX_ADD_INPUT',  # Initial funding succeeds
                   '-WIRE_TX_ADD_INPUT',
                   '+WIRE_TX_ADD_INPUT',
                   '-WIRE_TX_ADD_OUTPUT',
                   '+WIRE_TX_ADD_OUTPUT',
                   '-WIRE_TX_COMPLETE',
                   '+WIRE_TX_COMPLETE']

    l1, l2 = node_factory.get_nodes(2,
                                    opts=[{'disconnect': disconnects,
                                           'may_reconnect': True},
                                          {'may_reconnect': True}])

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount)
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    next_feerate = find_next_feerate(l1, l2)

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    # Run through TX_ADD wires
    for d in disconnects[1:-2]:
        l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
        with pytest.raises(RpcError):
            l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])
        assert l1.rpc.getpeer(l2.info['id']) is not None

    # Now we finish off the completes failure check
    for d in disconnects[-2:]:
        l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
        bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])
        with pytest.raises(RpcError):
            update = l1.rpc.openchannel_update(chan_id, bump['psbt'])

    # Now we succeed
    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])
    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
    assert update['commitments_secured']


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.developer("uses dev-disconnect")
@pytest.mark.openchannel('v2')
def test_rbf_reconnect_tx_sigs(node_factory, brocoind, chainparams):
    disconnects = ['=WIRE_TX_SIGNATURES',  # Initial funding succeeds
                   '-WIRE_TX_SIGNATURES',  # When we send tx-sigs, RBF
                   '=WIRE_TX_SIGNATURES',  # When we reconnect
                   '+WIRE_TX_SIGNATURES']  # When we RBF again

    l1, l2 = node_factory.get_nodes(2,
                                    opts=[{'disconnect': disconnects,
                                           'may_reconnect': True},
                                          {'may_reconnect': True}])

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount)
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log('Broadcasting funding tx')
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    rate = int(find_next_feerate(l1, l2)[:-5])
    # We 4x the feerate to beat the min-relay fee
    next_feerate = '{}perkw'.format(rate * 4)

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'],
                                   funding_feerate=next_feerate)
    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])

    # Sign our inputs, and continue
    signed_psbt = l1.rpc.signpsbt(update['psbt'])['signed_psbt']

    # First time we error when we send our sigs
    with pytest.raises(RpcError, match='Owning subdaemon dualopend died'):
        l1.rpc.openchannel_signed(chan_id, signed_psbt)

    # We reconnect and try again. feerate should have bumped
    rate = int(find_next_feerate(l1, l2)[:-5])
    # We 4x the feerate to beat the min-relay fee
    next_feerate = '{}perkw'.format(rate * 4)

    # Initiate an RBF
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                               prev_utxos, reservedok=True,
                               min_witness_weight=110,
                               excess_as_change=True)

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)

    # l2 gets our sigs and broadcasts them
    l2.daemon.wait_for_log('peer_in WIRE_CHANNEL_REESTABLISH')
    l2.daemon.wait_for_log('peer_in WIRE_TX_SIGNATURES')
    l2.daemon.wait_for_log('sendrawtx exit 0')

    # Wait until we've done re-establish, if we try to
    # RBF again too quickly, it'll fail since they haven't
    # had time to process our sigs yet
    l1.daemon.wait_for_log('peer_in WIRE_CHANNEL_REESTABLISH')
    l1.daemon.wait_for_log('peer_in WIRE_TX_SIGNATURES')

    # 2nd RBF
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'],
                                   funding_feerate=next_feerate)
    update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
    signed_psbt = l1.rpc.signpsbt(update['psbt'])['signed_psbt']

    # Second time we error after we send our sigs
    with pytest.raises(RpcError, match='Owning subdaemon dualopend died'):
        l1.rpc.openchannel_signed(chan_id, signed_psbt)

    # l2 gets our sigs
    l2.daemon.wait_for_log('peer_in WIRE_TX_SIGNATURES')
    l2.daemon.wait_for_log('sendrawtx exit 0')

    # mine a block?
    brocoind.generate_block(1)
    sync_blockheight(brocoind, [l1])
    l1.daemon.wait_for_log(' to CHANNELD_NORMAL')

    # Check that they have matching funding txid
    l1_funding_txid = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['funding_txid']
    l2_funding_txid = only_one(only_one(l2.rpc.listpeers()['peers'])['channels'])['funding_txid']
    assert l1_funding_txid == l2_funding_txid


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
def test_rbf_no_overlap(node_factory, brocoind, chainparams):
    l1, l2 = node_factory.get_nodes(2,
                                    opts={'allow_warning': True})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount)
    chan_id = res['channel_id']

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')

    next_feerate = find_next_feerate(l1, l2)

    # Initiate an RBF (this grabs the non-reserved utxo, which isnt the
    # one we started with)
    startweight = 42 + 172  # base weight, funding output
    initpsbt = l1.rpc.fundpsbt(chan_amount, next_feerate, startweight,
                               min_witness_weight=110,
                               excess_as_change=True)

    # Do the bump
    bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])

    with pytest.raises(RpcError, match='No overlapping input present.'):
        l1.rpc.openchannel_update(chan_id, bump['psbt'])


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
@pytest.mark.developer("uses dev-sign-last-tx")
def test_rbf_fails_to_broadcast(node_factory, brocoind, chainparams):
    l1, l2 = node_factory.get_nodes(2,
                                    opts={'allow_warning': True,
                                          'may_reconnect': True})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    # Really low feerate means that the bump wont work the first time
    res = l1.rpc.fundchannel(l2.info['id'], chan_amount, feerate='253perkw')
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']
    assert inflights[-1]['funding_txid'] in brocoind.rpc.getrawmempool()

    def run_retry():
        startweight = 42 + 173
        rate = int(find_next_feerate(l1, l2)[:-5])
        # We 2x the feerate to beat the min-relay fee
        next_feerate = '{}perkw'.format(rate * 2)
        initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                                   prev_utxos, reservedok=True,
                                   min_witness_weight=110,
                                   excess_as_change=True)

        l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
        bump = l1.rpc.openchannel_bump(chan_id, chan_amount,
                                       initpsbt['psbt'],
                                       funding_feerate=next_feerate)
        # We should be able to call this with while an open is progress
        # but not yet committed
        l1.rpc.dev_sign_last_tx(l2.info['id'])
        update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
        assert update['commitments_secured']

        return l1.rpc.signpsbt(update['psbt'])['signed_psbt']

    signed_psbt = run_retry()
    l1.rpc.openchannel_signed(chan_id, signed_psbt)
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']
    assert inflights[-1]['funding_txid'] in brocoind.rpc.getrawmempool()

    # Restart and listpeers, used to crash
    l1.restart()
    l1.rpc.listpeers()

    # We've restarted. Let's RBF
    signed_psbt = run_retry()
    l1.rpc.openchannel_signed(chan_id, signed_psbt)
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']
    assert len(inflights) == 3
    assert inflights[-1]['funding_txid'] in brocoind.rpc.getrawmempool()

    l1.restart()

    # Are inflights the same post restart
    prev_inflights = inflights
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']
    assert prev_inflights == inflights
    assert inflights[-1]['funding_txid'] in brocoind.rpc.getrawmempool()

    # Produce a signature for every inflight
    last_txs = l1.rpc.dev_sign_last_tx(l2.info['id'])
    assert len(last_txs['inflights']) == len(inflights)
    for last_tx, inflight in zip(last_txs['inflights'], inflights):
        assert last_tx['funding_txid'] == inflight['funding_txid']
    assert last_txs['tx']


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
def test_rbf_broadcast_close_inflights(node_factory, brocoind, chainparams):
    """
    Close a channel before it's mined, and the most recent transaction
    hasn't made it to the mempool. Should publish all the commitment
    transactions that we have.
    """
    l1, l2 = node_factory.get_nodes(2,
                                    opts={'allow_warning': True})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount, feerate='7500perkw')
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']
    assert inflights[-1]['funding_txid'] in brocoind.rpc.getrawmempool()

    # Make it such that l1 and l2 cannot broadcast transactions
    # (mimics failing to reach the miner with replacement)
    def censoring_sendrawtx(r):
        return {'id': r['id'], 'result': {}}

    l1.daemon.rpcproxy.mock_rpc('sendrawtransaction', censoring_sendrawtx)
    l2.daemon.rpcproxy.mock_rpc('sendrawtransaction', censoring_sendrawtx)

    def run_retry():
        startweight = 42 + 173
        next_feerate = find_next_feerate(l1, l2)
        initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                                   prev_utxos, reservedok=True,
                                   min_witness_weight=110,
                                   excess_as_change=True)

        l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
        bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])
        update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
        assert update['commitments_secured']

        return l1.rpc.signpsbt(update['psbt'])['signed_psbt']

    signed_psbt = run_retry()
    l1.rpc.openchannel_signed(chan_id, signed_psbt)
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']
    assert inflights[-1]['funding_txid'] not in brocoind.rpc.getrawmempool()

    cmtmt_txid = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['scratch_txid']
    assert cmtmt_txid == inflights[-1]['scratch_txid']

    # l2 goes offline
    l2.stop()

    # l1 drops to chain.
    l1.daemon.rpcproxy.mock_rpc('sendrawtransaction', None)
    l1.rpc.close(chan_id, 1)
    l1.daemon.wait_for_logs(['Broadcasting txid {}'.format(inflights[0]['scratch_txid']),
                             'Broadcasting txid {}'.format(inflights[1]['scratch_txid']),
                             'sendrawtx exit 0',
                             'sendrawtx exit 25'])
    assert inflights[0]['scratch_txid'] in brocoind.rpc.getrawmempool()
    assert inflights[1]['scratch_txid'] not in brocoind.rpc.getrawmempool()


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
def test_rbf_non_last_mined(node_factory, brocoind, chainparams):
    """
    What happens if a 'non-tip' RBF transaction is mined?
    """
    l1, l2 = node_factory.get_nodes(2,
                                    opts={'allow_warning': True,
                                          'may_reconnect': True})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    amount = 2**24
    chan_amount = 100000
    brocoind.rpc.sendtoaddress(l1.rpc.newaddr()['bech32'], amount / 10**8 + 0.01)
    brocoind.generate_block(1)
    # Wait for it to arrive.
    wait_for(lambda: len(l1.rpc.listfunds()['outputs']) > 0)

    res = l1.rpc.fundchannel(l2.info['id'], chan_amount, feerate='7500perkw')
    chan_id = res['channel_id']
    vins = brocoind.rpc.decoderawtransaction(res['tx'])['vin']
    assert(only_one(vins))
    prev_utxos = ["{}:{}".format(vins[0]['txid'], vins[0]['vout'])]

    # Check that we're waiting for lockin
    l1.daemon.wait_for_log(' to DUALOPEND_AWAITING_LOCKIN')
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']
    assert inflights[-1]['funding_txid'] in brocoind.rpc.getrawmempool()

    def run_retry():
        startweight = 42 + 173
        rate = int(find_next_feerate(l1, l2)[:-5])
        # We 2x the feerate to beat the min-relay fee
        next_feerate = '{}perkw'.format(rate * 2)
        initpsbt = l1.rpc.utxopsbt(chan_amount, next_feerate, startweight,
                                   prev_utxos, reservedok=True,
                                   min_witness_weight=110,
                                   excess_as_change=True)

        l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
        bump = l1.rpc.openchannel_bump(chan_id, chan_amount, initpsbt['psbt'])
        update = l1.rpc.openchannel_update(chan_id, bump['psbt'])
        assert update['commitments_secured']

        return l1.rpc.signpsbt(update['psbt'])['signed_psbt']

    # Make a second inflight
    signed_psbt = run_retry()
    l1.rpc.openchannel_signed(chan_id, signed_psbt)

    # Make it such that l1 and l2 cannot broadcast transactions
    # (mimics failing to reach the miner with replacement)
    def censoring_sendrawtx(r):
        return {'id': r['id'], 'result': {}}

    l1.daemon.rpcproxy.mock_rpc('sendrawtransaction', censoring_sendrawtx)
    l2.daemon.rpcproxy.mock_rpc('sendrawtransaction', censoring_sendrawtx)

    # Make a 3rd inflight that won't make it into the mempool
    signed_psbt = run_retry()
    l1.rpc.openchannel_signed(chan_id, signed_psbt)

    l1.daemon.rpcproxy.mock_rpc('sendrawtransaction', None)
    l2.daemon.rpcproxy.mock_rpc('sendrawtransaction', None)

    # We fetch out our inflights list
    inflights = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])['inflight']

    # l2 goes offline
    l2.stop()

    # The funding transaction gets mined (should be the 2nd inflight)
    brocoind.generate_block(6, wait_for_mempool=1)

    # l2 comes back up
    l2.start()

    # everybody's got the right things now
    l1.daemon.wait_for_log(r'to CHANNELD_NORMAL')
    l2.daemon.wait_for_log(r'to CHANNELD_NORMAL')

    channel = only_one(only_one(l1.rpc.listpeers()['peers'])['channels'])
    assert channel['funding_txid'] == inflights[1]['funding_txid']
    assert channel['scratch_txid'] == inflights[1]['scratch_txid']

    # We delete inflights when the channel is in normal ops
    assert 'inflights' not in channel

    # l2 stops, again
    l2.stop()

    # l1 drops to chain.
    l1.rpc.close(chan_id, 1)
    l1.daemon.wait_for_log('Broadcasting txid {}'.format(channel['scratch_txid']))

    # The funding transaction gets mined (should be the 2nd inflight)
    brocoind.generate_block(1, wait_for_mempool=1)
    l1.daemon.wait_for_log(r'to ONCHAIN')


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
@pytest.mark.openchannel('v2')
def test_funder_options(node_factory, brocoind):
    l1, l2, l3 = node_factory.get_nodes(3)
    l1.fundwallet(10**7)

    # Check the default options
    funder_opts = l1.rpc.call('funderupdate')

    assert funder_opts['policy'] == 'fixed'
    assert funder_opts['policy_mod'] == 0
    assert funder_opts['min_their_funding_mbro'] == Millibronees('10000000mbro')
    assert funder_opts['max_their_funding_mbro'] == Millibronees('4294967295000mbro')
    assert funder_opts['per_channel_min_mbro'] == Millibronees('10000000mbro')
    assert funder_opts['per_channel_max_mbro'] == Millibronees('4294967295000mbro')
    assert funder_opts['reserve_tank_mbro'] == Millibronees('0mbro')
    assert funder_opts['fuzz_percent'] == 0
    assert funder_opts['fund_probability'] == 100
    assert funder_opts['leases_only']

    # l2 funds a chanenl with us. We don't contribute
    l2.rpc.connect(l1.info['id'], 'localhost', l1.port)
    l2.fundchannel(l1, 10**6)
    chan_info = only_one(only_one(l2.rpc.listpeers(l1.info['id'])['peers'])['channels'])
    # l1 contributed nothing
    assert chan_info['funding']['remote_mbro'] == Millibronees('0mbro')
    assert chan_info['funding']['local_mbro'] != Millibronees('0mbro')

    # Change all the options
    funder_opts = l1.rpc.call('funderupdate',
                              {'policy': 'available',
                               'policy_mod': 100,
                               'min_their_funding_mbro': '100000mbro',
                               'max_their_funding_mbro': '2000000000mbro',
                               'per_channel_min_mbro': '8000000mbro',
                               'per_channel_max_mbro': '10000000000mbro',
                               'reserve_tank_mbro': '3000000mbro',
                               'fund_probability': 99,
                               'fuzz_percent': 0,
                               'leases_only': False})

    assert funder_opts['policy'] == 'available'
    assert funder_opts['policy_mod'] == 100
    assert funder_opts['min_their_funding_mbro'] == Millibronees('100000mbro')
    assert funder_opts['max_their_funding_mbro'] == Millibronees('2000000000mbro')
    assert funder_opts['per_channel_min_mbro'] == Millibronees('8000000mbro')
    assert funder_opts['per_channel_max_mbro'] == Millibronees('10000000000mbro')
    assert funder_opts['reserve_tank_mbro'] == Millibronees('3000000mbro')
    assert funder_opts['fuzz_percent'] == 0
    assert funder_opts['fund_probability'] == 99

    # Set the fund probability back up to 100.
    funder_opts = l1.rpc.call('funderupdate',
                              {'fund_probability': 100})
    l3.rpc.connect(l1.info['id'], 'localhost', l1.port)
    l3.fundchannel(l1, 10**6)
    chan_info = only_one(only_one(l3.rpc.listpeers(l1.info['id'])['peers'])['channels'])
    # l1 contributed all its funds!
    assert chan_info['funding']['remote_mbro'] == Millibronees('9994255000mbro')
    assert chan_info['funding']['local_mbro'] == Millibronees('1000000000mbro')


@unittest.skipIf(TEST_NETWORK != 'regtest', 'elementsd doesnt yet support PSBT features we need')
def test_funder_contribution_limits(node_factory, brocoind):
    opts = {'experimental-dual-fund': None,
            'feerates': (5000, 5000, 5000, 5000)}
    l1, l2, l3 = node_factory.get_nodes(3, opts=opts)

    # We do a lot of these, so do them all then mine all at once.
    addr, txid = l1.fundwallet(10**8, mine_block=False)
    l1msgs = ['Owning output .* txid {} CONFIRMED'.format(txid)]

    # Give l2 lots of utxos
    l2msgs = []
    for amt in (10**3,  # this one is too small to add
                10**5, 10**4, 10**4, 10**4, 10**4, 10**4):
        addr, txid = l2.fundwallet(amt, mine_block=False)
        l2msgs.append('Owning output .* txid {} CONFIRMED'.format(txid))

    # Give l3 lots of utxos
    l3msgs = []
    for amt in (10**3,  # this one is too small to add
                10**4, 10**4, 10**4, 10**4, 10**4, 10**4, 10**4, 10**4, 10**4, 10**4, 10**4):
        addr, txid = l3.fundwallet(amt, mine_block=False)
        l3msgs.append('Owning output .* txid {} CONFIRMED'.format(txid))

    brocoind.generate_block(1)
    l1.daemon.wait_for_logs(l1msgs)
    l2.daemon.wait_for_logs(l2msgs)
    l3.daemon.wait_for_logs(l3msgs)

    # Contribute 100% of available funds to l2, all 6 utxos (smallest utxo
    # 10**3 is left out)
    l2.rpc.call('funderupdate',
                {'policy': 'available',
                 'policy_mod': 100,
                 'min_their_funding_mbro': '1000mbro',
                 'per_channel_min_mbro': '1000000mbro',
                 'fund_probability': 100,
                 'fuzz_percent': 0,
                 'leases_only': False})

    # Set our contribution to 50k bro, should only use 7 of 12 available utxos
    l3.rpc.call('funderupdate',
                {'policy': 'fixed',
                 'policy_mod': '50000bro',
                 'min_their_funding_mbro': '1000mbro',
                 'per_channel_min_mbro': '1000bro',
                 'per_channel_max_mbro': '500000bro',
                 'fund_probability': 100,
                 'fuzz_percent': 0,
                 'leases_only': False})

    l1.rpc.connect(l2.info['id'], 'localhost', l2.port)
    l1.fundchannel(l2, 10**7)
    assert l2.daemon.is_in_log('Policy .* returned funding amount of 139020bro')
    assert l2.daemon.is_in_log(r'calling `signpsbt` .* 6 inputs')

    l1.rpc.connect(l3.info['id'], 'localhost', l3.port)
    l1.fundchannel(l3, 10**7)
    assert l3.daemon.is_in_log('Policy .* returned funding amount of 50000bro')
    assert l3.daemon.is_in_log(r'calling `signpsbt` .* 7 inputs')
