/* ════════════════════════════════════════════════════════════
   RC Column Fire-Resistance — frontend logic
   3 бүлэг арматур: As1 (булан) · As2 (гол) · As3 (нэмэлт)
   As = π·d²/4 · n  (автомат) · γ бүлэг бүрт тусдаа · φ авто-интерп.
═══════════════════════════════════════════════════════════════ */
function fmt(x,n=2){return Number.isFinite(x)?x.toFixed(n):'—';}
function getNum(id){const el=document.getElementById(id);return el?+el.value:NaN;}

function tempColor(temp){
  if(temp<100)return'#5aa9ff';
  if(temp<300)return'#3ddc84';
  if(temp<500)return'#ffd166';
  if(temp<700)return'#ff9957';
  return'#ff5c5c';
}

let chartObj=null;
window._lastResult=null;
window._lastInputs=null;
window._currentTau=0;

/* ─────────── As = π·d²/4·n ─────────── */
function areaOf(d,n){
  const dd=parseFloat(d),nn=parseInt(n);
  if(!dd||!nn||dd<=0||nn<=0||isNaN(dd)||isNaN(nn))return 0;
  return Math.round(Math.PI*dd*dd/4*nn);
}

/* As талбаруудыг шинэчлэх (input өөрчлөгдөх бүрт) */
function refreshArm(){
  const As1=areaOf(getNum('d1'),getNum('n1'));
  const As2=areaOf(getNum('d2'),getNum('n2'));
  const As3=areaOf(getNum('d3'),getNum('n3'));
  const set=(id,v)=>{const el=document.getElementById(id);if(el){el.textContent=v>0?v.toLocaleString()+' мм²':'—';el.className='arm-result '+(v>0?'arm-result-ok':'');}};
  set('As1disp',As1);set('As2disp',As2);set('As3disp',As3);
  const tot=As1+As2+As3;
  const totEl=document.getElementById('AsTotDisp');
  if(totEl){totEl.textContent=tot>0?tot.toLocaleString()+' мм²':'—';totEl.className='arm-result '+(tot>0?'arm-result-ok':'');}
  // Хэрэв тооцоо хийгдсэн бол сечениг шинэчлэх (preview)
  if(!window._lastResult)drawStaticPreview();
}

function collectArm(){
  return{
    d1:getNum('d1'),n1:getNum('n1'),a1:getNum('a1'),As1:areaOf(getNum('d1'),getNum('n1')),
    d2:getNum('d2'),n2:getNum('n2'),a2:getNum('a2'),As2:areaOf(getNum('d2'),getNum('n2')),
    d3:getNum('d3'),n3:getNum('n3'),a3:getNum('a3'),As3:areaOf(getNum('d3'),getNum('n3')),
  };
}

/* ─────────── MAIN CALC ─────────── */
async function calc(){
  const arm=collectArm();
  const inputs={
    b:getNum('b'),h:getNum('h'),H0:getNum('H0'),kL:getNum('kL'),
    Rbn:getNum('Rbn'),Rsn:getNum('Rsn'),rho:getNum('rho'),
    W:getNum('W'),tb:getNum('tb'),t0:getNum('t0'),
    As1:arm.As1,As2:arm.As2,As3:arm.As3,
    a1:arm.a1,a2:arm.a2,a3:arm.a3,
    Np:getNum('Np'),step:getNum('step'),tmax:getNum('tmax'),
  };
  const phiV=document.getElementById('phiManual').value.trim();
  if(phiV!=='')inputs.phiManual=+phiV;

  const bad=['b','h','H0','Rbn','Rsn','rho','Np'].filter(k=>!inputs[k]||inputs[k]<=0);
  if(bad.length){alert(t('alertCheck')+bad.join(', '));return;}
  if(arm.As1+arm.As2+arm.As3<=0){alert(t('alertNoRebar'));return;}

  // Backend нь c1/c2-ийн оронд a1/a2/a3 авна — гэхдээ хуучин түлхүүр дэмжихийн тулд
  inputs.c1=arm.a1; inputs.c2=arm.a2;

  let result;
  try{
    const res=await fetch('/api/calculate',{
      method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify(inputs),
    });
    result=await res.json();
    if(!res.ok||result.error){alert(t('errBackend')+(result.error||res.status));return;}
  }catch(e){alert(t('errBackend')+e.message);return;}

  // Огтлол зурахад хэрэгтэй арматурын тоог хадгалах
  inputs._arm=arm;
  window._lastResult=result;
  window._lastInputs=inputs;
  window._currentTau=0;

  const slider=document.getElementById('tauSlider');
  slider.disabled=false;slider.min=0;
  slider.max=result.chartRows?.length?result.chartRows[result.chartRows.length-1].tau:0;
  slider.step=1;slider.value=0;

  window._renderLastResult();
}

/* ════════════════════════════════════════════════════════════
   CROSS-SECTION SVG — бодит байршил
   As1 → 4 булан · As2 → гол босоо тэнхлэг · As3 → доод/дээд мөрд нэмэлт
═══════════════════════════════════════════════════════════════ */
/* ════════════════════════════════════════════════════════════
   placeRebars — БҮЛЭГ БҮРИЙГ ХЭРЧМИЙН ЗАХАД БАЙРШУУЛНА
   ─────────────────────────────────────────────────────────
   As1 (group 1) — БУЛАНГИЙН стержнүүд
     • Үргэлж 4 булан дээр: (a1,a1),(b-a1,a1),(a1,h-a1),(b-a1,h-a1)
     • n1>4 бол илүүдлийг доод+дээд граниар буланнуудын хооронд тараана
     • Хоёр талаас халдаг → γ хамгийн хурдан буурна

   As2 (group 2) — ХАЖУУГИЙН (зүүн+баруун) граний стержнүүд
     • Зүүн грань дээр x=a2, баруун грань дээр x=b-a2
     • n2 стержнийг зүүн/баруун тал тус бүрт жигд тараана
     • Нэг талаас халдаг, a2 зайд → γ дунд зэрэг буурна

   As3 (group 3) — ДООД+ДЭЭД граний нэмэлт стержнүүд
     • Доод грань дээр y=a3, дээд грань дээр y=h-a3
     • Буланнуудын ХООРОНД (As1-тэй зэрэгцэхгүй) байршина
     • Нэг талаас халдаг → γ дунд зэрэг буурна

   ДҮРЭМ: ЖОДОО бүх арматур хэрчмийн захад (perimeter) байна.
          Дотор "хөвж буй" стержень байхгүй.
═══════════════════════════════════════════════════════════════ */
function placeRebars(b,h,a1,a2,a3,n1,n2,n3){
  const pts=[];

  /* ── As1: Булангийн стержнүүд ── */
  if(n1>0){
    const corners=[[a1,a1],[b-a1,a1],[b-a1,h-a1],[a1,h-a1]]; // цагийн зүүтэй эрэмбэ
    const take=Math.min(n1,4);
    for(let i=0;i<take;i++) pts.push({x:corners[i][0],y:corners[i][1],g:1});
    // n1>4: илүүдэл стержнүүдийг доод ба дээд граниар тэнцүү тараана
    if(n1>4){
      const extra=n1-4;
      const bot=Math.ceil(extra/2), top=extra-bot;
      for(let i=0;i<bot;i++){
        const x=a1+(b-2*a1)*(i+1)/(bot+1);
        pts.push({x,y:a1,g:1});
      }
      for(let i=0;i<top;i++){
        const x=a1+(b-2*a1)*(i+1)/(top+1);
        pts.push({x,y:h-a1,g:1});
      }
    }
  }

  /* ── As2: доод+дээд граний стержнүүд (нэмэлт, буланнуудын хооронд) ──
     a2 = доод/дээд граниас тэнхлэг хүртэлх зай
     Буланнуудын ХООРОНД байрлана (As1-тэй давхцахгүй)              */
  if(n2>0){
    const bot=Math.ceil(n2/2), top=n2-bot;
    const xLo=a1+Math.max(a2*0.5,8), xHi=b-a1-Math.max(a2*0.5,8);
    for(let i=0;i<bot;i++){
      const x=xLo+(xHi-xLo)*(bot===1?0.5:i/(bot-1));
      pts.push({x,y:a2,g:2});
    }
    for(let i=0;i<top;i++){
      const x=xLo+(xHi-xLo)*(top===1?0.5:i/(top-1));
      pts.push({x,y:h-a2,g:2});
    }
  }

  /* ── As3: зүүн+баруун граний стержнүүд (хажуугийн) ──
     a3 = зүүн/баруун граниас тэнхлэг хүртэлх зай
     n3 стержнийг зүүн/баруун тал тус бүрт жигд тараана             */
  if(n3>0){
    const perSide=Math.ceil(n3/2), leftN=perSide, rightN=n3-perSide;
    const yLo=a1+Math.max(a3*0.5,8), yHi=h-a1-Math.max(a3*0.5,8);
    const place=(n,xFace)=>{
      if(n<=0)return;
      if(n===1){pts.push({x:xFace,y:(yLo+yHi)/2,g:3});return;}
      for(let i=0;i<n;i++) pts.push({x:xFace,y:yLo+(yHi-yLo)*i/(n-1),g:3});
    };
    place(leftN, a3);
    place(rightN, b-a3);
  }

  return pts;
}

function drawCrossSection(b,h,arm,delta,ts1,ts2,ts3,tau){
  const SZ=480,pad=70,drawW=SZ-2*pad;
  const scale=drawW/Math.max(b,h);
  const bs=b*scale,hs=h*scale;
  const ox=(SZ-bs)/2,oy=(SZ-hs)/2;
  const ds=Math.max(0,Math.min(delta*scale,Math.min(bs,hs)/2-2));

  const cols={1:tempColor(ts1),2:tempColor(ts2),3:tempColor(ts3)};
  const pts=placeRebars(b,h,arm.a1||50,arm.a2||50,arm.a3||50,
                        arm.n1||0,arm.n2||0,arm.n3||0);

  // Стержний радиус — диаметрээс гаргаж, дэлгэцэд харагдахаар
  const rOf=(d)=>Math.max(7,Math.min(22,d*scale/2));
  const rad={1:rOf(arm.d1||32),2:rOf(arm.d2||25),3:rOf(arm.d3||25)};

  let svg=`<svg viewBox="0 0 ${SZ} ${SZ}" xmlns="http://www.w3.org/2000/svg">`;

  // Шатсан давхрага хэв
  svg+=`<defs>
    <pattern id="burnt" patternUnits="userSpaceOnUse" width="8" height="8" patternTransform="rotate(45)">
      <rect width="8" height="8" fill="#ff7a1a"/>
      <rect width="4" height="8" fill="#5a2a08"/>
    </pattern>
  </defs>`;

  // Гадна бетон (шатсан давхрага харуулна)
  svg+=`<rect x="${ox}" y="${oy}" width="${bs}" height="${hs}"
    fill="url(#burnt)" stroke="#888" stroke-width="1.5"/>`;

  // Идэвхтэй дотор бетон
  const iw=Math.max(0,bs-2*ds), ih=Math.max(0,hs-2*ds);
  if(iw>2&&ih>2)
    svg+=`<rect x="${(ox+ds).toFixed(1)}" y="${(oy+ds).toFixed(1)}"
      width="${iw.toFixed(1)}" height="${ih.toFixed(1)}"
      fill="#2e3a48" stroke="#4a5a6a" stroke-width="1"/>`;

  // δ хэмжүүр (шатсан давхрагын зузаан)
  if(delta>1){
    svg+=`<line x1="${ox}" y1="${oy-10}" x2="${(ox+ds).toFixed(1)}" y2="${oy-10}"
      stroke="#ff7a1a" stroke-width="2" marker-end="url(#arr)"/>`;
    svg+=`<text x="${(ox+ds/2).toFixed(1)}" y="${oy-14}"
      text-anchor="middle" fill="#ff9957" font-size="11"
      font-family="Consolas,monospace">δ=${fmt(delta,1)} мм</text>`;
  }

  // ── Арматурууд ──
  // y: SVG-д дээш = бага тул: py = oy + hs - y*scale
  for(const p of pts){
    const px=(ox+p.x*scale).toFixed(1);
    const py=(oy+hs-p.y*scale).toFixed(1);
    const r=rad[p.g];
    const col=cols[p.g];
    const gLabel={1:'1',2:'2',3:'3'}[p.g];

    // Стержний дугуй (хоёр давхар давхрага — бодит стержний харагдац)
    svg+=`<circle cx="${px}" cy="${py}" r="${r+2}"
      fill="${col}" fill-opacity="0.25" stroke="${col}" stroke-width="1"/>`;
    svg+=`<circle cx="${px}" cy="${py}" r="${r}"
      fill="${col}" fill-opacity="0.92" stroke="#0b0f14" stroke-width="1.5"/>`;

    // Бүлгийн дугаар (1/2/3)
    svg+=`<text cx="${px}" cy="${py}"
      x="${px}" y="${(parseFloat(py)+4).toFixed(1)}"
      text-anchor="middle" fill="#0b0f14"
      font-size="${Math.max(8,r*0.85).toFixed(0)}"
      font-weight="800" font-family="Consolas,monospace">${gLabel}</text>`;
  }

  // Хэмжээс
  svg+=`<text x="${(ox+bs/2).toFixed(1)}" y="${(oy+hs+24).toFixed(1)}"
    text-anchor="middle" fill="#9aa7b5" font-size="13"
    font-family="Consolas,monospace">b = ${b} мм</text>`;
  svg+=`<text x="${(ox-22).toFixed(1)}" y="${(oy+hs/2).toFixed(1)}"
    text-anchor="middle" fill="#9aa7b5" font-size="13"
    font-family="Consolas,monospace"
    transform="rotate(-90 ${(ox-22).toFixed(1)} ${(oy+hs/2).toFixed(1)})">h = ${h} мм</text>`;

  // τ ба бүлэг бүрийн халалт
  const infoY=oy+hs+46;
  svg+=`<g font-family="Consolas,monospace">
    <text x="${ox}" y="${oy-30}" fill="#ff9957" font-size="14"
      font-weight="700">τ = ${tau}${t('lblMin')}</text>
    <circle cx="${ox+8}"        cy="${infoY-4}" r="6" fill="${cols[1]}"/>
    <circle cx="${ox+8}"        cy="${infoY+14}" r="6" fill="${cols[2]}"/>
    <circle cx="${ox+8}"        cy="${infoY+32}" r="6" fill="${cols[3]}"/>
    <text x="${ox+20}" y="${infoY}"
      fill="#e7edf5" font-size="11">As₁ булан  ${fmt(ts1,0)} °C  γ₁=${fmt(ts1<300?1:ts1<600?0.97-(ts1-300)*0.001833:0.03,3)}</text>
    <text x="${ox+20}" y="${infoY+18}"
      fill="#e7edf5" font-size="11">As₂ хажуу  ${fmt(ts2,0)} °C</text>
    <text x="${ox+20}" y="${infoY+36}"
      fill="#e7edf5" font-size="11">As₃ нэмэлт ${fmt(ts3,0)} °C</text>
  </g>`;

  svg+=`</svg>`;
  return svg;
}

function drawStaticPreview(){
  const arm=collectArm();
  const b=getNum('b')||300,h=getNum('h')||300;
  document.getElementById('sectionSvg').innerHTML=drawCrossSection(b,h,arm,0,20,20,20,0);
}

function chartRowAt(tau){
  const r=window._lastResult;
  if(!r||!r.chartRows||!r.chartRows.length)return null;
  return r.chartRows[Math.max(0,Math.min(r.chartRows.length-1,Math.round(tau)))];
}

function refreshSection(){
  const r=window._lastResult,inp=window._lastInputs;
  if(!r||!inp)return;
  const cr=chartRowAt(window._currentTau);
  if(!cr)return;
  document.getElementById('tauReadout').textContent=`τ = ${cr.tau}${t('lblMin')}`;
  document.getElementById('sectionSvg').innerHTML=drawCrossSection(
    inp.b,inp.h,inp._arm,cr.delta,cr.ts1,cr.ts2,cr.ts3,cr.tau);
}

/* ════════════════════════════════════════════════════════════
   RENDER RESULTS
═══════════════════════════════════════════════════════════════ */
window._renderLastResult=function(){
  const r=window._lastResult;
  if(!r)return;

  document.getElementById('paramRows').innerHTML=
    `<tr><th>${t('rowLambdaT')}</th><td>${fmt(r.lambdaTem,4)} ${t('uWmC')}</td></tr>`+
    `<tr><th>${t('rowCT')}</th><td>${fmt(r.cTem,0)} ${t('uJkgC')}</td></tr>`+
    `<tr><th>${t('rowARed')}</th><td>${fmt(r.aRed,4)} ${t('uMm2s')}</td></tr>`+
    `<tr><th>${t('rowKb')}</th><td>${fmt(r.kbS,1)} ${t('uMm')}</td></tr>`;

  const phiNote=r.phiManual?t('phiManualNote'):t('phiAutoNote');
  document.getElementById('zeroRows').innerHTML=
    `<tr><th>${t('rowL0')}</th><td>${fmt(r.l0,0)} ${t('uMm')}</td></tr>`+
    `<tr><th>${t('rowLambda')}</th><td>${fmt(r.lambda,2)}</td></tr>`+
    `<tr><th>${t('rowPhi')}</th><td>${fmt(r.phi,3)} <span class="${r.phiManual?'':'ok'}">${phiNote}</span></td></tr>`+
    `<tr><th>${t('rowAsTot')}</th><td>${fmt(r.AsTot,0)} ${t('uMm2')} <span class="mini">(${fmt(r.As1,0)}+${fmt(r.As2,0)}+${fmt(r.As3,0)})</span></td></tr>`+
    `<tr><th>${t('rowN0')}</th><td>${fmt(r.N0,0)} ${t('uKn')}</td></tr>`+
    `<tr><th>${t('rowCheck')}</th><td class="${r.N0pass?'ok':'bad'}">${r.N0pass?t('okMsg'):t('badMsg')}</td></tr>`;

  document.getElementById('timeRows').innerHTML=(r.rows||[]).map(row=>
    `<tr><td>${row.tau}</td>`+
    `<td>${row.root?fmt(row.root,0):'—'}</td>`+
    `<td>${fmt(row.delta,1)}</td>`+
    `<td style="color:${tempColor(row.ts1)}">${fmt(row.ts1,0)}</td>`+
    `<td style="color:${tempColor(row.ts2)}">${fmt(row.ts2,0)}</td>`+
    `<td style="color:${tempColor(row.ts3)}">${fmt(row.ts3,0)}</td>`+
    `<td>${fmt(row.g1,3)}</td><td>${fmt(row.g2,3)}</td><td>${fmt(row.g3,3)}</td>`+
    `<td class="${row.ok?'ok':'bad'}">${fmt(row.Nu,0)}</td>`+
    `<td class="${row.ok?'ok':'bad'}">${row.ok?t('okShort'):t('badShort')}</td></tr>`
  ).join('');

  // Пф
  let pfText='',pfExact=null;
  if(r.verdict==='more'){pfText=t('pfMore').replace('{0}',r.tauLast);}
  else if(r.verdict==='zero'){pfText=t('pfZero');pfExact=0;}
  else{const tR=Math.round(r.tExact*10)/10;pfText=t('pfApprox').replace('{0}',tR);pfExact=r.tExact;}

  document.getElementById('pf').textContent=pfText;
  document.getElementById('conclusion').innerHTML=
    t('conclTpl').replace('{Np}',fmt(r.Np,0)).replace('{pf}',pfText)
      .replace('{extra}',r.verdict==='more'?t('conclPass'):t('conclFail'));

  // Chart
  const cr=r.chartRows||[];
  const labels=cr.map(x=>x.tau);
  const capData=cr.map(x=>+x.Nu.toFixed(1));
  const loadData=cr.map(()=>r.Np);

  const pfPlugin={
    id:'pfAnnotation',
    afterDraw(chart){
      if(pfExact===null||pfExact<=0)return;
      const{ctx,chartArea:{top,bottom},scales:{x}}=chart;
      const xPx=x.getPixelForValue(pfExact);
      if(xPx<x.left||xPx>x.right)return;
      ctx.save();
      ctx.beginPath();ctx.moveTo(xPx,top);ctx.lineTo(xPx,bottom);
      ctx.strokeStyle='#ffd166';ctx.lineWidth=2.5;ctx.setLineDash([7,5]);ctx.stroke();
      ctx.setLineDash([]);
      const label=`Пф = ${Math.round(pfExact*10)/10}${t('lblMin')}`;
      ctx.font='bold 12px Consolas,monospace';
      const tw=ctx.measureText(label).width;
      const bx=Math.min(Math.max(xPx-tw/2-8,x.left+2),x.right-tw-18);
      ctx.fillStyle='rgba(10,15,22,0.85)';
      ctx.beginPath();
      if(ctx.roundRect)ctx.roundRect(bx,top+4,tw+16,22,5);else ctx.rect(bx,top+4,tw+16,22);
      ctx.fill();ctx.strokeStyle='#ffd166';ctx.lineWidth=1;ctx.stroke();
      ctx.fillStyle='#ffd166';ctx.textAlign='left';ctx.fillText(label,bx+8,top+19);
      ctx.restore();
    }
  };

  if(chartObj)chartObj.destroy();
  chartObj=new Chart(document.getElementById('chart'),{
    type:'line',plugins:[pfPlugin],
    data:{labels,datasets:[
      {label:t('chartCap'),data:capData,borderColor:'#ff7a1a',backgroundColor:'rgba(255,122,26,.14)',fill:true,tension:.25,pointRadius:0,borderWidth:2},
      {label:t('chartLoad'),data:loadData,borderColor:'#5aa9ff',borderDash:[8,5],fill:false,pointRadius:0,borderWidth:2},
    ]},
    options:{responsive:true,interaction:{mode:'index',intersect:false},
      plugins:{legend:{labels:{color:'#e7edf5'}},
        tooltip:{callbacks:{title:items=>items[0].label+t('lblMin'),label:ctx=>ctx.dataset.label+': '+ctx.parsed.y+' '+t('uKn')}}},
      scales:{
        x:{ticks:{color:'#9aa7b5',maxTicksLimit:13,callback:function(v){const tau=labels[v];return(tau%50===0)?tau+t('lblMin'):'';}},
           grid:{color:'rgba(255,255,255,.06)'},title:{display:true,text:t('axisTime'),color:'#9aa7b5'}},
        y:{ticks:{color:'#9aa7b5'},grid:{color:'rgba(255,255,255,.06)'},title:{display:true,text:t('axisKn'),color:'#9aa7b5'}},
      }},
  });

  refreshSection();
};

/* ─────────── ПРИМЕР ─────────── */
function loadExample(){
  const g=(id,v)=>{const el=document.getElementById(id);if(el)el.value=v;};
  g('b',300);g('h',300);g('H0',4000);g('kL',0.8);
  g('Rbn',22);g('Rsn',400);g('rho',2300);g('W',2);g('tb',450);g('t0',20);
  // 3 бүлэг: булан 4×Ø32, гол 2×Ø25, нэмэлт 2×Ø25
  g('d1',32);g('n1',4);g('a1',50);
  g('d2',25);g('n2',2);g('a2',50);
  g('d3',25);g('n3',2);g('a3',50);
  g('Np',2354);g('tmax',500);g('step','60');g('phiManual','');
  refreshArm();
  calc();
}

/* ─────────── SLIDER ─────────── */
document.getElementById('tauSlider').addEventListener('input',e=>{
  window._currentTau=+e.target.value;refreshSection();
});

window.calc=calc;window.loadExample=loadExample;window.refreshArm=refreshArm;

document.getElementById('year').textContent=new Date().getFullYear();

(function initLang(){
  let saved='ru';try{saved=localStorage.getItem('siteLang')||'ru';}catch(e){}
  setLang(saved);
})();

(function init(){
  refreshArm();
  drawStaticPreview();
})();
