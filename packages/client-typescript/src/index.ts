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

export type JerryOptions = {
  reconnectTimeout: number;
  seed            : Seed;
  publicKey       : PublicKey;
  secretKey       : SecretKey;
};

export class JerryClient {
  protected opts     : JerryOptions;
  protected listeners: JerryListener[] = [];
  protected sequence : number = Math.floor(Math.random() * 2**16);
  protected keypair  : Promise<KeyPair>;

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
              if (!('bdy' in data)) continue;

              // TODO: verify signature

              for(const listener of _.listeners) {
                try {
                  listener(data.bdy, data.pub);
                } catch {
                  // We do not care
                }
              }
            } catch {
              // Invalid json
            }
          }
        }

        // Nicely closed
        setTimeout(() => retryer(_, 0, delays), delays[delayIdx]);
      } catch {
        // Died
        const newIdx = Math.min(delayIdx + 1, delays.length - 1);
        console.log(newIdx);
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

    const data = {
      pub: (await this.keypair).publicKey?.toString('hex'),
      bdy: body,
      seq: this.sequence++,
    };

    const signature = await (await this.keypair).sign(JSON.stringify(data));

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
  }

}
