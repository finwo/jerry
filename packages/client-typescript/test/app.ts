import { JerryClient, JerryEventBody } from '../src';
const client = new JerryClient('http://localhost:4000/api/v1/jerry');

client.addListener((bdy: JerryEventBody, pub?: string) => {
  console.log({ bdy, pub });
});

client.emit('pizza');
