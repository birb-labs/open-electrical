# Handoff — open-electrical (2026-07-10 · sessão distribuição de fiação)

## Sessão distribuição de fiação (mudanças UNSTAGED, tudo por cima do abaixo)
Build limpo · **31/31 testes** (+3) · instalado em `~/.local/share/open-electrical/`.

### Tarefa 1 — Outline da tomada honra as pontas arredondadas
- `src/utilities/ElementOutline.cpp`: novo `appendRoundedTriangle()` amostra o
  contorno com os fillets (`kOutletFilletR = 0.2*kOutletBW`), espelhando
  `SymbolFactory::addOutletTriangle` — base sempre arredondada; ápice arredondado
  só no último módulo (junções ficam sharp). `segments(Kind::Outlet)` usa isso.
  Ponto de conexão do eletroduto agora cai na curva real, não no vértice afiado.

### Tarefa 2 — Distribuição gráfica de fiação (estende EL-WIRE)
Decisão do usuário: contar **por circuito** que trafega no eletroduto (nº de
componentes NÃO multiplica; nº de circuitos distintos sim); R branco com **máscara
preta**; three-way = **2 R**; **estender EL-WIRE** (sem comando novo).
- `src/services/WireCounts.h` (novo, header-only, testável): `ConductorSet` +
  `conductorsFor(CircuitType)` — ilum.=1F+1N+1R; tomada=1F+1N+1PE; motor=F+N+PE;
  mista=F+N+PE+R.
- `src/models/Conduit.{h,cpp}`: campos `circuitIds` e `annotationHandles`
  (serializados).
- `src/services/Router.{h,cpp}`: `route()` carimba `circuitIds` no conduíte durante
  o roteamento; `conduitCircuits()`/`conduitConductors()` (soma sobre circuitos
  distintos, com fallback pelas pontas); `pullWires()` reescrito (1 jogo por
  circuito, bitola do circuito).
- `src/commands/CommandUtil.{h,cpp}`: `drawColoredPolyline()` (RGB) e `drawLabel()`
  (texto RGB centralizado + AcDbSolid de máscara preta opcional).
- `src/commands/CircuitCommands.cpp`: `drawWiringBalloon()` (aro "C" inclinado →
  haste → reta branca → F vermelho / N ciano / PE verde / R branco+máscara),
  `refreshWiringBalloons()` (idempotente: apaga handles antigos e redesenha).
  `routeConduit` deriva `circuitIds` das pontas; `EL-WIRE` e `EL-ROUTE-AUTO`
  desenham/atualizam os balões.
- Testes: `outline_outlet_apex_is_rounded`, `wire_counts_per_circuit_type`,
  `wire_counts_sum_across_shared_circuits`.

### Validar no BricsCAD (sem GUI aqui) / limitações
- Rodar EL-CIRCUIT-AUTO → EL-CONDUIT-AUTO → **EL-WIRE**; conferir balões no meio de
  cada eletroduto, cores e o R legível sobre a reta branca.
- Aro levemente inclinado: se o "C" abrir para o lado errado, inverter o `lean`/a
  ordem de amostragem em `drawWiringBalloon`.
- Reta/aro brancos assumem fundo escuro (padrão model space); só o R foi mascarado
  (decisão do usuário).
- Trechos "tronco" com 2+ circuitos: só aparecem quando um conduíte manual liga
  componentes de circuitos diferentes — o auto-roteador ainda desenha 1 polilinha
  por circuito (não funde trechos coincidentes). `EL-DEL` de um conduíte não apaga
  os balões órfãos (limpar via layer `EL_WIRING`) — follow-up.

---

# Handoff — open-electrical (2026-07-10)

Branch `main`. **Nada commitado** (aguardando autorização do usuário).
Testes **28/28** · build limpo · módulo **instalado** em
`~/.local/share/open-electrical/open-electrical.lrx` (última build 14:22).
`SymbolFactory::kSymbolVersion = 11`.

---

## Como buildar / instalar / recarregar (IMPORTANTE)
- Build+install: `bash scripts/setup.sh` (Release, Ninja). Instala o `.lrx` +
  `resources/` em `~/.local/share/open-electrical/` (var `INSTALL_DIR`).
  O `install()` do CMake aponta pra `/usr/local` — **não** é o caminho real; use o
  `setup.sh`.
- Testes BRX-free: `cmake --build build --target check` (target `oe_tests`).
- **Após rebuild, reinstalar** e **fechar/reabrir o BricsCAD** (ele faz cache do
  `.lrx` por processo). Blocos antigos se redefinem sozinhos pelo carimbo de
  versão (`SymbolFactory::ensure` compara `kSymbolVersion`); rode `REGEN`.
- ⚠️ Se `build/` foi configurado com outro gerador, `setup.sh` falha por
  mismatch (Ninja vs Makefiles) → `rm -rf build` e rode de novo.
- ⚠️ **GateGuard**: o ambiente tem um hook que exige "fact-forcing" antes de cada
  primeiro edit/bash por arquivo e antes de comandos destrutivos (ex.: `rm -rf`).
  Some com `ECC_GATEGUARD=off` ou `ECC_DISABLED_HOOKS=pre:edit-write:gateguard-fact-force,pre:bash:gateguard-fact-force`.

---

## O que foi feito nesta sessão (todas as mudanças estão UNSTAGED)

### A. Tomadas — pontas arredondadas + preenchimento + conectividade
Arquivo: `src/services/SymbolFactory.cpp` (v8 → **v11**).
- Pontas do triângulo levemente arredondadas via **fillet por canto**
  (`computeRoundCorners`/`addRoundedTriangleOutline`, `RoundCorner{r,...}`).
- Preenchimento (média/alta) segue o **mesmo contorno arredondado**
  (`appendArcSamples`+`fillConvex`), não mais `AcDbSolid` afiado.
- **Bug corrigido**: em cadeia, o ápice de um módulo é a junção com o próximo →
  fica **sharp** (raio 0); só a ponta livre do topo é arredondada
  (`addOutletTriangle(..., bool roundApex)`; base sempre arredondada).
  `buildOutletChain` arredonda só o último; `buildOutletWithSwitch` mantém todos
  os ápices sharp (a gravata do interruptor conecta no ponto).

### B. Eletroduto azul exclusivo (sessão anterior, já dentro do build)
`src/commands/CommandUtil.cpp` `ensurePluginLayer`: `EL_CONDUIT` = **5 (azul)**,
`EL_TEXT` movido pra **7 (branco)**. Azul é exclusivo do conduíte.

### C. Regra de conexão por componente (outline) — **novo módulo**
Arquivos novos: `src/utilities/ElementOutline.{h,cpp}` (BRX-free, no `oe_tests` via
`CMakeLists.txt`).
- `outline::Kind{Light,Outlet,Switch,Panel}`, `segments(kind,modules)`,
  `closestOnSegments`, `toLocal/toWorld(origin,rot)`, `nearestAttachPoint(...)`.
- Shapes espelham a simbologia: Light=círculo `kLightR`; Switch=círculo 1 seção +
  gravata; Panel=retângulo 6·kSwR × 2·kSwR (3:1); Outlet=cadeia de N triângulos de
  tomada baixa (N do nome do bloco).
- Constantes duplicadas de `SymbolFactory` (kSymUnit=0.09 etc.) — **manter em
  sincronia** se as dimensões dos símbolos mudarem.

### D. EL-CONDUIT reescrito + EL-CONDUIT-EDIT — `src/commands/CircuitCommands.cpp`
- **EL-CONDUIT**: seleciona origem+destino (block refs); calcula pontos de conexão
  **mútuos** nos outlines (parte do ponto da origem mais próx. do destino → ponto
  do destino mais próx. da origem), linha **reta**. Helpers: `resolveAttach`
  (posição+rotação+kind+módulos via `blockRefName`/`classifyOutline`),
  `mutualAttachPoints`. Guarda `srcHandle`/`dstHandle` no `Conduit`.
- **EL-CONDUIT-EDIT** (novo): pick do conduíte → keyword
  `[Source/Destination/Curve]`:
  - Source/Destination: novo ponto **encaixado no outline** do componente ligado.
  - Curve: ponto por onde a curva passa → calcula `bulge` (arco) e redesenha.
  - Atualiza `path`/`bulge`/comprimento e salva → **reflete no BOM**.

### E. Modelo Conduit — `src/models/Conduit.{h,cpp}`
- Novos campos `srcHandle`, `dstHandle`, `bulge` (serializados).
- `recomputeLength` agora mede **arco** quando `bulge≠0` (path de 2 pontos).

### F. Helpers de desenho — `src/commands/CommandUtil.{h,cpp}`
- `drawArc2D(layer,a,b,bulge)` (AcDbPolyline com bulge no vértice 0).
- `blockRefName(id,out)` (lê o nome do bloco de um block reference).

### G. Remoção — `src/commands/DeleteCommands.cpp` (**novo**, auto-globbed)
- **EL-DEL**: `acedSSGet` (pickset) → apaga entidades selecionadas do desenho **e**
  do modelo (`elements`/`conduits` por handle) → some do BOM.
- **EL-DEL-ROOM**: lista de cômodos via `ui::showRoomPicker` (novo em
  `UiBridge.{h,cpp}`, `wxSingleChoiceDialog`) → remove **só o boundary**
  (`boundaryHandle`); componentes ficam, `roomId` → -1.

### H. Registro + UI
- `src/commands/Commands.{h,cpp}`: declarados/registrados **EL-CONDUIT-EDIT**,
  **EL-DEL** (`USEPICKSET`), **EL-DEL-ROOM**.
- `src/ui/MainPalette.cpp`: botões **"Edit conduit"**, **"Delete"**, **"Delete
  room"** (seção nova no palette `EL`).

### Testes — `tests/tests.cpp` (+`CMakeLists.txt`)
4 casos novos de outline (círculo/retângulo/rotação/empilhamento de módulos).
Total **28/28**.

---

## Pendências / validar no BricsCAD (não dá pra testar sem GUI aqui)
1. `EL-CONDUIT`: confirmar que os endpoints caem no contorno de cada tipo e a reta
   não invade os símbolos, em vários ângulos de rotação.
2. `EL-CONDUIT-EDIT` → Curve: sentido/half do bulge (o sinal do sagitta segue a
   normal esquerda `(-dy,dx)`; se curvar pro lado errado, inverter o sinal em
   `c->bulge`).
3. `EL-DEL` com seleção mista (componente + eletroduto) e conferir BOM (`EL-REPORT`).
4. `EL-DEL-ROOM`: diálogo aparece, boundary some, componentes permanecem.
5. Tomadas média/alta encadeadas: módulos conectados, pontas arredondadas,
   preenchimento acompanhando o contorno.

## Decisões do usuário (para não reabrir)
- Edição de eletroduto = **por comando** (EL-CONDUIT-EDIT), **não** grips ao vivo.
- Traçado do EL-CONDUIT = **linha reta** entre os pontos mais próximos.
- EL-DEL-ROOM remove **só o cômodo** (mantém componentes).

## Limitações conhecidas / próximos passos possíveis
- O outline da tomada usa triângulos "cheios" (ignora o arredondamento das pontas)
  para o ponto de conexão — diferença submilimétrica.
- Se quiser **arrastar** de verdade (grips ao vivo com limites): exigiria
  `AcDbGripOverrule` sobre polylines da layer `EL_CONDUIT` lendo `srcHandle`/
  `dstHandle` (bem maior; foi descartado nesta sessão a favor do comando).
- `EL-CONDUIT-EDIT` só edita runs retos de 2 pontos (os manuais); os auto-roteados
  (multi-segmento) não são editáveis por ele.
- Nada foi commitado. Sugerido: commitar A–H como um conjunto coeso.
