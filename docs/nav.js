/* open-electrical docs — sidebar compartilhada.
 *
 * Única fonte da navegação: cada página tem apenas <aside data-nav></aside> e
 * este script injeta o conteúdo, marca a página ativa (aria-current), expande
 * os sub-itens (âncoras) da página atual com um scrollspy simples e acrescenta
 * o carimbo de build ao rodapé. Sem dependências externas — funciona em file://.
 */
(function () {
  "use strict";

  // Carimbo exibido no rodapé de todas as páginas. Atualize ao regenerar a doc.
  var DOC_STAMP = { date: "2026-07-12", symbolVersion: 11, commands: 26 };

  var NAV = [
    { group: null, items: [
      { href: "index.html", label: "Início" },
      { href: "primeiro-projeto.html", label: "Guia: primeiro projeto" },
    ]},
    { group: "Fundamentos", items: [
      { href: "fundamentos.html", label: "Fundamentos", subs: [
        ["#requisitos", "Requisitos"],
        ["#estrutura", "Estrutura do projeto"],
        ["#tecnologias", "Tecnologias"],
        ["#logica", "Lógica geral"],
        ["#testes", "Testes de unidade"],
      ]},
    ]},
    { group: "Comandos", items: [
      { href: "elementos.html", label: "Elementos & símbolos", subs: [
        ["#el-room", "EL-ROOM"],
        ["#el-light", "EL-LIGHT"],
        ["#el-outlet", "EL-OUTLET(-AUTO)"],
        ["#el-switch", "EL-SWITCH(-AUTO)"],
        ["#el-panel", "EL-PANEL"],
        ["#el-edit", "EL-EDIT"],
        ["#simbolos", "SymbolFactory"],
        ["#galeria", "Galeria NBR 5444"],
      ]},
      { href: "calculo.html", label: "Cálculo & circuitos", subs: [
        ["#el-calc-light", "EL-CALC-LIGHT"],
        ["#el-circuit", "EL-CIRCUIT(-AUTO)"],
      ]},
      { href: "roteamento.html", label: "Eletrodutos & roteamento", subs: [
        ["#el-conduit-auto", "EL-CONDUIT-AUTO"],
        ["#el-conduit", "EL-CONDUIT"],
        ["#el-conduit-edit", "EL-CONDUIT-EDIT"],
        ["#el-route-auto", "EL-ROUTE-AUTO"],
        ["#outline", "ElementOutline"],
      ]},
      { href: "el-wire.html", label: "Fiação (EL-WIRE)", subs: [
        ["#fluxo", "Fluxo do comando"],
        ["#condutores", "Contagem de condutores"],
        ["#baloes", "Anatomia do balão"],
        ["#simbologias", "Formato das simbologias"],
        ["#modificar", "Como modificar"],
        ["#flip", "EL-WIRE-FLIP"],
      ]},
      { href: "relatorios.html", label: "Relatórios & remoção", subs: [
        ["#el-report", "EL-REPORT"],
        ["#el-del", "EL-DEL(-ROOM)"],
        ["#el-undo", "EL-UNDO"],
      ]},
    ]},
    { group: "Interface", items: [
      { href: "interface.html", label: "Paleta, config & i18n", subs: [
        ["#el", "EL (paleta)"],
        ["#el-config", "EL-CONFIG"],
        ["#i18n", "i18n"],
        ["#temas", "Aparência"],
        ["#tabela", "Referência rápida"],
      ]},
    ]},
    { group: "Ajuda", items: [
      { href: "troubleshooting.html", label: "Problemas comuns" },
    ]},
  ];

  function currentPage() {
    var p = location.pathname.split("/").pop();
    return p && p.length ? p : "index.html";
  }

  function build() {
    var aside = document.querySelector("aside[data-nav]");
    if (!aside) return;
    var page = currentPage();

    var html = [
      '<a href="index.html"><img class="brand-logo" src="assets/open-electrical.svg" alt="open-electrical"></a>',
      '<div class="tag">Plugin elétrico para BricsCAD · BRX / C++</div>',
      "<nav>",
    ];
    NAV.forEach(function (g) {
      if (g.group) html.push('<div class="group">' + g.group + "</div>");
      g.items.forEach(function (it) {
        var active = it.href === page;
        html.push(
          '<a href="' + it.href + '"' +
          (active ? ' class="active" aria-current="page"' : "") + ">" +
          it.label + "</a>"
        );
        if (active && it.subs) {
          it.subs.forEach(function (s) {
            html.push('<a class="sub" data-spy="' + s[0] + '" href="' + s[0] + '">' + s[1] + "</a>");
          });
        }
      });
    });
    html.push("</nav>");
    aside.innerHTML = html.join("\n");

    stampFooter();
    scrollSpy();
  }

  function stampFooter() {
    var f = document.querySelector("main footer");
    if (!f) return;
    var s = document.createElement("span");
    s.className = "stamp";
    s.textContent = "Documenta a build de " + DOC_STAMP.date +
      " · kSymbolVersion " + DOC_STAMP.symbolVersion +
      " · " + DOC_STAMP.commands + " comandos registrados";
    f.appendChild(s);
  }

  function scrollSpy() {
    var links = Array.prototype.slice.call(document.querySelectorAll("aside a.sub[data-spy]"));
    if (!links.length || !("IntersectionObserver" in window)) return;
    var byId = {};
    links.forEach(function (l) { byId[l.getAttribute("data-spy").slice(1)] = l; });

    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (!e.isIntersecting) return;
        links.forEach(function (l) { l.classList.remove("current"); });
        var l = byId[e.target.id];
        if (l) l.classList.add("current");
      });
    }, { rootMargin: "0px 0px -70% 0px" });

    Object.keys(byId).forEach(function (id) {
      var el = document.getElementById(id);
      if (el) observer.observe(el);
    });
  }

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", build);
  else
    build();
})();
