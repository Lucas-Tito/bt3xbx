// Prova da triagem com um GitHub falso: node .github/scripts/teste_triagem.js [caminho do triagem.js]
// Sai com 1 se algum caso não der o esperado. Rodar também com uma cópia estragada para vê-lo falhar.
const path = require('path');
const triagem = require(path.resolve(process.argv[2] || path.join(__dirname, 'triagem.js')));

function falso(comentarios = []) {
  const feito = [];
  const rest = { issues: {
    listComments: 'listComments',
    createComment: async (a) => { feito.push(['comenta', a.body]); comentarios.push({ body: a.body }); },
    update: async (a) => feito.push(['estado', a.state, a.state_reason || '']),
    addLabels: async (a) => feito.push(['rótulo+', a.labels.join(',')]),
    removeLabel: async (a) => feito.push(['rótulo-', a.name]),
    lock: async (a) => feito.push(['tranca', a.lock_reason]),
  } };
  const graphql = async (q, v) => feito.push(['oculta', v.c]);
  return { feito, github: { rest, graphql, paginate: async () => comentarios.slice() } };
}
const ctx = (eventName, payload) => ({ eventName, payload, repo: { owner: 'o', repo: 'r' } });
const issue = (labels, body, state = 'open', extra = {}) => ({ number: 7, labels: labels.map((name) => ({ name })), body, state, user: { login: 'autor' }, locked: false, ...extra });
const CRASH = 'BT3 crash report\r\nBuild: 1234abcd diff:da39a3ee5e\r\nPhase: luta\r\nException: C0000005  IAR: 8212ABCD  LR: 8212ABC0\r\n';
const ANEXO = 'see [crash.txt](https://github.com/user-attachments/files/123456/crash.txt)';

const casos = [
  ['queda sem crash.txt fecha na hora', ctx('issues', { action: 'opened', issue: issue(['crash', 'needs-triage'], 'it crashed') }),
    'fechada sem crash.txt', (f) => f.some((x) => x[0] === 'estado' && x[1] === 'closed') && f.some((x) => x[1] === 'needs-crash-txt')],
  ['queda com o conteúdo colado fica aberta', ctx('issues', { action: 'opened', issue: issue(['crash'], CRASH) }), 'com crash.txt', (f) => f.length === 0],
  ['queda com o anexo fica aberta', ctx('issues', { action: 'opened', issue: issue(['crash'], ANEXO) }), 'com crash.txt', (f) => f.length === 0],
  ['cabeçalho sem Build não vale', ctx('issues', { action: 'opened', issue: issue(['crash'], 'BT3 crash report\nIAR: 8212ABCD') }), 'fechada sem crash.txt', null],
  ['anexo com outro nome não vale', ctx('issues', { action: 'opened', issue: issue(['crash'], 'https://github.com/user-attachments/files/1/log.txt') }), 'fechada sem crash.txt', null],
  ['edição com o arquivo reabre', ctx('issues', { action: 'edited', issue: issue(['crash', 'needs-crash-txt'], ANEXO, 'closed') }),
    'reaberta', (f) => f.some((x) => x[0] === 'estado' && x[1] === 'open') && f.some((x) => x[0] === 'rótulo-')],
  ['comentário do autor com o arquivo reabre', ctx('issue_comment', { action: 'created', comment: { body: CRASH, user: { login: 'autor' } }, issue: issue(['crash', 'needs-crash-txt'], 'x', 'closed') }), 'reaberta', null],
  ['comentário de outro não reabre', ctx('issue_comment', { action: 'created', comment: { body: CRASH, user: { login: 'outro' } }, issue: issue(['crash', 'needs-crash-txt'], 'x', 'closed') }), 'comentário sem efeito', (f) => f.length === 0],
  ['reaberta à mão por quem cuida fica aberta', ctx('issues', { action: 'reopened', sender: { login: 'lucas' }, issue: issue(['crash'], 'x') }), 'reaberta à mão', (f) => f.length === 0],
  ['defeito sem queda não exige crash.txt', ctx('issues', { action: 'opened', issue: issue(['bug', 'needs-triage'], 'texture wrong') }), 'não é queda', (f) => f.length === 0],
  ['rótulo duplicate comenta e fecha', ctx('issues', { action: 'labeled', label: { name: 'duplicate' }, issue: issue(['bug', 'duplicate'], 'x') }),
    'resposta duplicate', (f) => f.some((x) => x[0] === 'comenta') && f.some((x) => x[1] === 'closed')],
  ['rótulo piracy fecha e tranca', ctx('issues', { action: 'labeled', label: { name: 'piracy' }, issue: issue(['bug', 'piracy'], 'x') }), 'resposta piracy', (f) => f.some((x) => x[0] === 'tranca')],
  ['rótulo needs-info comenta sem fechar', ctx('issues', { action: 'labeled', label: { name: 'needs-info' }, issue: issue(['bug', 'needs-info'], 'x') }),
    'resposta needs-info', (f) => f.some((x) => x[0] === 'comenta') && !f.some((x) => x[0] === 'estado')],
  ['rótulo feature-request comenta e fecha', ctx('issues', { action: 'labeled', label: { name: 'feature-request' }, issue: issue(['bug', 'feature-request'], 'x') }), 'resposta feature-request',
    (f) => f.some((x) => x[0] === 'comenta' && x[1].includes('only for bugs and crashes')) && f.some((x) => x[1] === 'closed')],
  ['issue com palavrão fecha, rotula e tranca', ctx('issues', { action: 'opened', issue: issue(['bug'], 'this port is sh1t, you f*ck') }), 'moderada (ofensa)',
    (f) => f.some((x) => x[1] === 'spam') && f.some((x) => x[1] === 'closed') && f.some((x) => x[0] === 'tranca')],
  ['issue com palavrão em português e acento', ctx('issues', { action: 'opened', issue: issue(['bug'], 'que PÔRRA é essa') }), 'moderada (ofensa)', null],
  ['palavrão no título também conta', ctx('issues', { action: 'opened', issue: issue(['bug'], 'texture wrong', 'open', { title: 'vai se foder' }) }), 'moderada (ofensa)', null],
  ['propaganda fecha', ctx('issues', { action: 'opened', issue: issue(['bug'], 'Best crypto airdrop, join our Telegram') }), 'moderada (propaganda)', null],
  ['muitos links fecha', ctx('issues', { action: 'opened', issue: issue(['bug'], 'a http://a b http://b c http://c d http://d e http://e') }), 'moderada (links)', null],
  ['anexos não contam como links', ctx('issues', { action: 'opened', issue: issue(['bug'], [1, 2, 3, 4, 5, 6].map((n) => `https://github.com/user-attachments/assets/${n}`).join(' ')) }), 'não é queda', (f) => f.length === 0],
  ['palavra que contém outra passa (Scunthorpe, cucumber, computador)', ctx('issues', { action: 'opened', issue: issue(['bug'], 'Scunthorpe cucumber computador escuro shitake assassin disputa') }), 'não é queda', (f) => f.length === 0],
  ['queda com crash.txt e sem ofensa segue a regra da queda', ctx('issues', { action: 'opened', issue: issue(['crash'], CRASH) }), 'com crash.txt', (f) => f.length === 0],
  ['dono do repositório nunca é moderado', ctx('issues', { action: 'opened', issue: issue(['bug'], 'porra', 'open', { author_association: 'OWNER' }) }), 'não é queda', (f) => f.length === 0],
  ['comentário ofensivo fica oculto como abuso', ctx('issue_comment', { action: 'created', comment: { body: 'filho da puta', node_id: 'C1', user: { login: 'x' } }, issue: issue(['bug'], 'x') }),
    'comentário oculto (ofensa)', (f) => f.length === 1 && f[0][0] === 'oculta' && f[0][1] === 'ABUSE'],
  ['comentário de propaganda fica oculto como spam', ctx('issue_comment', { action: 'created', comment: { body: 'clique aqui', node_id: 'C2', user: { login: 'x' } }, issue: issue(['bug'], 'x') }),
    'comentário oculto (propaganda)', (f) => f[0] && f[0][1] === 'SPAM'],
  ['comentário de colaborador não é moderado', ctx('issue_comment', { action: 'created', comment: { body: 'merda', author_association: 'COLLABORATOR', user: { login: 'x' } }, issue: issue(['bug'], 'x') }), 'não é queda', (f) => f.length === 0],
  ['rótulo confirmed não faz nada', ctx('issues', { action: 'labeled', label: { name: 'confirmed' }, issue: issue(['bug'], 'x') }), 'rótulo sem resposta', (f) => f.length === 0],
];

(async () => {
  let falhas = 0;
  for (const [nome, c, esperado, conferir] of casos) {
    const { feito, github } = falso();
    let r;
    try { r = await triagem({ github, context: c }); } catch (e) { r = `erro: ${e.message}`; }
    const ok = r === esperado && (!conferir || conferir(feito));
    if (!ok) falhas++;
    console.log(`${ok ? 'ok  ' : 'FALHA'} | ${nome}: ${r}${ok ? '' : ` (esperado ${esperado}; ${JSON.stringify(feito)})`}`);
  }
  // Toda resposta automática (por rótulo, a queda sem crash.txt e a reabertura) termina dizendo que é automática.
  const rodape = '<sub>This is an automated reply.</sub>';
  const comRodape = [
    ...Object.keys(triagem.RESPOSTAS).map((n) => ctx('issues', { action: 'labeled', label: { name: n }, issue: issue(['bug', n], 'x') })),
    ctx('issues', { action: 'opened', issue: issue(['crash'], 'it crashed') }),
    ctx('issues', { action: 'edited', issue: issue(['crash', 'needs-crash-txt'], ANEXO, 'closed') }),
    ctx('issue_comment', { action: 'created', comment: { body: CRASH, user: { login: 'autor' } }, issue: issue(['crash', 'needs-crash-txt'], 'x', 'closed') }),
  ];
  for (const c of comRodape) {
    const { feito, github } = falso();
    await triagem({ github, context: c });
    const coms = feito.filter((x) => x[0] === 'comenta');
    const ok = coms.length === 1 && coms[0][1].endsWith(rodape);
    if (!ok) falhas++;
    const quem = (c.payload.label && c.payload.label.name) || `${c.eventName} ${c.payload.action}`;
    console.log(`${ok ? 'ok  ' : 'FALHA'} | rodapé automático em ${quem}${ok ? '' : `: ${JSON.stringify(coms)}`}`);
  }
  // A mesma resposta não sai duas vezes.
  const coms = [];
  const a = falso(coms), b = falso(coms);
  const c = ctx('issues', { action: 'labeled', label: { name: 'duplicate' }, issue: issue(['duplicate'], 'x') });
  await triagem({ github: a.github, context: c }); await triagem({ github: b.github, context: c });
  const dup = b.feito.some((x) => x[0] === 'comenta');
  if (dup) falhas++;
  console.log(`${dup ? 'FALHA' : 'ok  '} | rótulo repetido não comenta de novo`);
  console.log(falhas ? `${falhas} FALHA(S)` : 'tudo certo');
  process.exit(falhas ? 1 : 0);
})();
