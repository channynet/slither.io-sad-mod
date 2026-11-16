const ncs='https://ncscolour.com/en-int/pages/colour-harmonies';
const gestalt='https://pmc.ncbi.nlm.nih.gov/articles/PMC9326748/';
export const theoryCatalog=[
 {id:'wheel',name:'색상환 조화',description:'유사색·보색·삼각·사각의 색상각 관계를 후보 선택에 사용합니다.',group:'기존'},
 {id:'lightContrast',name:'명도 대비',description:'주조·보조·강조색 사이의 밝기 차이를 추가합니다.',group:'기존'},
 {id:'chromaContrast',name:'크로마 대비',description:'강조색의 선명도를 높여 역할을 구분합니다.',group:'기존'},
 {id:'temperature',name:'한난 대비',description:'해석한 따뜻함·차가움과 가까운 후보에 가중치를 줍니다.',group:'기존'},
 {id:'areaBalance',name:'면적 대비',description:'주조·보조의 사용량과 중앙 강조 배치에 가중치를 줍니다.',group:'기존'},
 {id:'interaction',name:'이웃색 대비 · 알버스 참고',description:'강조색 전후의 색차를 높이는 배치 규칙입니다.',group:'기존'},
 {id:'continuity',name:'색의 연속성',description:'일반 구간에서 이웃 색이 부드럽게 이어지도록 합니다.',group:'기존'},
 {id:'background',name:'배경 대비',description:'배경과 구분되는 밝기의 강조색을 우선합니다.',group:'기존'},
 {id:'hueUnity',name:'동일 색상 조화',description:'주조·보조색이 중심 색상각에서 멀어지지 않게 합니다.',group:'추가',source:ncs,sourceTitle:'NCS · Hue harmony',detail:'같은 색상 계열의 조화를 참고해 주조·보조 후보와 중심색의 OKLCH 색상각 차이에 벌점을 더합니다. NCS 좌표 변환이 아닌 OKLCH 색상각을 사용하는 응용입니다. 무채색의 색상각은 평가하지 않습니다. 직접 지정한 포인트는 유지합니다.'},
 {id:'nuance',name:'밝기·크로마 유사성 · NCS 참고',description:'색이 달라도 밝기·선명함을 비슷하게 맞춥니다.',group:'추가',source:ncs,sourceTitle:'NCS · Nuance harmony',detail:'NCS의 뉘앙스 공통성을 참고해 주조·보조색의 OKLab 밝기와 크로마가 중심 목표에서 멀어지는 것을 줄입니다. NCS의 흑색량·유채색량과 동일한 수치는 아닙니다.'},
 {id:'chromaUnity',name:'동일 크로마 조화',description:'선택색들의 선명함 차이를 줄이는 방향으로 고릅니다.',group:'추가',source:ncs,sourceTitle:'NCS · Chromaticness harmony',detail:'유채색 성분의 공통성 원리를 참고해 후보와 중심 목표의 OKLCH 크로마 차이에 가중치를 더합니다. NCS chromaticness를 직접 계산한 것은 아닙니다. 크로마 대비와 함께 켜면 두 경향을 절충합니다.'},
 {id:'grouping',name:'게슈탈트 · 색의 유사성',description:'가까운 색들을 짧은 구간 안에서 묶어 보이게 합니다.',group:'추가',source:gestalt,sourceTitle:'Pinna & Conti · Similarity',detail:'최종 배열에서 18칸 구간의 첫 색과 나머지 색 사이의 지각 색차를 줄이는 교환을 평가합니다. 같은 색 배열을 복제하지 않으며, 총 사용량을 유지하는 자체 배치 규칙입니다. 사람이 반드시 하나의 묶음으로 지각한다는 보장은 아닙니다.'},
 {id:'figureGround',name:'게슈탈트 · 도형과 배경',description:'주조색 무리와 구분되는 강조색을 고릅니다.',group:'추가',source:gestalt,sourceTitle:'Pinna & Conti · Figure–ground',detail:'주조 목표를 바탕, 강조색을 도형으로 삼는 설계 제안입니다. 강조색 선택에서 주조 목표와의 OKLab 색차를 높입니다. 도형·배경 지각 자체를 재현하는 시각 모델은 아닙니다.'},
 {id:'opticalMix',name:'인접색 평균 · 혼색 참고',description:'가까이 놓인 두 색의 평균이 중심 분위기에 가까워지게 합니다.',group:'추가',source:'https://www.albersfoundation.org/alberses/teaching/josef-albers/logic-magic-of-color',sourceTitle:'Albers Foundation · Color Mixing',detail:'인접한 두 색을 같은 면적으로 가정한 선형 RGB 평균을 계산하고, 평균색이 주조 목표에 가까운 쌍에 배치 가중치를 줍니다. 실제 스킨에 평균색을 추가하지 않으며 눈의 혼색·베졸트 착시를 정확히 시뮬레이션하지 않습니다.'},
] as const;
export type TheoryId=typeof theoryCatalog[number]['id'];
export type TheorySettings=Record<TheoryId,boolean>;
export const defaultTheories=Object.fromEntries(theoryCatalog.map(t=>[t.id,t.group==='기존'||t.id==='nuance'||t.id==='grouping'])) as TheorySettings;
export function resolveTheories(value:Partial<TheorySettings>={}):TheorySettings{return Object.fromEntries(theoryCatalog.map(t=>[t.id,typeof value[t.id]==='boolean'?value[t.id]:defaultTheories[t.id]])) as TheorySettings;}
