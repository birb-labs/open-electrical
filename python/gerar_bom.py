#!/usr/bin/env python3
"""
Gera Relação de Materiais (BOM) a partir de arquivo DXF.
Birb Labs - Plugin Elétrico para BricsCAD

Uso:
  python3 gerar_bom.py <arquivo.dxf> [saida.csv]
"""
import ezdxf
import csv
import os
import sys
from collections import defaultdict
from datetime import datetime

COMPONENTES = {
    "TUG":  ("Tomada de Uso Geral 2P+T 10A 127/220V",           "un.", "Tomadas"),
    "TUGA": ("Tomada de Uso Geral Alta 2P+T 10A 127/220V",      "un.", "Tomadas"),
    "TUE":  ("Tomada de Uso Específico 2P+T 20A 220V",          "un.", "Tomadas"),
    "TUF":  ("Tomada Trifásica 3P+T 20A 220V",                  "un.", "Tomadas"),
    "IS":   ("Interruptor Simples 10A 250V",                     "un.", "Interruptores"),
    "ID":   ("Interruptor Duplo 10A 250V",                       "un.", "Interruptores"),
    "IP":   ("Interruptor Paralelo (Three-way) 10A 250V",        "un.", "Interruptores"),
    "IT":   ("Interruptor Temporizado 10A 250V",                 "un.", "Interruptores"),
    "PLT":  ("Ponto de Luz de Teto",                             "pt.", "Iluminação"),
    "PLP":  ("Ponto de Luz de Parede",                           "pt.", "Iluminação"),
    "LE":   ("Luminária Embutida (Painel LED/Fluorescente)",     "un.", "Iluminação"),
    "LS":   ("Luminária de Sobrepor",                            "un.", "Iluminação"),
    "LEX":  ("Luminária Externa / Arandela",                     "un.", "Iluminação"),
    "APL":  ("Aplique de Parede (Balizador/Tartaruga)",          "un.", "Iluminação"),
    "QD":   ("Quadro de Distribuição",                           "un.", "Quadros"),
    "QG":   ("Quadro Geral de Força e Luz",                      "un.", "Quadros"),
    "PTV":  ("Ponto de TV a Cabo / Antena",                      "pt.", "Dados e TV"),
    "PTEL": ("Ponto de Telefone / Lógica RJ45/RJ11",             "pt.", "Dados e TV"),
    "PINT": ("Ponto de Interfone",                               "pt.", "Comunicação"),
    "CAM":  ("Campainha / Botoeira",                             "un.", "Comunicação"),
    "DFUM": ("Detector de Fumaça",                               "un.", "SPCI"),
    "SPDA": ("Haste do SPDA (Para-raios)",                       "un.", "SPDA"),
}

# Ordem desejada das categorias no relatório
ORDEM_CAT = ["Tomadas", "Interruptores", "Iluminação", "Quadros",
             "Dados e TV", "Comunicação", "SPCI", "SPDA"]


def ler_circuito(entity):
    try:
        for att in entity.attribs:
            if att.dxf.tag.upper() == "CIRCUITO":
                val = att.dxf.text.strip()
                return val if val else "Sem Circuito"
    except Exception:
        pass
    return "Sem Circuito"


def gerar_bom(dxf_path, output_path=None):
    if not os.path.exists(dxf_path):
        print(f"Erro: arquivo não encontrado: {dxf_path}")
        return False

    print(f"Lendo: {dxf_path}")

    try:
        doc = ezdxf.readfile(dxf_path)
    except Exception as e:
        print(f"Erro ao ler DXF: {e}")
        return False

    contagem = defaultdict(int)
    por_circuito = defaultdict(lambda: defaultdict(int))

    # Percorre todas as layouts (model + paperspaces)
    for layout in doc.layouts:
        for entity in layout:
            if entity.dxftype() == "INSERT":
                nome = entity.dxf.name.upper()
                if nome in COMPONENTES:
                    contagem[nome] += 1
                    circ = ler_circuito(entity)
                    por_circuito[circ][nome] += 1

    if not contagem:
        print("Nenhum componente elétrico encontrado.")
        return False

    if not output_path:
        base = os.path.splitext(dxf_path)[0]
        output_path = base + "_relacao_materiais.csv"

    # Ordena itens por categoria e depois por sigla
    cat_order = {c: i for i, c in enumerate(ORDEM_CAT)}
    sorted_items = sorted(
        contagem.items(),
        key=lambda x: (cat_order.get(COMPONENTES[x[0]][2], 99), x[0])
    )

    with open(output_path, "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f, delimiter=";")

        w.writerow(["RELAÇÃO DE MATERIAIS - INSTALAÇÕES ELÉTRICAS"])
        w.writerow([f"Arquivo: {os.path.basename(dxf_path)}"])
        w.writerow([f"Gerado em: {datetime.now().strftime('%d/%m/%Y %H:%M')}"])
        w.writerow([])
        w.writerow(["ITEM", "SIGLA", "DESCRIÇÃO", "UNID.", "QTDE.", "CATEGORIA"])

        current_cat = None
        item_num = 1
        for nome, qtde in sorted_items:
            desc, unid, cat = COMPONENTES[nome]
            if cat != current_cat:
                w.writerow([])
                w.writerow(["", "", f"[ {cat.upper()} ]", "", "", ""])
                current_cat = cat
            w.writerow([item_num, nome, desc, unid, qtde, cat])
            item_num += 1

        # Resumo por circuito (quando há mais de um)
        if len(por_circuito) > 1:
            w.writerow([])
            w.writerow([])
            w.writerow(["RESUMO POR CIRCUITO"])
            w.writerow(["CIRCUITO", "SIGLA", "DESCRIÇÃO", "QTDE."])
            for circ in sorted(por_circuito.keys()):
                for nome, qtde in sorted(por_circuito[circ].items()):
                    w.writerow([circ, nome, COMPONENTES[nome][0], qtde])

    # Console output
    print(f"\n{'─'*55}")
    print("  RELAÇÃO DE MATERIAIS")
    print(f"{'─'*55}")
    current_cat = None
    total = 0
    for nome, qtde in sorted_items:
        desc, unid, cat = COMPONENTES[nome]
        if cat != current_cat:
            print(f"\n  ── {cat} ──")
            current_cat = cat
        print(f"  {nome:<6}  {qtde:>4} {unid:<5}  {desc}")
        total += qtde
    print(f"\n{'─'*55}")
    print(f"  TOTAL: {total} pontos/componentes")
    print(f"\n  Arquivo: {output_path}")

    return True


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python3 gerar_bom.py <arquivo.dxf> [saida.csv]")
        sys.exit(1)

    dxf_file = sys.argv[1]
    out_file = sys.argv[2] if len(sys.argv) > 2 else None
    sys.exit(0 if gerar_bom(dxf_file, out_file) else 1)
