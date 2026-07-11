# Relatório de correções — open-electrical

Data: 2026-07-09 · Branch: `main` · Build: limpo · Testes: 20/20 (74 checks) ·
Módulo: `build/open-electrical.lrx` (md5 `9fdd407…`).

> **Nada foi commitado.** Todas as alterações estão na árvore de trabalho,
> aguardando autorização.

---

## 1. Resumo das correções realizadas

### Item 1 — Quadro (QGBT) na proporção 3:1
- **Arquivos:** `src/services/SymbolFactory.cpp` (`buildPanel`), `SymbolFactory.h`.
- Retângulo do quadro alterado de 2:1 para **3:1** (`w = 3.0 * h`, mantendo a
  altura = diâmetro do interruptor). O triângulo retângulo interno preenchido
  (ângulo reto no canto da parede, hipotenusa na diagonal secundária) foi
  mantido.
- Atributos `NOME` e `CIRCUITOS` recentralizados no novo retângulo (x = 0,
  centralização horizontal — ver Item 2.1).
- `kSymbolVersion` incrementado para **v8** → `ensure()` redefine automaticamente
  os blocos antigos gravados no `.dwg`.

### Item 2.1 — Anotações dos pontos de luz centralizadas
- **Arquivos:** `src/services/SymbolFactory.cpp` (`buildLight`, `buildLightWall`,
  novo helper `addAttDefCentered`).
- Novo helper que cria a `AcDbAttributeDefinition` com
  `setHorizontalMode(kTextCenter)` + `setVerticalMode(kTextVertMid)` e
  `setAlignmentPoint()` no centro do setor. Assim "2x32W" e "a" ficam centrados
  no mesmo ponto, independentemente do comprimento do texto.
- `POTENCIA` (setor superior), `COMANDO` (inferior-esquerdo) e `CIRCUITO`
  (inferior-direito) reposicionados nos centros dos setores (raio ≈ 0,144).
  Idem para o arandela (`buildLightWall`) e para o quadro (`NOME`/`CIRCUITOS`).

### Item 2.2 — Altura do ponto de luz referida ao teto
- **Arquivos:** `src/models/LightPoint.h`/`.cpp`, `src/ui/ElementDialogs.cpp`
  (`LightDialog`), `src/commands/ElementCommands.cpp` (`insertLight`,
  `placeLightsInRoom`).
- Novo campo `LightPoint::offsetFromCeiling` (m, padrão **0.0**), serializado
  (`offsetFromCeiling`, com default 0.0 para desenhos antigos).
- Label do diálogo alterado para **"Height below ceiling (m)"**, valor padrão 0.0,
  gravando em `offsetFromCeiling` (clamp ≥ 0).
- No comando, a cota Z do bloco passa a ser
  `Z = (ceilingHeight − offsetFromCeiling)` (em unidades de desenho), buscando a
  altura do teto do cômodo sob o ponto (senão 2,8 m). `mountingHeight` (base,
  altura acima do piso) é derivado = `ceilingHeight − offsetFromCeiling`, o que
  também alimenta corretamente as descidas verticais do roteador.

### Item 3 — Layers por tipo de componente
- **Arquivos:** `src/commands/CommandUtil.cpp` (novo `ensurePluginLayer`,
  chamado em `insertSymbol`, `drawPolyline`, `drawPolyline3D`).
- **Causa-raiz:** `insertSymbol` já fazia `br->setLayer(nome)`, mas a layer nunca
  era criada antes; um nome de layer inexistente torna `setLayer` um *no-op*
  silencioso e a entidade fica na layer 0. Agora a layer é garantida (criada com
  a cor correta) imediatamente antes de `setLayer`.
- Convenção de cores: `EL_PANELS` 1 (vermelho), `EL_LIGHTING` 3 (verde),
  `EL_OUTLETS` 4 (ciano), `EL_SWITCHES` 6 (magenta), `EL_CONDUIT` 5 (azul),
  `EL_WIRING` 2 (amarelo), `EL_ROOMS` 8 (cinza), `EL_TEXT` 7 (branco). O azul
  (ACI 5) é **exclusivo** dos eletrodutos; o texto, antes azul, passou a branco
  para não compartilhar a cor do conduíte. A layer corrente do desenho **não** é
  alterada (define-se a layer por entidade).

### Item 4 — Cálculo luminotécnico inserindo os pontos
- **Arquivos:** `src/commands/ElementCommands.cpp` (`placeLightsInRoom`),
  `#include utilities/Diag.h`.
- Diagnóstico `EL_DIAG_LOG` antes/depois das inserções (ativável com
  `EL_DIAG=1`), aviso ao usuário quando o método dos lumens não produz posições,
  e **detecção de falha silenciosa**: se `insertSymbol` retornar handle vazio,
  o usuário é avisado (bloco não inserido) e a contagem reflete só o que foi de
  fato inserido.
- Como parte do Item 3, as luminárias auto agora caem na layer `EL_LIGHTING`
  (antes ficavam na layer 0 quando `EL_LIGHTING` não existia).

### Item 5 — Roteamento de conduítes com topologia correta
- **Arquivos:** `src/services/Router.cpp` (`route` reescrito),
  `#include models/Switch.h`.
- Elementos de cada circuito agora são **classificados por tipo**
  (LightPoint / Switch / Outlet / outros) e roteados assim:
  - **Iluminação:** quadro → luz₁ → luz₂ … (caminhada por proximidade), trechos
    ortogonais (`geom::orthogonalPath`, L de dois perpendiculares).
  - **Interruptores:** cada um é um **ramal** a partir da luz que ele comanda
    (`controlledLightHandles`), com *fallback* para a luz mais próxima e depois
    para o quadro.
  - **Tomadas:** quadro → tomada₁ → tomada₂ … em série.
  - Descidas verticais (teto → dispositivo na sua altura de montagem) para
    quadro e cada dispositivo; o modo "somente descidas" (`omitHorizontal`) foi
    preservado.

### Item 6 — Comandos duplicados
- **Arquivos:** `src/commands/Commands.cpp`, `Commands.h`,
  `src/commands/ElementCommands.cpp`.
- Removido **`EL-LIGHT-AUTO`** (`insertLightAuto`), que era alias exato de
  `EL-CALC-LIGHT` (ambos chamavam `runLightingCalc`). Nenhuma outra referência
  (paleta usa `EL-CIRCUIT-AUTO`, não este). Verificado por `grep`.

---

## 2. Avaliação geral frente às normas (NBR 5410 / 5413 / 5444)

Prioridade: **[C]rítico · [M]édio · [B]aixo**.

### 2.1 Faltantes / gaps
| Prio | Item | Situação atual | Sugestão |
|------|------|----------------|----------|
| C | **Queda de tensão (NBR 5410 6.2.7)** | `Circuit::voltageDropPct` existe mas **nunca é calculado** (fica 0). | Calcular ΔV% = f(comprimento, seção, corrente) por circuito e sinalizar > 4 % (iluminação) / limites de projeto; usar o comprimento total do conduíte já disponível no roteador. |
| C | **Dimensionamento por ampacidade** | Condutor definido só pelo **mínimo** (1,5 / 2,5 mm²) em `CircuitDistributor::minConductorMM2`. | Comparar com a corrente de projeto (I = VA/V) e as tabelas de capacidade de condução (NBR 5410 tab. 36–39), método de instalação e agrupamento; subir a seção quando necessário. |
| M | **Fatores de demanda (NBR 5410 anexo)** | `demandFactor` fica 1,0. | Aplicar fator de demanda por tipo/quantidade de circuitos ao dimensionar o alimentador/quadro. |
| M | **Coordenação disjuntor × condutor** | `breakerFor` mapeia seção → disjuntor, mas não valida In ≤ Iz. | Garantir Ib ≤ In ≤ Iz e proteção contra curto. |
| B | **Fator de potência** | Cargas tratadas como VA = W. | Permitir FP por carga para separar W/VA. |
| B | **DR / DPS** | Não modelados. | Sinalizar necessidade de DR (áreas molhadas) e DPS no quadro. |

### 2.2 Funções presentes — observações
- **Método dos lumens (NBR 5413):** correto e coberto por testes
  (`E·A/(CU·MF)`, `N = ceil`). CU por índice do local saturante e realista
  (regressões de superdimensionamento passam). Bom.
- **Símbolos (NBR 5444):** após v8, quadro 3:1, anotações centradas, tamanhos
  normalizados (`kSymUnit`). Ponto de luz, arandela, tomadas (baixa/média/alta),
  interruptores (1–3 seções, paralelo, intermediário, campainha) presentes.
- **Distribuição de circuitos:** separa iluminação/TUG/TUE, respeita tetos de VA
  e faz balanceamento de fases — adequado como base.

### 2.3 Inconsistências com as normas
| Prio | Item | Sugestão |
|------|------|----------|
| M | `EL-OUTLET-AUTO` usa critério simplificado (1 tomada / 5 m; molhadas 3,5 m) e não distingue quantidades mínimas por tipo de cômodo da NBR 5410 (ex.: cozinha/área de serviço acima de bancada, banheiro). | Refinar regras por `RoomUsage`. |
| B | Potência das tomadas: primeiras TUG a 100 VA e demais a 100 VA — conferir a regra "3×600 VA depois 100 VA" e mínimos por cômodo. | Ajustar em `Outlet::totalVA`/distribuidor. |

### 2.4 Comandos — duplicados/uso
- Removido `EL-LIGHT-AUTO` (feito).
- **A avaliar (não removidos):** `EL-CIRCUIT` (chama `distributeCircuitsAuto`) e
  `EL-WIRE` (chama `routeWireAuto`) são hoje quase-idênticos aos `-AUTO`. Mantidos
  por serem "pontos de entrada manuais" declarados; recomenda-se ou implementar o
  fluxo manual real ou consolidá-los.

---

## 3. Testes realizados e como validar no BricsCAD

**Automático (feito):** `cmake --build build --target check` → **20/20**;
`cmake --build build` → link limpo do `.lrx`. As mudanças no núcleo BRX-free
(PolyMath / LightingCalculator / serialização de Room) continuam verdes; o
`Router` e a UI não têm cobertura unitária (dependem do BRX) e foram validados
por compilação.

**Manual (a validar no BricsCAD, recarregando o `.lrx` — v8 força redefinição):**
1. **Item 1:** `EL-PANEL` → quadro deve aparecer 3× mais largo que alto, com o
   triângulo preenchido; `NOME`/`CIRCUITOS` centrados.
2. **Item 2.1:** `EL-LIGHT` com "2x32W" e comando "a" → textos centrados nos
   setores. Editar para texto mais longo e confirmar que permanecem centrados.
3. **Item 2.2:** `EL-LIGHT` → label "Height below ceiling (m)", padrão 0,00; com
   0,5 o bloco deve descer 0,5 m abaixo do teto (checar Z em vista 3D).
4. **Item 3:** inserir cada tipo e conferir que caem nas layers `EL_*` com as
   cores da convenção (não na layer 0).
5. **Item 4:** `EL-ROOM` (com parâmetros) → `EL-CALC-LIGHT` → luminárias devem
   ser inseridas na layer `EL_LIGHTING`; rodar com `EL_DIAG=1` para o log em
   `$HOME/oe-init.log` se algo não aparecer.
6. **Item 5:** montar quadro + 2 luzes + 1 interruptor + tomadas, `EL-CIRCUIT-AUTO`
   e `EL-CONDUIT-AUTO` → conferir quadro→luzes em série, ramal do interruptor à
   luz que ele comanda, tomadas em série, e descidas verticais.

---

## 4. Limitações conhecidas e próximos passos

- Itens **críticos 2.1** (queda de tensão e ampacidade) **não** foram
  implementados neste ciclo — são a próxima prioridade e exigem tabelas da
  NBR 5410.
- O roteador liga interruptor→luz pela associação `controlledLightHandles`; se o
  modelo não tiver essa associação preenchida, usa a luz mais próxima
  (heurística). `EL-SWITCH-AUTO` preenche essa associação; a inserção manual do
  interruptor ainda não vincula a luz explicitamente.
- Validação visual no BricsCAD ainda pendente (não executável neste ambiente).
- `Item 4`: o fluxo de inserção parece correto no código; as melhorias adicionam
  diagnóstico + detecção de falha silenciosa + criação de layer. Se persistir a
  não-inserção, o log `EL_DIAG=1` apontará a etapa.
