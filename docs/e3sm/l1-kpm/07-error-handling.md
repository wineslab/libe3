# Handling of unknown, unforeseen and erroneous protocol data

## General

Clause 9.6 of [1] defines the handling of errors in the E3AP messages. This clause adds
the handling of errors in the IEs of the present document. An error in a service model
IE shall not end the E3AP association and shall not end the subscription.

An E3 Agent that cannot encode the INDICATION DATA IE of a slot shall not send that
indication, and continues with the next slot. When the E3 Agent cannot send an
indication to one dApp, it still sends it to the other dApps.

## Abstract syntax errors

An abstract syntax error is a payload that the receiver cannot decode: an APER encoding
that does not match the grammar of clause 8.4, or a JSON value that is not valid JSON or
whose members do not have the JSON types of clause 8.6.

Table: Handling of abstract syntax errors

| Receiver | Payload | Action |
|---|---|---|
| dApp | INDICATION DATA IE | Discard the indication. Keep the subscription. |
| dApp | RAN FUNCTION DATA IE | Do not use RAN function `2` in this association. |
| E3 Agent | CONTROL DATA IE | Answer with a MESSAGE ACK message "negative". Keep the previous subscription data. |

## Unknown fields and values

- A receiver shall ignore an extension addition that it does not know (clause 4.4).
- A JSON receiver shall ignore a member that it does not know.
- A JSON receiver that finds a value outside the range of clause 8.4 shall treat the field as absent.
- A dApp that receives a RAN FUNCTION DATA IE whose `version` it does not support shall not decode the payloads of RAN function `2`.
- An E3 Agent that receives a CONTROL DATA IE with an alternative that it does not know shall answer with a MESSAGE ACK message "negative".

## Fields that do not match the subscription

- A dApp shall accept an indication that holds a field whose identifier is not granted. It may ignore that field.
- A dApp shall treat a granted field that is absent as not reported for that slot.
- When `nCells`, `nUe` or `nSrsUe` and the length of the list that it counts differ, the list is right. The dApp should use the length of the list.

## Erroneous control actions

The E3 Agent shall answer with a MESSAGE ACK message "negative", and keep the previous
subscription data, in each of the following cases.

- The `controlIdentifier` is not `1`.
- The alternative of the CONTROL DATA IE does not match the `controlIdentifier`.
- The dApp holds no subscription for RAN function `2`, or control identifier `1` is not granted to it.
- A filter names a cell that the E3 Agent does not serve.

A UE filter that names an RNTI that is not in use is not an error. The UE is then absent
from the indications.

## Erroneous shared memory references

A dApp shall not read the region for a shared memory reference in each of the following
cases. It may use the other fields of the indication.

- `shmName` differs from the `memoryKey` of the stream descriptor.
- `bufferIndex` is not `0` or `1`.
- `writeIndex` is not below `numRows` of the stream descriptor.
- The data that the reference and the UE scope locators give does not lie inside the buffer of the stream.
- The header of the region has a version that the dApp does not support (Annex A.3).

A dApp that cannot open the region shall not use the shared memory streams. It may use
the inline streams.

## Indications that arrive late

The E3 Agent does not wait for a dApp before it reuses a row. A dApp that reads a row too
late reads the data of a later slot. Annex A.4 gives the rules that let a dApp detect
this case. A dApp that detects it shall discard the data of that row.
