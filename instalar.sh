#!/usr/bin/env bash
# instalar.sh - Plugin Eletrico Birb Labs para BricsCAD v26
# Uso: ./instalar.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUPPORT="$HOME/Bricsys/BricsCAD/V26x64/en_US/Support"

echo "+--------------------------------------------------+"
echo "|  Plugin Eletrico - Birb Labs  (instalacao)      |"
echo "+--------------------------------------------------+"
echo ""

# -- Verificar ezdxf --------------------------------------------------------
echo ">> Verificando dependencias..."
if ! python3 -c "import ezdxf" 2>/dev/null; then
    echo "   Instalando ezdxf..."
    pip3 install --user ezdxf
fi
echo "   ezdxf OK"
echo ""

# -- Gerar biblioteca de blocos ---------------------------------------------
echo ">> Gerando blocos eletricos ABNT NBR 5444..."
mkdir -p "$SCRIPT_DIR/blocos"
python3 "$SCRIPT_DIR/python/criar_blocos.py"
echo ""

# -- Limpar e copiar arquivos para o Support path --------------------------
echo ">> Instalando no BricsCAD ($SUPPORT)..."
mkdir -p "$SUPPORT"

# Converte LSP/DCL para ASCII puro antes de copiar (BricsCAD exige ASCII)
python3 - "$SCRIPT_DIR" "$SUPPORT" <<'PYEOF'
import sys, os

REPLACEMENTS = {
    'e': ['e', 'e', 'e', 'e', 'e'],  # dummy, replaced below
}
REPLACE = {
    'é':'e','è':'e','ê':'e','ë':'e','É':'E',
    'á':'a','à':'a','â':'a','ã':'a','Á':'A','Ã':'A',
    'ó':'o','ô':'o','õ':'o','Ó':'O',
    'ú':'u','ù':'u','û':'u','Ú':'U',
    'í':'i','ì':'i','î':'i','Í':'I',
    'ç':'c','Ç':'C',
    '—':'--','–':'-','’':"'",'‘':"'",'“':'"','”':'"',
    '≤':'<=','≥':'>=','≠':'!=',
    '─':'-','│':'|','┐':'+','┌':'+','└':'+','┘':'+',
    '├':'+','┤':'+','┬':'+','┴':'+','┼':'+',
    '═':'=','║':'|','╔':'+','╗':'+','╚':'+','╝':'+',
    '╠':'+','╣':'+','╦':'+','╩':'+','╬':'+',
}

src_dir, dst_dir = sys.argv[1], sys.argv[2]
for fname in ['eletrica.lsp', 'eletrica_panel.dcl']:
    src = os.path.join(src_dir, 'lisp', fname)
    if not os.path.exists(src):
        print(f"   AVISO: {fname} nao encontrado em lisp/")
        continue
    text = open(src, 'r', encoding='utf-8').read()
    for orig, rep in REPLACE.items():
        text = text.replace(orig, rep)
    remaining = [c for c in text if ord(c) > 127]
    if remaining:
        print(f"   AVISO: {fname} ainda tem {len(remaining)} chars nao-ASCII apos conversao!")
    else:
        dst = os.path.join(dst_dir, fname)
        open(dst, 'w', encoding='ascii').write(text)
        print(f"   {fname}: ASCII OK -> copiado")
PYEOF

cp "$SCRIPT_DIR/blocos/eletrica_blocos.dxf" "$SUPPORT/eletrica_blocos.dxf"
echo "   eletrica_blocos.dxf -> OK"
echo ""

# -- Configurar carregamento automatico ------------------------------------
echo ">> Configurando carregamento automatico..."

LOAD_LINE='(if (findfile "eletrica.lsp") (load "eletrica.lsp"))'

for LSP_FILE in "$SUPPORT/on_start.lsp" "$SUPPORT/on_doc_load.lsp"; do
    if [ -f "$LSP_FILE" ]; then
        if ! grep -q "eletrica.lsp" "$LSP_FILE"; then
            echo "" >> "$LSP_FILE"
            echo "$LOAD_LINE" >> "$LSP_FILE"
            echo "   $(basename $LSP_FILE): entrada adicionada"
        else
            echo "   $(basename $LSP_FILE): ja configurado"
        fi
    else
        echo "$LOAD_LINE" > "$LSP_FILE"
        echo "   $(basename $LSP_FILE): criado"
    fi
done
echo ""

echo "+--------------------------------------------------+"
echo "|  Instalacao concluida!                           |"
echo "+--------------------------------------------------+"
echo ""
echo "No BricsCAD, abra um desenho e use:"
echo "  EL-PAINEL  -- abre o painel de insercao"
echo "  EL-SETUP   -- configura camadas + abre painel"
echo "  EL-BOM     -- gera relacao de materiais (CSV)"
echo ""
echo "Para recarregar sem reiniciar o BricsCAD:"
echo "  (load \"eletrica.lsp\")"
echo ""
echo "BOM standalone:"
echo "  python3 \"$SCRIPT_DIR/python/gerar_bom.py\" projeto.dxf"
