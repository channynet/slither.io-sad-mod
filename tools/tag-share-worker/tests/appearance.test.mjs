import test from 'node:test';
import assert from 'node:assert/strict';
import {Room} from '../src/index.js';
const send=async(room,body)=>(await room.fetch(new Request('https://room/',{method:'POST',body:JSON.stringify(body)}))).json();
test('new clients share effects and cosmetics while legacy tags remain compatible',async()=>{
  const room=new Room({});
  await send(room,{sid:1,tag:['ntl',200],appearance:{fire:.7,cosmetic:['0123456789abcdef',160,-18,45,1]}});
  const result=await send(room,{sid:2,tag:null,appearance:{gradient:[2,1,0,0,0,0,1]}});
  assert.deepEqual(result.p,[[1,'ntl',200]]);
  assert.equal(result.a.length,2);
  assert.equal(result.a[0][1].cosmetic[2],-18);
  const removed=await send(room,{sid:1,tag:null,appearance:{}});
  assert.equal(removed.p.length,0);assert.equal(removed.a.length,1);
  const legacy=await send(room,{sid:2,tag:['ntl',201]});
  assert.deepEqual(legacy.p,[[2,'ntl',201]]);assert.deepEqual(legacy.a,[]);
});
test('untrusted appearance bounds, invalid values and expiration',async()=>{
  const room=new Room({});
  let result=await send(room,{sid:1,appearance:{fire:99,cosmetic:['0123456789abcdef',999,-999,999,1],gradient:[2,0,0]}});
  assert.deepEqual(result.a,[[1,{fire:1.5,cosmetic:['0123456789abcdef',320,-100,180,1]}]]);
  result=await send(room,{sid:2,appearance:{cosmetic:['../../bad'],gradient:[3,1]}});
  assert.equal(result.a.length,1);
  room.entries.get(1).at=Date.now()-13000;
  result=await send(room,{sid:-1});assert.deepEqual(result.a,[]);
});
