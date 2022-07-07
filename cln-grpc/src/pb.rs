tonic::include_proto!("cln");
use std::str::FromStr;

use cln_rpc::primitives::{
    Amount as JAmount, AmountOrAll as JAmountOrAll, AmountOrAny as JAmountOrAny,
    Feerate as JFeerate, Outpoint as JOutpoint, OutputDesc as JOutputDesc,
};

impl From<JAmount> for Amount {
    fn from(a: JAmount) -> Self {
        Amount { mbro: a.mbro() }
    }
}

impl From<&Amount> for JAmount {
    fn from(a: &Amount) -> Self {
        JAmount::from_mbro(a.mbro)
    }
}

impl From<JOutpoint> for Outpoint {
    fn from(a: JOutpoint) -> Self {
        Outpoint {
            txid: a.txid,
            outnum: a.outnum,
        }
    }
}

impl From<&Outpoint> for JOutpoint {
    fn from(a: &Outpoint) -> Self {
        JOutpoint {
            txid: a.txid.clone(),
            outnum: a.outnum,
        }
    }
}

impl From<&Feerate> for cln_rpc::primitives::Feerate {
    fn from(f: &Feerate) -> cln_rpc::primitives::Feerate {
        use feerate::Style;
        match f.style.clone().unwrap() {
            Style::Slow(_) => JFeerate::Slow,
            Style::Normal(_) => JFeerate::Normal,
            Style::Urgent(_) => JFeerate::Urgent,
            Style::Perkw(i) => JFeerate::PerKw(i),
            Style::Perkb(i) => JFeerate::PerKb(i),
        }
    }
}

impl From<&OutputDesc> for JOutputDesc {
    fn from(od: &OutputDesc) -> JOutputDesc {
        JOutputDesc {
            address: od.address.clone(),
            amount: od.amount.as_ref().unwrap().into(),
        }
    }
}

impl From<JAmountOrAll> for AmountOrAll {
    fn from(a: JAmountOrAll) -> Self {
        match a {
            JAmountOrAll::Amount(a) => AmountOrAll {
                value: Some(amount_or_all::Value::Amount(a.into())),
            },
            JAmountOrAll::All => AmountOrAll {
                value: Some(amount_or_all::Value::All(true)),
            },
        }
    }
}

impl From<&AmountOrAll> for JAmountOrAll {
    fn from(a: &AmountOrAll) -> Self {
        match &a.value {
            Some(amount_or_all::Value::Amount(a)) => JAmountOrAll::Amount(a.into()),
            Some(amount_or_all::Value::All(_)) => JAmountOrAll::All,
            None => panic!("AmountOrAll is neither amount nor all: {:?}", a),
        }
    }
}

impl From<JAmountOrAny> for AmountOrAny {
    fn from(a: JAmountOrAny) -> Self {
        match a {
            JAmountOrAny::Amount(a) => AmountOrAny {
                value: Some(amount_or_any::Value::Amount(a.into())),
            },
            JAmountOrAny::Any => AmountOrAny {
                value: Some(amount_or_any::Value::Any(true)),
            },
        }
    }
}
impl From<&AmountOrAny> for JAmountOrAny {
    fn from(a: &AmountOrAny) -> Self {
        match &a.value {
            Some(amount_or_any::Value::Amount(a)) => JAmountOrAny::Amount(a.into()),
            Some(amount_or_any::Value::Any(_)) => JAmountOrAny::Any,
            None => panic!("AmountOrAll is neither amount nor any: {:?}", a),
        }
    }
}
impl From<RouteHop> for cln_rpc::primitives::Routehop {
    fn from(c: RouteHop) -> Self {
        Self {
            id: cln_rpc::primitives::Pubkey::from_slice(&c.id).unwrap(),
            scid: cln_rpc::primitives::ShortChannelId::from_str(&c.short_channel_id).unwrap(),
            feebase: c.feebase.as_ref().unwrap().into(),
            feeprop: c.feeprop,
            expirydelta: c.expirydelta as u16,
        }
    }
}
impl From<Routehint> for cln_rpc::primitives::Routehint {
    fn from(c: Routehint) -> Self {
        Self {
            hops: c.hops.into_iter().map(|h| h.into()).collect(),
        }
    }
}
impl From<RoutehintList> for cln_rpc::primitives::RoutehintList {
    fn from(c: RoutehintList) -> Self {
        Self {
            hints: c.hints.into_iter().map(|h| h.into()).collect(),
        }
    }
}
#[cfg(test)]
mod test {
    use super::*;
    use serde_json::json;

    #[test]
    fn test_listpeers() {
        let j: serde_json::Value = json!({
          "peers": [
            {
              "id": "0266e4598d1d3c415f572a8488830b60f7e744ed9235eb0b1ba93283b315c03518",
              "connected": true,
              "netaddr": [
                "127.0.0.1:39152"
              ],
              "features": "8808226aa2",
              "channels": [
                {
                  "state": "CHANNELD_NORMAL",
                  "scratch_txid": "fd4659658d235c20c81f96f7bc867c17abbfd20fcdd46c27eaad74ea52eaee90",
                  "last_tx_fee_mbro": "14257000mbro",
                  "feerate": {
                    "perkw": 11000,
                    "perkb": 44000
                  },
                  "owner": "channeld",
                  "short_channel_id": "103x2x1",
                  "direction": 0,
                  "channel_id": "44b77a6d66ca54f0c365c84b13a95fbde462415a0549228baa25ee1bb1dfef66",
                  "funding_txid": "67efdfb11bee25aa8b2249055a4162e4bd5fa9134bc865c3f054ca666d7ab744",
                  "funding_outnum": 1,
                  "close_to_addr": "bcrt1q9tc6q49l6wrrtp8ul45rj92hsleehwwxty32zu",
                  "close_to": "00142af1a054bfd3863584fcfd6839155787f39bb9c6",
                  "private": false,
                  "opener": "remote",
                  "features": [
                    "option_static_remotekey",
                    "option_anchor_outputs"
                  ],
                  "funding": {
                    "local_mbro": "0mbro",
                    "remote_mbro": "1000000000mbro",
                    "pushed_mbro": "0mbro"
                  },
                  "mbronees_to_us": 0,
                  "to_us_mbro": "0mbro",
                  "mbronees_to_us_min": 0,
                  "min_to_us_mbro": "0mbro",
                  "mbronees_to_us_max": 0,
                  "max_to_us_mbro": "0mbro",
                  "mbronees_total": 1000000000,
                  "total_mbro": "1000000000mbro",
                  "fee_base_mbro": "1mbro",
                  "fee_proportional_millionths": 10,
                  "dust_limit_broneess": 546,
                  "dust_limit_mbro": "546000mbro",
                  "max_total_htlc_in_mbro": "18446744073709551615mbro",
                  "their_channel_reserve_broneess": 10000,
                  "their_reserve_mbro": "10000000mbro",
                  "our_channel_reserve_broneess": 10000,
                  "our_reserve_mbro": "10000000mbro",
                  "spendable_mbronees": 0,
                  "spendable_mbro": "0mbro",
                  "receivable_mbronees": 853257998,
                  "receivable_mbro": "853257998mbro",
                  "htlc_minimum_mbro": 0,
                  "minimum_htlc_in_mbro": "0mbro",
                  "their_to_self_delay": 5,
                  "our_to_self_delay": 5,
                  "max_accepted_htlcs": 483,
                  "state_changes": [
                    {
                      "timestamp": "2022-03-25T13:57:33.322Z",
                      "old_state": "CHANNELD_AWAITING_LOCKIN",
                      "new_state": "CHANNELD_NORMAL",
                      "cause": "remote",
                      "message": "Lockin complete"
                    }
                  ],
                  "status": [
                    "CHANNELD_NORMAL:Funding transaction locked. Channel announced."
                  ],
                  "in_payments_offered": 1,
                  "in_mbronees_offered": 100002002,
                  "in_offered_mbro": "100002002mbro",
                  "in_payments_fulfilled": 0,
                  "in_mbronees_fulfilled": 0,
                  "in_fulfilled_mbro": "0mbro",
                  "out_payments_offered": 0,
                  "out_mbronees_offered": 0,
                  "out_offered_mbro": "0mbro",
                  "out_payments_fulfilled": 0,
                  "out_mbronees_fulfilled": 0,
                  "out_fulfilled_mbro": "0mbro",
                  "htlcs": [
                    {
                      "direction": "in",
                      "id": 0,
                      "mbronees": 100002002,
                      "amount_mbro": "100002002mbro",
                      "expiry": 131,
                      "payment_hash": "d17a42c4f7f49648064a0ce7ce848bd92c4c50f24d35fe5c3d1f3a7a9bf474b2",
                      "state": "RCVD_ADD_ACK_REVOCATION"
                    }
                  ]
                }
              ]
            },
            {
              "id": "035d2b1192dfba134e10e540875d366ebc8bc353d5aa766b80c090b39c3a5d885d",
              "connected": true,
              "netaddr": [
                "127.0.0.1:38321"
              ],
              "features": "8808226aa2",
              "channels": [
                {
                  "state": "CHANNELD_NORMAL",
                  "scratch_txid": "30530d3f522862773100b7600d8ea8921a5ee84df17a2317326f9aa2c4829326",
                  "last_tx_fee_mbro": "16149000mbro",
                  "feerate": {
                    "perkw": 11000,
                    "perkb": 44000
                  },
                  "owner": "channeld",
                  "short_channel_id": "103x1x0",
                  "direction": 0,
                  "channel_id": "006a2044fc72fa5c4a54c9fddbf208970a7b3b4fd2aaa70a96abba757c01769e",
                  "funding_txid": "9e76017c75baab960aa7aad24f3b7b0a9708f2dbfdc9544a5cfa72fc44206a00",
                  "funding_outnum": 0,
                  "close_to_addr": "bcrt1qhfmyce4ujce2pyugew2435tlwft6p6w4s3py6d",
                  "close_to": "0014ba764c66bc9632a09388cb9558d17f7257a0e9d5",
                  "private": false,
                  "opener": "local",
                  "features": [
                    "option_static_remotekey",
                    "option_anchor_outputs"
                  ],
                  "funding": {
                    "local_mbro": "1000000000mbro",
                    "remote_mbro": "0mbro",
                    "pushed_mbro": "0mbro"
                  },
                  "mbronees_to_us": 1000000000,
                  "to_us_mbro": "1000000000mbro",
                  "mbronees_to_us_min": 1000000000,
                  "min_to_us_mbro": "1000000000mbro",
                  "mbronees_to_us_max": 1000000000,
                  "max_to_us_mbro": "1000000000mbro",
                  "mbronees_total": 1000000000,
                  "total_mbro": "1000000000mbro",
                  "fee_base_mbro": "1mbro",
                  "fee_proportional_millionths": 10,
                  "dust_limit_broneess": 546,
                  "dust_limit_mbro": "546000mbro",
                  "max_total_htlc_in_mbro": "18446744073709551615mbro",
                  "their_channel_reserve_broneess": 10000,
                  "their_reserve_mbro": "10000000mbro",
                  "our_channel_reserve_broneess": 10000,
                  "our_reserve_mbro": "10000000mbro",
                  "spendable_mbronees": 749473998,
                  "spendable_mbro": "749473998mbro",
                  "receivable_mbronees": 0,
                  "receivable_mbro": "0mbro",
                  "htlc_minimum_mbro": 0,
                  "minimum_htlc_in_mbro": "0mbro",
                  "their_to_self_delay": 5,
                  "our_to_self_delay": 5,
                  "max_accepted_htlcs": 483,
                  "state_changes": [
                    {
                      "timestamp": "2022-03-25T13:57:33.325Z",
                      "old_state": "CHANNELD_AWAITING_LOCKIN",
                      "new_state": "CHANNELD_NORMAL",
                      "cause": "user",
                      "message": "Lockin complete"
                    }
                  ],
                  "status": [
                    "CHANNELD_NORMAL:Funding transaction locked. Channel announced."
                  ],
                  "in_payments_offered": 0,
                  "in_mbronees_offered": 0,
                  "in_offered_mbro": "0mbro",
                  "in_payments_fulfilled": 0,
                  "in_mbronees_fulfilled": 0,
                  "in_fulfilled_mbro": "0mbro",
                  "out_payments_offered": 2,
                  "out_mbronees_offered": 200002002,
                  "out_offered_mbro": "200002002mbro",
                  "out_payments_fulfilled": 0,
                  "out_mbronees_fulfilled": 0,
                  "out_fulfilled_mbro": "0mbro",
                  "htlcs": [
                    {
                      "direction": "out",
                      "id": 1,
                      "mbronees": 100001001,
                      "amount_mbro": "100001001mbro",
                      "expiry": 125,
                      "payment_hash": "d17a42c4f7f49648064a0ce7ce848bd92c4c50f24d35fe5c3d1f3a7a9bf474b2",
                      "state": "SENT_ADD_ACK_REVOCATION"
                    },
                    {
                      "direction": "out",
                      "id": 0,
                      "mbronees": 100001001,
                      "amount_mbro": "100001001mbro",
                      "expiry": 124,
                      "payment_hash": "d17a42c4f7f49648064a0ce7ce848bd92c4c50f24d35fe5c3d1f3a7a9bf474b2",
                      "state": "SENT_ADD_ACK_REVOCATION"
                    }
                  ]
                }
              ]
            }
          ]
        });
        let u: cln_rpc::model::ListpeersResponse = serde_json::from_value(j).unwrap();
        let g: ListpeersResponse = (&u).into();
    }
}
