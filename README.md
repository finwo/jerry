Jerry
=====

JSON Event Relay





### Tasklist

rewrite incoming, finally got direction

#### Routes

- GET /api/v1/jerry<br/>
  Subscribe to all messages on the server (no history?)

- GET /api/v1/jerry/:topic<br/>
  Get all historic messages on the timestamp and afterwards receive new messages

- GET /api/v1/jerry/:topic?since=&lt;timestamp&gt;<br/>
  Same as above, but only include history since mentioned timestamp

- POST /api/v1/jerry/:topic<br/>
  Post a new message to a certain topic

#### Message structure

```js
{
  "alg": "ed25519",     // req, algorithm of the pubkeys & signature
  "snd": "deadbeef",    // req, hexidecimal public key of the author of the message
  "rcv": "deadbeef",    // opt, hexidecimal public key of the receiver of the message
  "typ": "edm",         // req, content-type of the message. TODO: define message types
  "iat": 12345678,      // req, timestamp in milliseconds
  "enc": "gzip,base64", // opt, encoding-order of the body
  "bdy": "base64;IQ==", // req, contents of the message
  "sig": "deadbeef",    // req, hexidecimal signature of the message
}
```

#### TODO

- Add keepalive messages (single newline character)
- Update http-server lib to support dynamic urls
- Make http-client lib, for the common code now existing for requests
  - Must support redirects (3xx)
  - Must support ssl (libcurl?)
  - Must still support long polled get
- Topics are pubkeys, only spec can mark certain topics as non-pubkey
- Historic message storage (must be flaggable)
- Make servers keep history on topics they should
- Fully rewrite jerry to support this new structure
- Authentication support

#### Reasoning

- Having a single-topic urls for pubkeys allows distributed username registrars,
  like `https://finwo.net/:username` to redirect to a specific
  server/pubkey combination.

- Having an all-topic endpoint allows for connecting multiple servers together
  providing a live feed when setting up a relay cluster.

- Having an all-topic endpoint allows for service discovery when setting up a
  task-specific network (like the music player project)

- Features can be extended to be turned in to distributed social media platform

#### Message types

Way more should be here, will update along the way

| code | content       | description                                                          |
| ---- | ------------- | -------------------------------------------------------------------- |
| edm  | tbd           | encrypted direct message                                             |
| not  | tbd           | encrypted notification                                               |
| rel  | plaintext url | indicate a (extra) relay where messages for this pubkey can be found |

---

Usage
-----

### Basic usage

```
jerry [options]

Options:
  --remote address:port
  --listen address:port
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
