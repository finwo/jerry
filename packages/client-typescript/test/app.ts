import { JerryClient, JerryEventBody } from '../src';
const client = new JerryClient('http://localhost:4000/api/v1/jerry');

client.addListener((bdy: JerryEventBody) => {
  console.log({ bdy });
});

client.emit('pizza');
