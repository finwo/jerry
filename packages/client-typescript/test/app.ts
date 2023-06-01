import { JerryClient } from '../src';
const client = new JerryClient('http://localhost:4000/api/v1/jerry');
client.emit('pizza');
