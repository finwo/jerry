Jerry
=====

JSON Event Relay

Usage
-----

### Basic usage

```
jerry [--remote <hostname>:<port>] <listen-address>
```

### Message structure

A message is simply a json-encoded object, where certain fields have meaning (extra fields should be rejected)

| Field | Description                                           |
| ----- | ----------------------------------------------------- |
| pub   | The public key of the client that generated the event |
| sig   | Signature corresponding with the message and pubkey   |
| seq   | Sequence number, for deduplication in clusters        |
| bdy   | The actual data of the message                        |

### Transport

In short: HTTP, GET chunked transfer, POST short-lived
