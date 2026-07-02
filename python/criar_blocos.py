#!/usr/bin/env python3
"""
Cria biblioteca de blocos elétricos ABNT NBR 5444 para BricsCAD.

Simbologia de tomadas (triangular):
  Triângulo VAZIO   → altura baixa  (≤ 0,40 m)      bloco base  (TUG, TUE…)
  Triângulo HACHURA → altura média  (~1,10 m bancada) bloco + M  (TUGM, TUEM…)
  Triângulo SÓLIDO  → altura alta   (~1,50 m)         bloco + A  (TUGA, TUEA…)

Simbologia de interruptores (círculo-eixo + braço):
  Círculo VAZIO   → baixo   (IS, ID…)
  Círculo HACHURA → médio   (ISM, IDM…)
  Círculo SÓLIDO  → alto    (ISA, IDA…)
"""
import ezdxf
from ezdxf.enums import TextEntityAlignment
import math
import os

R = 0.15   # raio base em metros — bom para escalas 1:50 / 1:100

# ── Helpers de geometria ──────────────────────────────────────────────────────

TRI = [(-R, -R * 0.2), (R, -R * 0.2), (0, R * 0.9)]  # triângulo de tomada


def _circ_pts(cx, cy, r, n=32):
    """Aproximação poligonal de um círculo (para HATCH)."""
    return [(cx + r * math.cos(i * 2 * math.pi / n),
             cy + r * math.sin(i * 2 * math.pi / n)) for i in range(n)]


def hatch_solid(block, vertices):
    h = block.add_hatch()
    h.paths.add_polyline_path(vertices, is_closed=True)


def hatch_padrao(block, vertices, scale=None):
    h = block.add_hatch()
    h.set_pattern_fill("ANSI31", scale=scale or R * 0.18, angle=45)
    h.paths.add_polyline_path(vertices, is_closed=True)


def txt(block, text, height=None, y=0, x=0):
    h = height or R * 0.55
    t = block.add_text(text, dxfattribs={"height": h})
    t.set_placement((x, y), align=TextEntityAlignment.MIDDLE_CENTER)


# ── Atributos padrão (CIRCUITO visível; DESC, CARGA, TENSAO invisíveis) ──────

def atts(block, desc, carga_va=0, tensao=127):
    oy = -(R * 2.1)
    block.add_attdef("CIRCUITO", insert=(0, oy),
        dxfattribs={"height": R*0.55, "prompt": "Circuito:", "text": "C-?", "flags": 0})
    block.add_attdef("DESC", insert=(0, oy - R*0.75),
        dxfattribs={"height": R*0.4, "prompt": "Descrição:", "text": desc, "flags": 1})
    block.add_attdef("CARGA", insert=(0, oy - R*1.35),
        dxfattribs={"height": R*0.4, "prompt": "Carga (VA):", "text": str(carga_va), "flags": 1})
    block.add_attdef("TENSAO", insert=(0, oy - R*1.95),
        dxfattribs={"height": R*0.4, "prompt": "Tensão (V):", "text": str(tensao), "flags": 1})


# ── TOMADAS ──────────────────────────────────────────────────────────────────

def _tom_pinos_2p(block):
    """Pinos 2P+T saindo da base do triângulo."""
    block.add_line((-R*0.27, -R*0.2), (-R*0.27, -R*0.78))   # fase
    block.add_line(( R*0.27, -R*0.2), ( R*0.27, -R*0.78))   # neutro
    block.add_line((-R*0.44, -R*0.60), (R*0.44, -R*0.60))   # terra (PE)


def _tom_pinos_3p(block):
    """Pinos 3P+T saindo da base do triângulo."""
    block.add_line((-R*0.37, -R*0.2), (-R*0.37, -R*0.78))
    block.add_line(( 0,      -R*0.2), ( 0,      -R*0.78))
    block.add_line(( R*0.37, -R*0.2), ( R*0.37, -R*0.78))
    block.add_line((-R*0.5,  -R*0.60), (R*0.5,  -R*0.60))   # terra (PE)


def _tom_triangulo(block, modo="vazio"):
    block.add_lwpolyline(TRI, close=True)
    if modo == "hachura":
        hatch_padrao(block, TRI)
    elif modo == "solido":
        hatch_solid(block, TRI)


# TUG – Tomada de Uso Geral 2P+T 10A
def criar_tug(doc):
    b = doc.blocks.new("TUG")
    _tom_triangulo(b, "vazio");  _tom_pinos_2p(b)
    atts(b, "Tomada de Uso Geral 2P+T 10A 127/220V", carga_va=100)

def criar_tugm(doc):
    b = doc.blocks.new("TUGM")
    _tom_triangulo(b, "hachura"); _tom_pinos_2p(b)
    atts(b, "Tomada de Uso Geral 2P+T 10A 127/220V (Bancada)", carga_va=100)

def criar_tuga(doc):
    b = doc.blocks.new("TUGA")
    _tom_triangulo(b, "solido");  _tom_pinos_2p(b)
    atts(b, "Tomada de Uso Geral Alta 2P+T 10A 127/220V", carga_va=100)


# TUE – Tomada de Uso Específico 2P+T 20A 220V
def criar_tue(doc):
    b = doc.blocks.new("TUE")
    _tom_triangulo(b, "vazio");  _tom_pinos_2p(b)
    txt(b, "E", R*0.38, R*0.4)
    atts(b, "Tomada de Uso Específico 2P+T 20A 220V", carga_va=600, tensao=220)

def criar_tuem(doc):
    b = doc.blocks.new("TUEM")
    _tom_triangulo(b, "hachura"); _tom_pinos_2p(b)
    txt(b, "E", R*0.38, R*0.4)
    atts(b, "Tomada de Uso Específico 2P+T 20A 220V (Bancada)", carga_va=600, tensao=220)

def criar_tuea(doc):
    b = doc.blocks.new("TUEA")
    _tom_triangulo(b, "solido");  _tom_pinos_2p(b)
    txt(b, "E", R*0.38, R*0.4)
    atts(b, "Tomada de Uso Específico Alta 2P+T 20A 220V", carga_va=600, tensao=220)


# TUF – Tomada Trifásica 3P+T 20A 220V (uma variante, sempre fixo)
def criar_tuf(doc):
    b = doc.blocks.new("TUF")
    _tom_triangulo(b, "vazio"); _tom_pinos_3p(b)
    atts(b, "Tomada Trifásica 3P+T 20A 220V", carga_va=1000, tensao=220)


# ── INTERRUPTORES ─────────────────────────────────────────────────────────────

def _int_eixo(block, modo="vazio"):
    """Círculo de eixo com preenchimento variável."""
    r_circ = R * 0.18
    block.add_circle((-R, 0), r_circ)
    if modo == "hachura":
        hatch_padrao(block, _circ_pts(-R, 0, r_circ), scale=r_circ*0.4)
    elif modo == "solido":
        hatch_solid(block, _circ_pts(-R, 0, r_circ))
    block.add_line((-R, 0), (R, 0))   # linha de trilho


def _int_braco1(block):
    """Interruptor simples — 1 braço diagonal."""
    block.add_line((-R*0.1, 0), (-R*0.1 + R*0.65, R*0.65))

def _int_braco2(block):
    """Interruptor duplo — 2 braços diagonais."""
    block.add_line((-R*0.3, 0), (-R*0.3 + R*0.55, R*0.55))
    block.add_line(( R*0.1, 0), ( R*0.1 + R*0.55, R*0.55))

def _int_bracoY(block):
    """Interruptor paralelo — 2 braços em Y (two-way)."""
    block.add_line((-R*0.1, 0), (-R*0.1 + R*0.6,  R*0.6))
    block.add_line((-R*0.1, 0), (-R*0.1 + R*0.6, -R*0.6))


# IS – Interruptor Simples
def criar_is(doc):
    b = doc.blocks.new("IS")
    _int_eixo(b, "vazio"); _int_braco1(b)
    atts(b, "Interruptor Simples 10A 250V", carga_va=0)

def criar_ism(doc):
    b = doc.blocks.new("ISM")
    _int_eixo(b, "hachura"); _int_braco1(b)
    atts(b, "Interruptor Simples 10A 250V (Médio)", carga_va=0)

def criar_isa(doc):
    b = doc.blocks.new("ISA")
    _int_eixo(b, "solido"); _int_braco1(b)
    atts(b, "Interruptor Simples Alto 10A 250V", carga_va=0)


# ID – Interruptor Duplo
def criar_id(doc):
    b = doc.blocks.new("ID")
    _int_eixo(b, "vazio"); _int_braco2(b)
    atts(b, "Interruptor Duplo 10A 250V", carga_va=0)

def criar_idm(doc):
    b = doc.blocks.new("IDM")
    _int_eixo(b, "hachura"); _int_braco2(b)
    atts(b, "Interruptor Duplo 10A 250V (Médio)", carga_va=0)

def criar_ida(doc):
    b = doc.blocks.new("IDA")
    _int_eixo(b, "solido"); _int_braco2(b)
    atts(b, "Interruptor Duplo Alto 10A 250V", carga_va=0)


# IP – Interruptor Paralelo (three-way)
def criar_ip(doc):
    b = doc.blocks.new("IP")
    _int_eixo(b, "vazio"); _int_bracoY(b)
    atts(b, "Interruptor Paralelo (Three-way) 10A 250V", carga_va=0)


# IT – Interruptor Temporizado
def criar_it(doc):
    b = doc.blocks.new("IT")
    _int_eixo(b, "vazio"); _int_braco1(b)
    txt(b, "T", R*0.35, R*0.55)
    atts(b, "Interruptor Temporizado 10A 250V", carga_va=0)


# IPULSE – Pulsador (círculo na ponta = não trava)
def criar_ipulse(doc):
    b = doc.blocks.new("IPULSE")
    _int_eixo(b, "vazio"); _int_braco1(b)
    px, py = -R*0.1 + R*0.65, R*0.65
    b.add_circle((px, py), R*0.13)
    txt(b, "PU", R*0.34, R*0.82)
    atts(b, "Interruptor Pulsador de Campainha 10A 250V", carga_va=0)


# ── PONTOS DE LUZ ─────────────────────────────────────────────────────────────

def criar_plt(doc):
    """Ponto de luz de teto: círculo com cruz (⊕)."""
    b = doc.blocks.new("PLT")
    b.add_circle((0, 0), R)
    b.add_line((-R, 0), (R, 0))
    b.add_line((0, -R), (0, R))
    atts(b, "Ponto de Luz de Teto", carga_va=100)

def criar_plp(doc):
    """Ponto de luz de parede: semicírculo com cruz."""
    b = doc.blocks.new("PLP")
    pts = [(R*math.cos(a*math.pi/180), R*math.sin(a*math.pi/180)) for a in range(0, 181, 10)]
    b.add_lwpolyline(pts, close=False)
    b.add_line((-R, 0), (R, 0))
    b.add_line((0, 0), (0, R))
    atts(b, "Ponto de Luz de Parede", carga_va=100)

def criar_le(doc):
    """Luminária embutida: retângulo com X."""
    b = doc.blocks.new("LE")
    hw, hh = R*1.3, R*0.45
    b.add_lwpolyline([(-hw,-hh),(hw,-hh),(hw,hh),(-hw,hh)], close=True)
    b.add_line((-hw,-hh),(hw,hh)); b.add_line((-hw,hh),(hw,-hh))
    atts(b, "Luminária Embutida (Painel LED/Fluorescente)", carga_va=150)

def criar_ls(doc):
    """Luminária de sobrepor: retângulo com linha central."""
    b = doc.blocks.new("LS")
    hw, hh = R*1.3, R*0.45
    b.add_lwpolyline([(-hw,-hh),(hw,-hh),(hw,hh),(-hw,hh)], close=True)
    b.add_line((-hw, 0), (hw, 0))
    atts(b, "Luminária de Sobrepor", carga_va=150)

def criar_lex(doc):
    """Luminária externa / arandela: círculo com linha e EX."""
    b = doc.blocks.new("LEX")
    b.add_circle((0, 0), R)
    b.add_line((-R, 0), (R, 0))
    txt(b, "EX", R*0.42)
    atts(b, "Luminária Externa / Arandela", carga_va=150)

def criar_apl(doc):
    """Aplique de parede: semicírculo + base + haste."""
    b = doc.blocks.new("APL")
    pts = [(R*math.cos(a*math.pi/180), R*math.sin(a*math.pi/180)) for a in range(0, 181, 15)]
    b.add_lwpolyline(pts, close=False)
    b.add_line((-R, 0), (R, 0))
    b.add_line((0, 0), (0, R*0.65))
    atts(b, "Aplique de Parede (Balizador/Tartaruga)", carga_va=100)


# ── QUADROS ──────────────────────────────────────────────────────────────────

def criar_qd(doc):
    b = doc.blocks.new("QD")
    hw, hh = R*1.6, R*1.1
    b.add_lwpolyline([(-hw,-hh),(hw,-hh),(hw,hh),(-hw,hh)], close=True)
    txt(b, "QD", R*0.65)
    atts(b, "Quadro de Distribuição", carga_va=0)

def criar_qg(doc):
    b = doc.blocks.new("QG")
    hw, hh = R*1.6, R*1.1
    b.add_lwpolyline([(-hw,-hh),(hw,-hh),(hw,hh),(-hw,hh)], close=True)
    b.add_lwpolyline([(-hw+R*0.12,-hh+R*0.12),(hw-R*0.12,-hh+R*0.12),
                      (hw-R*0.12,hh-R*0.12),(-hw+R*0.12,hh-R*0.12)], close=True)
    txt(b, "QG", R*0.65)
    atts(b, "Quadro Geral de Força e Luz", carga_va=0)


# ── PONTOS ESPECIAIS ──────────────────────────────────────────────────────────

def criar_ptv(doc):
    b = doc.blocks.new("PTV")
    b.add_circle((0, 0), R); txt(b, "TV", R*0.5)
    atts(b, "Ponto de TV a Cabo / Antena", carga_va=0)

def criar_ptel(doc):
    b = doc.blocks.new("PTEL")
    b.add_circle((0, 0), R); txt(b, "TE", R*0.5)
    atts(b, "Ponto de Telefone / Lógica RJ45", carga_va=0)

def criar_pint(doc):
    b = doc.blocks.new("PINT")
    b.add_circle((0, 0), R); txt(b, "IF", R*0.5)
    atts(b, "Ponto de Interfone", carga_va=0)

def criar_cam(doc):
    """Campainha: semicírculo (sino) + base."""
    b = doc.blocks.new("CAM")
    pts = [(R*math.cos(a*math.pi/180), R*math.sin(a*math.pi/180)) for a in range(0, 181, 15)]
    b.add_lwpolyline(pts, close=False)
    b.add_line((-R, 0), (R, 0))
    b.add_line((-R*0.2, -R*0.3), (R*0.2, -R*0.3))  # badalo
    atts(b, "Campainha / Botoeira", carga_va=10)

def criar_dfum(doc):
    """Detector de fumaça: dois círculos concêntricos + FU."""
    b = doc.blocks.new("DFUM")
    b.add_circle((0, 0), R); b.add_circle((0, 0), R*0.5)
    txt(b, "FU", R*0.38)
    atts(b, "Detector de Fumaça", carga_va=10)

def criar_spda(doc):
    """SPDA: haste + raios + esfera de base."""
    b = doc.blocks.new("SPDA")
    b.add_line((0, -R), (0, R))
    b.add_line((-R*0.55, R*0.3), (0, R))
    b.add_line(( R*0.55, R*0.3), (0, R))
    b.add_circle((0, -R), R*0.2)
    atts(b, "Haste do SPDA (Para-raios)", carga_va=0)


# ── MAIN ─────────────────────────────────────────────────────────────────────

CRIADORES = [
    # Tomadas (triângulo: vazio/hachura/sólido por altura)
    criar_tug, criar_tugm, criar_tuga,
    criar_tue, criar_tuem, criar_tuea,
    criar_tuf,
    # Interruptores (círculo-eixo: vazio/hachura/sólido por altura)
    criar_is,  criar_ism,  criar_isa,
    criar_id,  criar_idm,  criar_ida,
    criar_ip,
    criar_it,
    criar_ipulse,
    # Iluminação
    criar_plt, criar_plp, criar_le, criar_ls, criar_lex, criar_apl,
    # Quadros
    criar_qd, criar_qg,
    # Especiais
    criar_ptv, criar_ptel, criar_pint, criar_cam, criar_dfum, criar_spda,
]


def criar_biblioteca(output_path):
    doc = ezdxf.new("R2010")
    doc.header["$INSUNITS"] = 6   # metros
    for fn in CRIADORES:
        fn(doc)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    doc.saveas(output_path)
    nomes = [f.__name__.replace("criar_", "").upper() for f in CRIADORES]
    print(f"Biblioteca criada: {output_path}")
    print(f"Total de blocos: {len(CRIADORES)}  →  {', '.join(nomes)}")


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output = os.path.normpath(os.path.join(script_dir, "..", "blocos", "eletrica_blocos.dxf"))
    criar_biblioteca(output)
