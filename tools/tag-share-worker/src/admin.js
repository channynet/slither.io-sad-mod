export const adminPage = `<!doctype html>
<html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Slither Native 클라이언트 관리</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#101720;color:#eef2f6}
body{max-width:1200px;margin:40px auto;padding:0 20px}h1{font-size:26px}p{color:#bdc9d8;line-height:1.7}
section{padding:20px;background:#192431;border:1px solid #33465a;border-radius:12px;margin:18px 0}
input,button{font:inherit;padding:10px;border-radius:6px;border:1px solid #54708b;background:#23354a;color:inherit;margin:4px}
input{min-width:260px}button{cursor:pointer}button:disabled{opacity:.5;cursor:default}
table{border-collapse:collapse;width:100%;font-size:14px}th,td{padding:12px;text-align:left;border-bottom:1px solid #33465a;white-space:nowrap}
.scroll{overflow:auto}.ban{color:#ff9a9a}.ok{color:#8de4b5}#message{min-height:28px;color:#ffd28b}
</style>
<h1>Slither Native 클라이언트 관리</h1>
<p>관리자는 접속 IP 주소별 사용 현황을 확인합니다. 위치는 IP 주소로 추정한 국가·도시입니다.<br>
같은 IP 주소를 사용하는 여러 사용자는 한 항목으로 표시됩니다. 차단은 이 기능이 포함된 클라이언트에 적용됩니다.</p>
<section id="login"><label>관리자 키 <input id="token" type="password" autocomplete="off" spellcheck="false"></label>
<button id="connect" type="button">접속</button></section>
<p id="message" role="status" aria-live="polite"></p>
<section id="panel" hidden>
<p id="summary"></p><button id="refresh" type="button">새로고침</button><button id="logout" type="button">로그아웃</button>
<div><label>접속 IP 주소 <input id="ip" placeholder="예: 203.0.113.10" spellcheck="false"></label>
<button id="ban" type="button">차단</button><button id="unban" type="button">차단 해제</button></div>
<p>차단 목록은 관리자가 해제할 때까지 유지됩니다. 접속 중인 클라이언트는 다음 확인 때 차단을 적용합니다.</p>
<div class="scroll"><table><thead><tr><th>접속 IP 주소</th><th>추정 국가</th><th>추정 도시</th><th>앱 버전</th><th>최근 접속</th><th>상태</th><th>관리</th></tr></thead><tbody id="rows"></tbody></table></div>
<button id="prev" type="button">이전 200개</button><button id="next" type="button">다음 200개</button>
</section>
<script>
const el=id=>document.getElementById(id);let key='',offset=0,busy=false;
const message=text=>{el('message').textContent=text;};
async function api(path,body){
 const response=await fetch('/v1/admin/'+path,{method:body?'POST':'GET',credentials:'omit',cache:'no-store',
 headers:{Authorization:'Bearer '+key,...(body?{'Content-Type':'application/json'}:{})},body:body?JSON.stringify(body):undefined});
 if(!response.ok)throw new Error(response.status===401?'관리자 키를 확인해 주세요.':'요청 실패: '+response.status);
 return response.json();
}
async function load(){
 const data=await api('clients?offset='+offset);el('rows').replaceChildren();
 el('summary').textContent='최근 '+data.retention_days+'일 동안 접속한 IP 주소 '+data.clients+'개 · 차단한 IP 주소 '+data.banned+'개';
 for(const row of data.rows){const tr=document.createElement('tr');
  for(const value of [row.ip,row.country||'미확인',row.city||'미확인',row.version||'미확인',row.last_seen?new Date(row.last_seen).toLocaleString():'접속 기록 없음',row.banned?'차단':'허용']){
   const td=document.createElement('td');td.textContent=value;tr.append(td);
  }
  tr.lastChild.className=row.banned?'ban':'ok';const td=document.createElement('td'),button=document.createElement('button');
  button.type='button';button.textContent=row.banned?'차단 해제':'차단';button.onclick=()=>run(()=>change(row.ip,!row.banned));td.append(button);tr.append(td);el('rows').append(tr);
 }
 el('prev').disabled=offset===0;el('next').disabled=data.rows.length<200;
 el('panel').hidden=false;el('login').hidden=true;message('접속 현황을 갱신했습니다.');
}
async function change(ip,banned){await api('ban',{ip:ip.trim(),banned});await load();message(banned?'접속 IP 주소를 차단했습니다.':'접속 IP 주소의 차단을 해제했습니다.');}
async function run(action){if(busy)return;busy=true;try{await action();}catch(e){message(e.message);}finally{busy=false;}}
el('connect').onclick=()=>run(async()=>{key=el('token').value.trim();el('token').value='';offset=0;await load();});
el('token').onkeydown=e=>{if(e.key==='Enter')el('connect').click();};
el('refresh').onclick=()=>run(load);el('ban').onclick=()=>run(()=>change(el('ip').value,true));el('unban').onclick=()=>run(()=>change(el('ip').value,false));
el('prev').onclick=()=>run(async()=>{offset=Math.max(0,offset-200);await load();});el('next').onclick=()=>run(async()=>{offset+=200;await load();});
el('logout').onclick=()=>{key='';location.reload();};
</script></html>`;
