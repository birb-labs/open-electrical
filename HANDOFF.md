# Handoff — site de documentação (open-electrical)

**Data:** 2026-07-11 · **Estado:** site estático inicial pronto, sem commit.

## O que foi feito
Gerado um **site estático de documentação** (HTML/CSS puro, sem build) em `docs/`.
Abrir com o navegador: `docs/index.html`.

### Estrutura de `docs/`
```
docs/
├── index.html         # Hub inicial (cards + logo hero)
├── fundamentos.html   # Requisitos · Estrutura · Tecnologias · Lógica geral (âncoras #requisitos, #estrutura, #tecnologias, #logica)
├── el-wire.html       # Mergulho: EL-WIRE / EL-WIRE-AUTO + customização dos balões
├── style.css          # Tema escuro, sidebar, code blocks, .glyph-demo
└── assets/            # logo.svg (símbolo), text.svg (nome), open-electrical.svg (completa)
```

### Logos
Copiadas de `~/Pictures/Projetos/Birb Labs/open-electrical/` para `docs/assets/`.
Em uso: `open-electrical.svg` na sidebar (`.brand-logo`) e no hero do hub (`.hero-logo`).
`logo.svg` e `text.svg` ainda **não** foram usadas — disponíveis para favicon/variações.

### Limpeza pedida
Os `.md` que estavam em `docs/` foram movidos para **`docs-legacy/`**
(ARCHITECTURE, BRICSCAD-BRX-LINUX, HANDOFF antigo, RELATORIO-CORRECOES). `docs/` ficou só com o site.

## Conteúdo por página (o que já está escrito)
- **Fundamentos:** requisitos (BRX SDK v26, BricsCAD V26, wxWidgets 3.2.8 estático, C++17,
  CMake+Ninja), como compilar/instalar (`scripts/setup.sh`), aviso do cache de `.lrx`,
  árvore de pastas, camadas (commands→services→models/utilities), tecnologias, e a lógica
  geral (tabela de comandos, `ProjectData`, persistência via `ProjectContext`).
- **EL-WIRE:** fluxo (`routeWireAuto` → `pullWires` + `refreshWiringBalloons`), diagnóstico
  impresso, trunk-union/idempotência, contagem de condutores (`WireCounts.h` +
  `Router::conduitConductors`), anatomia do balão (aro/haste/trilho, métricas `ringR/tH/barW/hc/sp`),
  formato de cada simbologia com **demos SVG** (Fase/Neutro/Terra/Retorno), tabela "como
  modificar", exemplo de edição, aviso `drawSolidRect` vs `setConstantWidth`, e `EL-WIRE-FLIP`.

## Continuação (sessão 2026-07-11) — documentação de comandos COMPLETA
Escritas 5 novas páginas cobrindo os **26 comandos** e a customização de cada camada. A nav
(sidebar) e os cards do hub foram atualizados em todas as páginas; todos os links internos
resolvem (verificado). Cada página termina com uma tabela "como customizar" → arquivo/função.

- **`elementos.html`** — EL-ROOM, EL-LIGHT, EL-OUTLET(-AUTO), EL-SWITCH(-AUTO), EL-PANEL, EL-EDIT;
  `SymbolFactory` (blocos NBR 5444) + **aviso do `kSymbolVersion`** (bump ao mudar geometria);
  `attrsFor`/`blockNameFor`/`layerFor`.
- **`calculo.html`** — EL-CALC-LIGHT/EL-LIGHT-AUTO (`LightingCalculator`, fórmula do método dos
  lúmens Φ=E·A/CU·FM) e EL-CIRCUIT(-AUTO) (`CircuitDistributor`, regras/tetos NBR 5410).
- **`roteamento.html`** — EL-CONDUIT(-AUTO/-EDIT), EL-ROUTE-AUTO; `Router` + `ElementOutline`
  (contornos de conexão por tipo, `nearestAttachPoint`, bulge do EL-CONDUIT-EDIT).
- **`relatorios.html`** — EL-REPORT/EL-LOAD-SCHEDULE/EL-SINGLE-LINE (`ReportGenerator`),
  EL-DEL/EL-DEL-ROOM, EL-UNDO.
- **`interface.html`** — EL (paleta wxWidgets/`MainPalette`/`PaletteHost`), EL-CONFIG
  (`ProjectSettings`), i18n (`Localization` + `resources/lang/*.json`), e **tabela de referência
  rápida de todos os comandos**.

### O que ainda falta (opcional)
- Modelo/persistência em detalhe (`ProjectContext`/`Serialization`) e paletas de cor wx.
- Favicon com `logo.svg`, busca no site, página individual por comando.

### Como continuar
Cada seção nova = 1 arquivo `.html` copiando o cabeçalho/sidebar de qualquer página (trocar o
link `active`) + adicionar a entrada no `<nav>` das outras páginas. O bloco `<nav>` já é
idêntico em todas — replicar tal e qual.

## Pendências não relacionadas ao site
- O código do plugin (EL-WIRE volumétrico com `drawSolidRect`) foi compilado/instalado na
  sessão anterior, mas **falta o usuário validar no BricsCAD** (fechar/reabrir por causa do
  cache de `.lrx`) e reportar a linha `EL-WIRE: N conduit(s)…`.
- Nada commitado ainda (site + mudanças de código anteriores continuam unstaged).
