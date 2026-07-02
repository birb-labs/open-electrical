#!/usr/bin/env python3
"""
Motor de dimensionamento de circuitos elétricos — ABNT NBR 5410:2004.

Lê arquivo DXF exportado pelo BricsCAD e calcula:
  • Carga por circuito (VA)
  • Corrente de projeto (A)
  • Bitola do condutor de fase (mm²)
  • Disjuntor de proteção (A)
  • Queda de tensão (%)
  • Diâmetro mínimo do eletroduto (mm)

Uso:
  python3 dimensionar.py <projeto.dxf> [saida.txt]
"""
import ezdxf
import os
import sys
import math
import csv
from collections import defaultdict
from datetime import datetime

# ── Tabelas NBR 5410 ─────────────────────────────────────────────────────────

# Tabela 36: capacidade de corrente (A) para condutor cobre XLPE,
# instalação embutida em eletroduto (método B2), 2 condutores ativos, 30 °C
BITOLA_CORRENTE = [
    (1.5,  15.5),
    (2.5,  21.0),
    (4.0,  28.0),
    (6.0,  36.0),
    (10.0, 50.0),
    (16.0, 68.0),
    (25.0, 89.0),
    (35.0, 110.0),
    (50.0, 134.0),
]

# Disjuntores comerciais disponíveis (A)
DISJUNTORES = [6, 10, 16, 20, 25, 32, 40, 50, 63, 80, 100]

# Área total do condutor com isolação XLPE (mm²) por bitola — para cálculo de eletroduto
AREA_COND = {
    1.5: 8.0, 2.5: 11.3, 4.0: 15.9, 6.0: 20.4,
    10.0: 29.5, 16.0: 41.2, 25.0: 57.7, 35.0: 74.0, 50.0: 95.0,
}

# Área interna de eletrodutos de PVC/aço (mm²) por diâmetro nominal (mm)
AREA_TUBO = [(20, 201), (25, 314), (32, 804), (40, 1257), (50, 1963), (63, 3117)]

# Carga mínima padrão por tipo de bloco (VA) — quando CARGA=0 no atributo
CARGA_PADRAO = {
    "TUG": 100, "TUGM": 100, "TUGA": 100,
    "TUE": 600, "TUEM": 600, "TUEA": 600,
    "TUF": 1000,
    "PLT": 100, "PLP": 100,
    "LE": 150, "LS": 150, "LEX": 150, "APL": 100,
    "CAM": 10, "DFUM": 10,
}

BLOCOS_ELETRICOS = set(CARGA_PADRAO.keys()) | {
    "IS","ISM","ISA","ID","IDM","IDA","IP","IT","IPULSE",
    "QD","QG","PTV","PTEL","PINT","SPDA",
}

# Camadas de eletroduto
LAYERS_TUBO = {"EL-ELETRODUTO-E", "EL-ELETRODUTO-A", "el-eletroduto-e", "el-eletroduto-a"}

# ── Funções de cálculo ───────────────────────────────────────────────────────

def bitola_minima(corrente: float) -> float:
    """Retorna a menor bitola (mm²) que suporta a corrente dada."""
    for bitola, i_max in BITOLA_CORRENTE:
        if corrente <= i_max:
            return bitola
    return 50.0  # acima da tabela — sinalizar ao usuário

def disjuntor_minimo(corrente: float) -> int:
    """Retorna o disjuntor padrão imediatamente acima da corrente."""
    for d in DISJUNTORES:
        if d >= corrente:
            return d
    return 100

def queda_tensao_pct(corrente: float, comprimento: float, bitola: float, tensao: int, fp=0.92) -> float:
    """
    ΔV% = (2 × ρ × L × I × cosφ) / (A × V) × 100
    ρ_Cu = 0.0175 Ω·mm²/m
    """
    if bitola <= 0 or comprimento <= 0:
        return 0.0
    rho = 0.0175
    return (2 * rho * comprimento * corrente * fp) / (bitola * tensao) * 100

def eletroduto_minimo(bitola: float, n_condutores: int) -> int:
    """
    Retorna o diâmetro nominal mínimo (mm) do eletroduto.
    Taxa de ocupação ≤ 40% (NBR 5410 Tabela J.52-1).
    n_condutores inclui fase + neutro + PE.
    """
    area_cond = AREA_COND.get(bitola, 30.0)
    area_total = area_cond * n_condutores
    area_min_tubo = area_total / 0.40
    for diam, area in AREA_TUBO:
        if area >= area_min_tubo:
            return diam
    return 63

# ── Leitura do DXF ──────────────────────────────────────────────────────────

def _get_att(entity, tag: str) -> str:
    try:
        for a in entity.attribs:
            if a.dxf.tag.upper() == tag.upper():
                return a.dxf.text.strip()
    except Exception:
        pass
    return ""

def _comprimento_lwpoly(entity) -> float:
    try:
        pts = list(entity.get_points())
        total = 0.0
        for i in range(len(pts) - 1):
            dx = pts[i+1][0] - pts[i][0]
            dy = pts[i+1][1] - pts[i][1]
            total += math.hypot(dx, dy)
        return total
    except Exception:
        return 0.0

def ler_dxf(dxf_path: str):
    """
    Retorna:
      circuitos: dict[str → {pontos, carga_va, tensao, tipo}]
      tubos:     dict[str → comprimento_metros]
    """
    doc = ezdxf.readfile(dxf_path)

    circuitos = defaultdict(lambda: {"pontos": [], "carga_va": 0, "tensao": 127, "tipo": "misto"})
    tubos = defaultdict(float)   # layer → comprimento total

    for layout in doc.layouts:
        for ent in layout:
            dtype = ent.dxftype()

            if dtype == "INSERT":
                nome = ent.dxf.name.upper()
                if nome not in BLOCOS_ELETRICOS:
                    continue
                circ = _get_att(ent, "CIRCUITO") or "Sem Circuito"
                carga_str = _get_att(ent, "CARGA")
                tensao_str = _get_att(ent, "TENSAO")
                try:
                    carga = int(carga_str)
                except (ValueError, TypeError):
                    carga = 0
                if carga == 0:
                    carga = CARGA_PADRAO.get(nome, 0)
                try:
                    tensao = int(tensao_str)
                except (ValueError, TypeError):
                    tensao = 127

                circuitos[circ]["pontos"].append(nome)
                circuitos[circ]["carga_va"] += carga
                circuitos[circ]["tensao"] = tensao
                # Tipo do circuito (iluminação prevalece sobre tomadas)
                if nome in {"PLT","PLP","LE","LS","LEX","APL"}:
                    if circuitos[circ]["tipo"] == "misto":
                        circuitos[circ]["tipo"] = "iluminacao"
                elif nome.startswith("TU"):
                    circuitos[circ]["tipo"] = "tomadas"

            elif dtype in ("LWPOLYLINE", "POLYLINE"):
                layer = ent.dxf.layer if hasattr(ent.dxf, "layer") else ""
                if layer in LAYERS_TUBO:
                    tubos[layer] += _comprimento_lwpoly(ent)

    return dict(circuitos), dict(tubos)

# ── Dimensionamento ──────────────────────────────────────────────────────────

def dimensionar(circuitos: dict, tubos: dict, fp=0.92):
    resultados = []
    for circ_nome in sorted(circuitos):
        c = circuitos[circ_nome]
        carga = c["carga_va"]
        tensao = c.get("tensao", 127)
        tipo = c.get("tipo", "misto")

        # Corrente de projeto
        corrente = carga / (tensao * fp) if tensao > 0 else 0

        # Bitola mínima NBR 5410 (iluminação ≥ 1,5 mm², tomadas ≥ 2,5 mm²)
        bitola = bitola_minima(corrente)
        if tipo == "iluminacao" and bitola < 1.5:
            bitola = 1.5
        elif tipo in ("tomadas", "misto") and bitola < 2.5:
            bitola = 2.5

        # Disjuntor
        disj = disjuntor_minimo(corrente)

        # Comprimento de eletroduto do circuito (soma de todos os trechos)
        comp = sum(tubos.values())   # simplificado: soma todos os trechos do projeto

        # Queda de tensão
        dv = queda_tensao_pct(corrente, comp, bitola, tensao, fp)

        # Eletroduto: fase + neutro + PE = 3 condutores
        n_cond = 3
        diam = eletroduto_minimo(bitola, n_cond)

        resultados.append({
            "circuito": circ_nome,
            "tipo": tipo,
            "pontos": len(c["pontos"]),
            "carga_va": carga,
            "tensao_v": tensao,
            "corrente_a": round(corrente, 2),
            "bitola_mm2": bitola,
            "disjuntor_a": disj,
            "comp_m": round(comp, 1),
            "queda_pct": round(dv, 2),
            "eletroduto_mm": diam,
            "alerta": ("QUEDA ALTA" if dv > 4.0 else ""),
        })
    return resultados

# ── Saída ────────────────────────────────────────────────────────────────────

def imprimir(resultados, tubos):
    sep = "─" * 72
    print(f"\n{sep}")
    print("  DIMENSIONAMENTO DE CIRCUITOS — ABNT NBR 5410:2004")
    print(f"{sep}")

    for r in resultados:
        alert = f"  ⚠ {r['alerta']}" if r['alerta'] else ""
        print(f"\n  Circuito : {r['circuito']}  ({r['tipo']}){alert}")
        print(f"  Pontos   : {r['pontos']}  |  Carga: {r['carga_va']} VA  |  "
              f"Tensão: {r['tensao_v']} V")
        print(f"  Corrente : {r['corrente_a']:.2f} A")
        print(f"  Condutor : {r['bitola_mm2']} mm²  |  Disjuntor: {r['disjuntor_a']} A")
        if r['comp_m'] > 0:
            print(f"  Eletroduto: ∅ {r['eletroduto_mm']} mm  |  "
                  f"Queda de tensão: {r['queda_pct']:.2f}%")

    if tubos:
        print(f"\n{sep}")
        print("  ELETRODUTOS")
        for layer, comp in tubos.items():
            print(f"  {layer:30s}: {comp:.1f} m")

    print(f"\n{sep}")
    print(f"  Total de circuitos: {len(resultados)}")
    print(f"{sep}\n")


def salvar_csv(resultados, output_path):
    with open(output_path, "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f, delimiter=";")
        w.writerow(["DIMENSIONAMENTO DE CIRCUITOS — ABNT NBR 5410:2004"])
        w.writerow([f"Gerado em: {datetime.now().strftime('%d/%m/%Y %H:%M')}"])
        w.writerow([])
        campos = ["CIRCUITO","TIPO","PONTOS","CARGA (VA)","TENSÃO (V)",
                  "CORRENTE (A)","BITOLA (mm²)","DISJUNTOR (A)",
                  "COMP. TUBO (m)","QUEDA ΔV (%)","ELETRODUTO (mm)","ALERTA"]
        w.writerow(campos)
        for r in resultados:
            w.writerow([
                r["circuito"], r["tipo"], r["pontos"], r["carga_va"],
                r["tensao_v"], r["corrente_a"], r["bitola_mm2"],
                r["disjuntor_a"], r["comp_m"], r["queda_pct"],
                r["eletroduto_mm"], r["alerta"],
            ])
    print(f"  CSV salvo em: {output_path}")


# ── CLI ──────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python3 dimensionar.py <projeto.dxf> [saida.csv]")
        sys.exit(1)

    dxf_file = sys.argv[1]
    if not os.path.exists(dxf_file):
        print(f"Arquivo não encontrado: {dxf_file}")
        sys.exit(1)

    print(f"Lendo: {dxf_file}")
    circuitos, tubos = ler_dxf(dxf_file)

    if not circuitos:
        print("Nenhum componente elétrico encontrado.")
        sys.exit(1)

    resultados = dimensionar(circuitos, tubos)
    imprimir(resultados, tubos)

    base = os.path.splitext(dxf_file)[0]
    csv_out = sys.argv[2] if len(sys.argv) > 2 else base + "_dimensionamento.csv"
    salvar_csv(resultados, csv_out)
