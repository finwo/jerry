import { createSeed, KeyPair, PublicKey, SecretKey, Seed } from 'supercop';

export type JerryListener = (body: JerryEventBody, pubkey: string) => void;

export type JerryEventBody = null | string | number | { [index:string]: JerryEventBody };

export type JerryEvent = {

  // Protocol spec:
  //
  // | Field | Description                                           |
  // | ----- | ----------------------------------------------------- |
  // | pub   | The public key of the client that generated the event |
  // | sig   | Signature corresponding with the message and pubkey   |
  // | seq   | Sequence number, for deduplication in clusters        |
  // | bdy   | The actual data of the message                        |

  publicKey: string;
  signature: string;
  sequence : number;
  body     : JerryEventBody;
};

const seqLimit      = 2 ** 16;
const knownSeqLimit = 3;
const mod           = (n: number, m: number) => ((n%m)+m)%m;

// Emulates converting unsigned integers to signed
const sint = (n: number, m: number) => {
  const t = mod(n,m);
  if (t >= (m/2)) return n - m;
  return n;
};

export type JerryOptions = {
  reconnectTimeout: number;
  seed            : Seed;
  publicKey       : PublicKey;
  secretKey       : SecretKey;
};

export class JerryClient {
  protected active                                     = false;
  protected queue         : JerryEventBody[]           = [];
  protected queueListeners: (()=>void)[]               = [];

  protected opts          : JerryOptions;
  protected listeners     : JerryListener[]            = [];
  protected sequence      : number                     = Math.floor(Math.random() * seqLimit);
  protected keypair       : Promise<KeyPair>;
  protected knownSequences: { [index:string]: number } = {};

  constructor(
    protected endpoint: string,
    opts: Partial<JerryOptions> = {},
  ) {
    this.opts = Object.assign({
      // Default settings
      reconnectTimeout: 5,
      seed            : createSeed(),
      publicKey       : null,
      secretKey       : null,
    }, opts);

    // Build the keypair we'll use
    if (opts.publicKey && opts.secretKey) {
      this.keypair = Promise.resolve(KeyPair.from(this.opts));
    } else {
      this.keypair = KeyPair.create(this.opts.seed);
    }

    (async function retryer(_: JerryClient, delayIdx: number, delays: number[]) {
      try {
        const response = await fetch(_.endpoint);
        const reader   = response?.body?.getReader();

        // Validate connection opening
        if (!reader) throw new Error('Invalid response');
        let buffer = Buffer.alloc(0);
        delayIdx   = 0;

        for(;;) {
          const { value, done } = await reader.read();
          if (done) break;

          // Append to the list
          buffer = Buffer.concat([ buffer, Buffer.from(value) ]);

          // Read lines while there's line to be read
          let idx = buffer.indexOf('\n');
          while(idx >= 0) {
            const chunk = buffer.subarray(0, idx + 1);
            buffer      = buffer.subarray(idx + 1);
            idx         = buffer.indexOf('\n');

            try {
              const data = JSON.parse(chunk.toString());
              if (!('pub' in data)) continue;
              if (!('bdy' in data)) continue;
              if (!('seq' in data)) continue;
              if (!('sig' in data)) continue;

              // TODO: Allow single retry of out-of-order delivery
              // Something like re-add it to the queue with a certain marker?

              // Sequence-based deduplication
              if (
                (data.pub in _.knownSequences) &&
                (sint(data.seq - _.knownSequences[data.pub], seqLimit) <= 0)
              ) {
                // Negative or repeating sequence, discard message
                console.log('Discard, invalid seq');
                continue;
              }

              // Build body the signature is supposedly build with
              const signatureBody = { ...data };
              delete signatureBody.sig;

              // Build usable public key from the given hex string
              const remoteKeypair  = KeyPair.from({ publicKey: Buffer.from(data.pub, 'hex') });

              // Validate the signature given
              const signatureIsValid = await remoteKeypair.verify(
                Buffer.from(data.sig, 'hex'),
                JSON.stringify(signatureBody),
              );
              if (!signatureIsValid) {
                // Discard message
                continue;
              }

              // Store the known sequence, because we verified the message originated from the pubkey
              _.knownSequences[data.pub] = data.seq;

              // Limit knownSequences size, else it'll grow quite big on busy busses
              const sequenceKeys = Object.keys(_.knownSequences);
              if (sequenceKeys.length > knownSeqLimit) {
                const deleteIdx = Math.floor(Math.random() * sequenceKeys.length);
                delete _.knownSequences[sequenceKeys[deleteIdx]];
              }

              for(const listener of _.listeners) {
                try {
                  listener(data.bdy, data.pub);
                } catch {
                  // We do not care
                }
              }
            } catch {
              // Invalid json or invalid pubkey
            }
          }
        }

        // Nicely closed
        setTimeout(() => retryer(_, 0, delays), delays[delayIdx]);
      } catch {
        // Died
        const newIdx = Math.min(delayIdx + 1, delays.length - 1);
        setTimeout(() => retryer(_, newIdx, delays), delays[delayIdx]);
      }
    })(this, 0, [1000, 2000, 5000, 10000, 15000]); // 1, 2, 5, 10, 15 seconds
  }

  addListener(fn: JerryListener) {
    this.removeListener(fn);
    this.listeners.push(fn);
  }

  removeListener(fn: JerryListener) {
    this.listeners = this.listeners.filter(listener => listener !== fn);
  }

  async emit(body: JerryEventBody): Promise<void> {
    this.queue.push(body);

    // Add this message to the sending queue
    if (this.active) {
      return new Promise<void>(resolve => {
        this.queueListeners.push(() => resolve());
      });
    }

    // Mark the sender as active, preventing parallel requests
    this.active = true;

    while(this.queue.length) {
      const seq = this.sequence = mod(this.sequence + 1, seqLimit);
      const bdy = this.queue.shift();

      const data = {
        pub: (await this.keypair).publicKey?.toString('hex'),
        bdy,
        seq,
      };

      const validationMessage = JSON.stringify(data);
      console.log('Msg', validationMessage.length, validationMessage);
      const signature = await (await this.keypair).sign(JSON.stringify(data));

      const isValid = await (await this.keypair).verify(signature,validationMessage);
      console.log(isValid);

      try {
        await fetch(this.endpoint, {
          method : 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            ...data,
            sig: signature.toString('hex'),
          }),
        });
      } catch {
        // Don't care for now
      }

    }

    // We're done sending, resolve pending promises
    this.active = false;
    while(this.queueListeners.length) {
      const listener = this.queueListeners.shift();
      if (!listener) continue;
      try {
        listener();
      } catch {
        // Don't care for now
      }
    }

  }

}
