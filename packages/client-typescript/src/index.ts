import { createSeed, KeyPair } from 'supercop';

export type JerryListener = (body: JerryEventBody) => void;

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
};

export class JerryClient {
  protected opts     : JerryOptions;
  protected listeners: JerryListener[] = [];
  protected sequence : number = Math.floor(Math.random() * 2**16);
  protected keypair  : Promise<KeyPair>;

  constructor(
    protected endpoint: string,
    opts: Partial<JerryOptions> = {}
  ) {
    this.opts = Object.assign({
      // Default settings
      reconnectTimeout: 5,
    }, opts);

    this.keypair = KeyPair.create(createSeed());

    // TODO: long get stream
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

    const response = await fetch(this.endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        ...data,
        sig: signature.toString('hex'),
      })
    });
  }

}
