// Triagem automática das issues do port do 360 (decisão do Lucas, 29/10/2026). Chamado pelo
// .github/workflows/issues-x360.yml com o github e o context do actions/github-script.
//
// 1. Relatório de queda sem crash.txt (anexo ou o conteúdo colado) fecha na hora, com o rótulo needs-crash-txt.
//    Se o autor editar a issue ou comentar com o crash.txt, ela reabre sozinha.
// 2. Quando quem cuida do repositório põe um rótulo de motivo, sai a resposta pronta dele e, conforme o
//    rótulo, a issue fecha (e tranca, na pirataria). Nada fecha por prazo.
// Teste: node .github/scripts/teste_triagem.js

const MARCA = (nome) => `<!-- bt3-triagem:${nome} -->`;
const README = 'https://github.com/Lucas-Tito/bt3xbx/blob/main/port/x360/package/README.txt';
const REGRAS = 'https://github.com/Lucas-Tito/bt3xbx/blob/main/.github/CONTRIBUTING.md';

// O cabeçalho que o jogo grava (port/x360/src/queda360.h) ou um anexo chamado crash*.txt.
function temCrashTxt(texto) {
  if (!texto) return false;
  const colado = /BT3 crash report/.test(texto) && /^Build: \S+/m.test(texto) && /IAR: [0-9A-F]{8}/.test(texto);
  const anexo = /https:\/\/github\.com\/(?:user-attachments\/files|[^\s/]+\/[^\s/]+\/files)\/\d+\/crash[^\s/)]*\.txt/i.test(texto);
  return colado || anexo;
}

const SEM_CRASH = `${MARCA('sem-crash')}
This crash report was closed automatically because it has no crash.txt.

The game saves crash.txt in its folder on the console, next to default.xex, when it shows the "Unexpected error" box. Attach the file to this issue (drag it into the text box) or paste its whole content, by editing the issue or in a comment, and the issue reopens by itself.

If the game closed without that box, or froze, use the Bug report form instead. See the [rules](${REGRAS}).`;

const REABERTA = `${MARCA('reaberta')}
crash.txt received; the issue is open again and will be looked at.`;

// Rótulo de motivo -> resposta, e se fecha ou tranca.
const RESPOSTAS = {
  'duplicate': { fecha: 'not_planned', texto: 'This was already reported. Please follow the issue linked here and add your details there.' },
  'not-supported': { fecha: 'not_planned', texto: `This port supports only the unmodified USA release (SLUS-21678) on an Xbox 360 with RGH or JTAG. Other versions of the game, modified ISOs, mods and emulators are not supported. See the [README](${README}).` },
  'off-topic': { fecha: 'not_planned', texto: 'This is not about the Xbox 360 port. Problems of the PC version belong to [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp/issues).' },
  'known-issue': { fecha: 'not_planned', texto: `This is already listed in the known issues of the [README](${README}).` },
  'feature-request': { fecha: 'not_planned', texto: 'Thanks for the idea. This first version keeps the game as it is, so feature requests are not tracked as issues for now.' },
  'wontfix': { fecha: 'not_planned', texto: 'This will not be changed.' },
  'piracy': { fecha: 'not_planned', tranca: true, texto: 'This project never provides the game, ISOs or game files, and does not allow links to them here.' },
  'needs-crash-txt': { texto: 'Please attach crash.txt from the game folder (drag it into a comment) or paste its whole content.' },
  'needs-info': { texto: 'Some information is missing or unclear. Please answer by editing the issue or in a comment.' },
};

async function jaComentou(github, alvo, nome) {
  const lista = await github.paginate(github.rest.issues.listComments, { ...alvo, per_page: 100 });
  return lista.some((c) => (c.body || '').includes(MARCA(nome)));
}

async function comentar(github, alvo, nome, texto) {
  if (await jaComentou(github, alvo, nome)) return false;
  await github.rest.issues.createComment({ ...alvo, body: texto.includes(MARCA(nome)) ? texto : `${MARCA(nome)}\n${texto}` });
  return true;
}

module.exports = async function triagem({ github, context }) {
  const { eventName, payload } = context;
  const issue = payload.issue;
  if (!issue || issue.pull_request) return 'ignorado';
  const alvo = { owner: context.repo.owner, repo: context.repo.repo, issue_number: issue.number };
  const rotulos = (issue.labels || []).map((l) => (typeof l === 'string' ? l : l.name));
  const ehQueda = rotulos.includes('crash');

  if (eventName === 'issues' && payload.action === 'labeled') {
    const nome = payload.label && payload.label.name;
    const r = RESPOSTAS[nome];
    if (!r) return 'rótulo sem resposta';
    await comentar(github, alvo, nome, r.texto);
    if (r.fecha && issue.state === 'open') await github.rest.issues.update({ ...alvo, state: 'closed', state_reason: r.fecha });
    if (r.tranca && !issue.locked) await github.rest.issues.lock({ ...alvo, lock_reason: 'spam' });
    return `resposta ${nome}`;
  }

  if (!ehQueda) return 'não é queda';

  if (eventName === 'issues' && ['opened', 'edited', 'reopened'].includes(payload.action)) {
    // Quem cuida do repositório pode reabrir à mão sem o arquivo; só o autor reabrindo passa pela regra.
    if (payload.action === 'reopened' && payload.sender && issue.user && payload.sender.login !== issue.user.login) return 'reaberta à mão';
    if (temCrashTxt(issue.body)) {
      if (issue.state === 'closed' && rotulos.includes('needs-crash-txt')) {
        await github.rest.issues.removeLabel({ ...alvo, name: 'needs-crash-txt' });
        await github.rest.issues.update({ ...alvo, state: 'open' });
        await github.rest.issues.createComment({ ...alvo, body: REABERTA });
        return 'reaberta';
      }
      return 'com crash.txt';
    }
    if (issue.state === 'open') {
      await github.rest.issues.addLabels({ ...alvo, labels: ['needs-crash-txt'] });
      await comentar(github, alvo, 'sem-crash', SEM_CRASH);
      await github.rest.issues.update({ ...alvo, state: 'closed', state_reason: 'not_planned' });
      return 'fechada sem crash.txt';
    }
    return 'fechada, ainda sem crash.txt';
  }

  if (eventName === 'issue_comment' && payload.action === 'created') {
    const doAutor = payload.comment && issue.user && payload.comment.user && payload.comment.user.login === issue.user.login;
    if (doAutor && issue.state === 'closed' && rotulos.includes('needs-crash-txt') && temCrashTxt(payload.comment.body)) {
      await github.rest.issues.removeLabel({ ...alvo, name: 'needs-crash-txt' });
      await github.rest.issues.update({ ...alvo, state: 'open' });
      await github.rest.issues.createComment({ ...alvo, body: REABERTA });
      return 'reaberta';
    }
    return 'comentário sem efeito';
  }
  return 'sem ação';
};

module.exports.temCrashTxt = temCrashTxt;
module.exports.RESPOSTAS = RESPOSTAS;
