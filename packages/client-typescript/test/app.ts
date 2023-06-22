import { JerryClient, JerryEventBody } from '../src';
const client = new JerryClient('http://localhost:4000/api/v1/jerry');

client.addListener((bdy: JerryEventBody, pub?: string) => {
  console.log({ bdy, pub });
});

setTimeout(() => {
  client.emit('pizza');
  client.emit(Math.random().toString(36).slice(2));
}, 500);
