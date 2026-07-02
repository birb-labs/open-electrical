;;; eletrica_dim.lsp — Módulo de Dimensionamento e Cômodos
;;; Birb Labs | Plugin Elétrico BricsCAD — ABNT NBR 5410:2004
;;;
;;; Comandos:
;;;   EL-COMODO   — delimita área de um cômodo (polilinha fechada + rótulo)
;;;   EL-CIRCUITO — edita propriedades de um circuito (tensão, tipo)
;;;   EL-DIM      — dimensionamento automático dos circuitos
;;;   EL-TABELA   — insere tabela de dimensionamento no desenho
;;;   EL-MAPA     — exibe mapa textual de circuitos no prompt

;;; ── Camadas ────────────────────────────────────────────────────────────────

(defun eld:setup-camadas ( / )
  (foreach par '(
    ("EL-COMODO"    8  "Continuous")
    ("EL-COMODO-TX" 8  "Continuous")
    ("EL-DIM-TAB"   7  "Continuous")
  )
    (if (not (tblsearch "LAYER" (car par)))
      (entmake (list '(0 . "LAYER") '(100 . "AcDbSymbolTableRecord")
                     '(100 . "AcDbLayerTableRecord")
                     (cons 2 (car par)) '(70 . 0)
                     (cons 62 (cadr par)) (cons 6 (caddr par)))))
  )
)

;;; ── Tabelas NBR 5410 embutidas ─────────────────────────────────────────────

; (corrente-max-A . "bitola-mm2")
(setq *ELD-TAB-BITOLA*
  '((15.5 . "1,5") (21.0 . "2,5") (28.0 . "4,0")
    (36.0 . "6,0") (50.0 . "10") (68.0 . "16")
    (89.0 . "25")  (110.0 . "35") (134.0 . "50")))

; Disjuntores comerciais disponíveis
(setq *ELD-DISJ* '(6 10 16 20 25 32 40 50 63 80 100))

; Diâmetros de eletroduto disponíveis (mm)
(setq *ELD-TUBOS* '(20 25 32 40 50 63))

; Área do condutor com isolação XLPE (mm²) por bitola
(setq *ELD-AREA-COND*
  '(("1,5" . 8.0) ("2,5" . 11.3) ("4,0" . 15.9)
    ("6,0" . 20.4) ("10" . 29.5) ("16" . 41.2) ("25" . 57.7)))

; Área interna do eletroduto (mm²) por diâmetro (mm)
(setq *ELD-AREA-TUBO*
  '((20 . 201) (25 . 314) (32 . 804) (40 . 1257) (50 . 1963) (63 . 3117)))

; Carga padrão (VA) por bloco — usada quando atributo CARGA = 0
(setq *ELD-CARGA-PAD*
  '(("TUG" . 100) ("TUGM" . 100) ("TUGA" . 100)
    ("TUE" . 600) ("TUEM" . 600) ("TUEA" . 600) ("TUF" . 1000)
    ("PLT" . 100) ("PLP" . 100)
    ("LE" . 150)  ("LS" . 150)   ("LEX" . 150)  ("APL" . 100)
    ("CAM" . 10)  ("DFUM" . 10)))

;;; ── Funções de cálculo ─────────────────────────────────────────────────────

(defun eld:bitola (corrente / resultado)
  (setq resultado "50+")
  (foreach par *ELD-TAB-BITOLA*
    (if (and (<= corrente (car par)) (= resultado "50+"))
      (setq resultado (cdr par))))
  resultado)

(defun eld:disj (corrente / resultado encontrado)
  (setq resultado 100 encontrado nil)
  (foreach d *ELD-DISJ*
    (if (and (not encontrado) (>= d corrente))
      (progn (setq resultado d encontrado T))))
  resultado)

(defun eld:area-cond (bitola)
  (setq r (assoc bitola *ELD-AREA-COND*))
  (if r (cdr r) 30.0))

(defun eld:eletroduto (bitola n-cond / area-total area-min resultado encontrado)
  ; Taxa de ocupação máxima = 40% (NBR 5410 Tabela J.52-1)
  (setq area-total (* (eld:area-cond bitola) n-cond))
  (setq area-min (/ area-total 0.40))
  (setq resultado 63 encontrado nil)
  (foreach par *ELD-AREA-TUBO*
    (if (and (not encontrado) (>= (cdr par) area-min))
      (progn (setq resultado (car par) encontrado T))))
  resultado)

(defun eld:queda (corrente comp bitola tensao / rho)
  ; ΔV% = (2 × ρ × L × I × cosφ) / (A × V) × 100   cosφ = 0,92
  ; ρ_Cu = 0.0175 Ω·mm²/m
  (if (or (= bitola "50+") (<= comp 0) (<= tensao 0))
    0.0
    (progn
      (setq rho 0.0175)
      (setq area (eld:area-num bitola))
      (if (> area 0)
        (* 100.0 (/ (* 2.0 rho comp corrente 0.92) (* area tensao)))
        0.0))))

(defun eld:area-num (bitola)
  ; Converte string de bitola para número float
  (cond ((= bitola "1,5")  1.5)  ((= bitola "2,5")  2.5)
        ((= bitola "4,0")  4.0)  ((= bitola "6,0")  6.0)
        ((= bitola "10")  10.0)  ((= bitola "16")   16.0)
        ((= bitola "25")  25.0)  ((= bitola "35")   35.0)
        ((= bitola "50")  50.0)  (T 0.0)))

(defun eld:bitola-tipo (corrente tipo)
  ; Aplica bitola mínima por tipo de circuito
  (setq b (eld:bitola corrente))
  (cond
    ((= b "50+") b)  ; fora da tabela
    ((= tipo "IL")  ; iluminação: mín 1,5 mm²
     (if (< (eld:area-num b) 1.5) "1,5" b))
    (T  ; tomadas/misto: mín 2,5 mm²
     (if (< (eld:area-num b) 2.5) "2,5" b))))

;;; ── Carga padrão ───────────────────────────────────────────────────────────

(defun eld:carga-pad (bname / r)
  (setq r (assoc (strcase bname) *ELD-CARGA-PAD*))
  (if r (cdr r) 0))

;;; ── Leitura de blocos elétricos ────────────────────────────────────────────

(defun eld:get-att (ins-ent tag / ent d val)
  (setq val "" ent (entnext ins-ent))
  (while ent
    (setq d (entget ent))
    (cond
      ((= (cdr (assoc 0 d)) "ATTRIB")
       (if (= (strcase (cdr (assoc 2 d))) (strcase tag))
         (progn (setq val (cdr (assoc 1 d))) (setq ent nil))
         (setq ent (entnext ent))))
      ((= (cdr (assoc 0 d)) "SEQEND") (setq ent nil))
      (T (setq ent (entnext ent)))))
  val)

(defun eld:blocos-ok ()
  ; Wildcard para ssget
  "TUG,TUGM,TUGA,TUE,TUEM,TUEA,TUF,IS,ISM,ISA,ID,IDM,IDA,IP,IT,IPULSE,PLT,PLP,LE,LS,LEX,APL,QD,QG,PTV,PTEL,PINT,CAM,DFUM,SPDA")

(defun eld:tipo-bloco (bname)
  ; Retorna "IL" (iluminação) ou "TM" (tomadas/misto)
  (if (wcmatch (strcase bname) "PLT,PLP,LE,LS,LEX,APL")
    "IL" "TM"))

(defun eld:coletar-circuitos ( / ss i ename bname circ carga tensao tipo tab)
  ; Retorna lista de listas: ((circ tipo carga tensao n-pontos) ...)
  (setq tab '())
  (setq ss (ssget "X" (list '(0 . "INSERT") (cons 2 (eld:blocos-ok)))))
  (if ss
    (progn
      (setq i 0)
      (while (< i (sslength ss))
        (setq ename (ssname ss i))
        (setq bname (strcase (cdr (assoc 2 (entget ename)))))
        (setq circ   (eld:get-att ename "CIRCUITO"))
        (setq carga  (eld:get-att ename "CARGA"))
        (setq tensao (eld:get-att ename "TENSAO"))
        (if (= circ "")  (setq circ "C-?"))
        (if (= carga "")  (setq carga "0"))
        (if (= tensao "") (setq tensao "127"))
        (setq carga-n  (atoi carga))
        (setq tensao-n (atoi tensao))
        (if (= carga-n 0) (setq carga-n (eld:carga-pad bname)))
        (setq tipo (eld:tipo-bloco bname))

        ; Adicionar / atualizar entrada na tabela
        (setq tab (eld:tab-add tab circ tipo carga-n tensao-n))
        (setq i (1+ i)))))
  tab)

(defun eld:tab-add (tab circ tipo carga tensao / achou resultado)
  (setq achou nil resultado '())
  (foreach item tab
    (if (and (not achou) (= (car item) circ))
      (progn
        (setq resultado
          (append resultado
            (list (list circ
                        (car (cdr item))  ; tipo existente
                        (+ (caddr item) carga)
                        tensao
                        (1+ (nth 4 item))))))
        (setq achou T))
      (setq resultado (append resultado (list item)))))
  (if (not achou)
    (append resultado (list (list circ tipo carga tensao 1)))
    resultado))

;;; ── Comprimento dos eletrodutos ────────────────────────────────────────────

(defun eld:comp-eletrodutos ( / ss i ent d comp-total comp pts j dx dy)
  (setq comp-total 0.0)
  (setq ss (ssget "X" '((0 . "LWPOLYLINE"))))
  (if ss
    (progn
      (setq i 0)
      (while (< i (sslength ss))
        (setq ent (ssname ss i))
        (setq d (entget ent))
        (setq layer (cdr (assoc 8 d)))
        (if (or (= layer "EL-ELETRODUTO-E") (= layer "EL-ELETRODUTO-A"))
          (progn
            (setq comp (eld:comprimento-pline ent))
            (setq comp-total (+ comp-total comp))))
        (setq i (1+ i)))))
  comp-total)

(defun eld:comprimento-pline (ent / d pts j comp)
  (setq d (entget ent))
  (setq pts '() comp 0.0)
  (foreach item d
    (if (= (car item) 10)
      (setq pts (append pts (list (cdr item))))))
  (if (> (length pts) 1)
    (progn
      (setq j 0)
      (while (< j (1- (length pts)))
        (setq p1 (nth j pts))
        (setq p2 (nth (1+ j) pts))
        (setq dx (- (car p2) (car p1)))
        (setq dy (- (cadr p2) (cadr p1)))
        (setq comp (+ comp (sqrt (+ (* dx dx) (* dy dy)))))
        (setq j (1+ j)))))
  comp)

;;; ── Dimensionar ────────────────────────────────────────────────────────────

(defun eld:dimensionar (tab comp-total / resultados)
  (setq resultados '())
  (foreach item tab
    (setq circ   (nth 0 item))
    (setq tipo   (nth 1 item))
    (setq carga  (nth 2 item))
    (setq tensao (nth 3 item))
    (setq n-pts  (nth 4 item))
    (if (<= tensao 0) (setq tensao 127))
    (setq corrente (/ (float carga) (* tensao 0.92)))
    (setq bitola (eld:bitola-tipo corrente tipo))
    (setq disj   (eld:disj corrente))
    (setq tubo   (eld:eletroduto bitola 3))  ; 3 condutores: F + N + PE
    (setq dv     (eld:queda corrente comp-total bitola tensao))
    (setq alerta (if (> dv 4.0) "!QUEDA" ""))
    (setq resultados
      (append resultados
        (list (list circ tipo n-pts carga tensao
                    (fix (* corrente 100)) ; × 100 para evitar real no formato
                    bitola disj tubo
                    (fix (* dv 100))       ; × 100
                    alerta)))))
  resultados)

;;; ── EL-MAPA (exibe no prompt) ─────────────────────────────────────────────

(defun C:EL-MAPA ( / tab comp res)
  (princ "\nColetando dados do desenho...")
  (setq tab (eld:coletar-circuitos))
  (if (not tab)
    (progn (princ "\nNenhum componente elétrico encontrado.") (exit)))
  (setq comp (eld:comp-eletrodutos))
  (setq res (eld:dimensionar tab comp))

  (princ "\n")
  (princ "\n+--MAPA DE CIRCUITOS (ABNT NBR 5410)-----------------------------+")
  (princ (strcat "\n| " (eld:fmt-col "CIRCUITO" 10)
                        (eld:fmt-col "TIPO" 4)
                        (eld:fmt-col "PTS" 4)
                        (eld:fmt-col "CARGA" 7)
                        (eld:fmt-col "I(A)" 6)
                        (eld:fmt-col "BIT." 5)
                        (eld:fmt-col "DISJ" 5)
                        (eld:fmt-col "TUBO" 5)
                        (eld:fmt-col "ΔV%" 5)
                        " |"))
  (princ "\n+-----------------------------------------------------------------+")
  (foreach r res
    (setq i-str  (strcat (itoa (/ (nth 5 r) 100)) "," (eld:zfill (rem (nth 5 r) 100) 2)))
    (setq dv-str (strcat (itoa (/ (nth 9 r) 100)) "," (eld:zfill (rem (nth 9 r) 100) 2)))
    (setq alert  (nth 10 r))
    (princ (strcat "\n| " (eld:fmt-col (nth 0 r) 10)
                          (eld:fmt-col (nth 1 r) 4)
                          (eld:fmt-col (itoa (nth 2 r)) 4)
                          (eld:fmt-col (strcat (itoa (nth 3 r)) "VA") 7)
                          (eld:fmt-col i-str 6)
                          (eld:fmt-col (nth 6 r) 5)
                          (eld:fmt-col (itoa (nth 7 r)) 5)
                          (eld:fmt-col (strcat (itoa (nth 8 r)) "mm") 5)
                          (eld:fmt-col dv-str 5)
                          (if (= alert "") " |" (strcat " " alert " |"))))
  )
  (if (> comp 0)
    (princ (strcat "\nEletrodutos: " (rtos comp 2 1) " m (total)")))
  (princ "\n+-----------------------------------------------------------------+")
  (princ (strcat "\n  Total de circuitos: " (itoa (length res))))
  (princ "\n")
  (princ)
)

(defun eld:fmt-col (s n / pad)
  (setq s (if s s ""))
  (if (> (strlen s) n) (substr s 1 n)
    (progn
      (setq pad "")
      (repeat (- n (strlen s)) (setq pad (strcat pad " ")))
      (strcat s pad))))

(defun eld:zfill (n len / s pad)
  (setq s (itoa (abs n)))
  (while (< (strlen s) len) (setq s (strcat "0" s)))
  s)

;;; ── EL-DIM (exporta CSV e abre) ──────────────────────────────────────────

(defun C:EL-DIM ( / tab comp res csv-path f total-carga)
  (princ "\nDimensionamento ABNT NBR 5410 — coletando dados...")
  (setq tab (eld:coletar-circuitos))
  (if (not tab)
    (progn (alert "Nenhum componente elétrico encontrado.") (exit)))
  (setq comp (eld:comp-eletrodutos))
  (setq res (eld:dimensionar tab comp))

  ; Gera CSV
  (setq csv-path (strcat (getvar "DWGPREFIX") (getvar "DWGNAME") "_dim.csv"))
  (setq f (open csv-path "w"))
  (if (not f) (progn (alert "Erro ao criar arquivo CSV.") (exit)))

  (write-line "DIMENSIONAMENTO DE CIRCUITOS - ABNT NBR 5410:2004" f)
  (write-line "" f)
  (write-line "CIRCUITO;TIPO;PONTOS;CARGA(VA);TENSAO(V);CORRENTE(A);BITOLA(mm2);DISJUNTOR(A);ELETRODUTO(mm);QUEDA(%);ALERTA" f)

  (setq total-carga 0)
  (foreach r res
    (setq i-r (/ (float (nth 5 r)) 100.0))
    (setq dv-r (/ (float (nth 9 r)) 100.0))
    (write-line
      (strcat (nth 0 r) ";" (nth 1 r) ";" (itoa (nth 2 r)) ";"
              (itoa (nth 3 r)) ";" (itoa (nth 4 r)) ";"
              (rtos i-r 2 2) ";" (nth 6 r) ";"
              (itoa (nth 7 r)) ";" (itoa (nth 8 r)) ";"
              (rtos dv-r 2 2) ";" (nth 10 r))
      f)
    (setq total-carga (+ total-carga (nth 3 r))))

  (write-line "" f)
  (write-line (strcat "TOTAL;;;;" (itoa total-carga)) f)
  (close f)

  ; Exibe resumo no prompt
  (C:EL-MAPA)
  (princ (strcat "\nCSV salvo em: " csv-path))

  ; Abre o arquivo com o visualizador padrão (Linux/Mac)
  (if (findfile csv-path)
    (startapp "xdg-open" (strcat "\"" csv-path "\"")))

  (princ)
)

;;; ── EL-TABELA (insere tabela no desenho) ──────────────────────────────────

(defun C:EL-TABELA ( / tab comp res pt lo orig x y col-w row-h txt-h)
  (princ "\nColetando dados...")
  (setq tab (eld:coletar-circuitos))
  (if (not tab)
    (progn (alert "Nenhum componente elétrico encontrado.") (exit)))
  (setq comp (eld:comp-eletrodutos))
  (setq res (eld:dimensionar tab comp))

  (eld:setup-camadas)
  (setq lo (getvar "CLAYER"))
  (setvar "CLAYER" "EL-DIM-TAB")

  (princ "\nClique para posicionar a tabela de dimensionamento:")
  (setq pt (getpoint))
  (if (not pt) (progn (setvar "CLAYER" lo) (exit)))

  (setq orig   pt)
  (setq col-w  0.8)   ; largura de coluna (unidades do desenho)
  (setq row-h  0.5)   ; altura de linha
  (setq txt-h  0.18)  ; altura do texto
  (setq colunas '("CIRCUITO" "TIPO" "PTS" "CARGA" "I(A)" "BITOLA" "DISJ" "∅TUBO" "ΔV%"))
  (setq n-col (length colunas))
  (setq larg-total (* col-w n-col))

  ; Cabeçalho
  (eld:retangulo orig larg-total row-h)
  (eld:texto-cel orig colunas col-w row-h txt-h)
  (eld:linha-h-sep (list (car orig) (- (cadr orig) row-h)) larg-total)

  ; Linhas de dados
  (setq y-atual (- (cadr orig) row-h))
  (foreach r res
    (setq i-r  (rtos (/ (float (nth 5 r)) 100.0) 2 2))
    (setq dv-r (rtos (/ (float (nth 9 r)) 100.0) 2 2))
    (setq dados (list (nth 0 r) (nth 1 r) (itoa (nth 2 r))
                      (strcat (itoa (nth 3 r)) "VA")
                      i-r (nth 6 r) (itoa (nth 7 r))
                      (strcat (itoa (nth 8 r)) "mm") dv-r))
    (eld:retangulo (list (car orig) y-atual) larg-total (- row-h))
    (eld:texto-cel (list (car orig) y-atual) dados col-w row-h txt-h)
    (if (= (nth 10 r) "!QUEDA")
      (eld:texto (list (+ (car orig) larg-total 0.05) (- y-atual (* row-h 0.5)))
                 "!ΔV" txt-h))
    (setq y-atual (- y-atual row-h))
  )

  ; Rodapé
  (eld:linha-h-sep (list (car orig) y-atual) larg-total)
  (eld:texto
    (list (+ (car orig) 0.05) (- y-atual (* row-h 0.5)))
    (strcat "TABELA ABNT NBR 5410:2004 | " (menucmd "M=$(getvar,DATE)")) txt-h)

  (setvar "CLAYER" lo)
  (princ "\nTabela inserida.")
  (princ)
)

(defun eld:retangulo (org larg alt / p1 p2 p3 p4)
  (setq x0 (car org) y0 (cadr org))
  (setq p1 (list x0 y0) p2 (list (+ x0 larg) y0)
        p3 (list (+ x0 larg) (+ y0 alt)) p4 (list x0 (+ y0 alt)))
  (entmake (list '(0 . "LWPOLYLINE") '(100 . "AcDbEntity") '(100 . "AcDbPolyline")
                 '(90 . 4) '(70 . 1)
                 (cons 10 p1) (cons 10 p2) (cons 10 p3) (cons 10 p4))))

(defun eld:linha-h-sep (org larg / )
  (entmake (list '(0 . "LINE") '(100 . "AcDbEntity") '(100 . "AcDbLine")
                 (cons 10 org)
                 (cons 11 (list (+ (car org) larg) (cadr org) 0.0)))))

(defun eld:texto (pt texto h / )
  (entmake (list '(0 . "TEXT") '(100 . "AcDbEntity") '(100 . "AcDbText")
                 (cons 10 (list (car pt) (cadr pt) 0.0))
                 (cons 40 h) (cons 1 texto) '(72 . 0))))

(defun eld:texto-cel (org dados col-w row-h txt-h / i x y)
  (setq i 0)
  (foreach d dados
    (setq x (+ (car org) (* i col-w) (* col-w 0.05)))
    (setq y (+ (cadr org) (- row-h) (* row-h 0.25)))
    (eld:texto (list x y) (if d d "") txt-h)
    (setq i (1+ i))
  )
)

;;; ── EL-COMODO (delimita cômodo) ────────────────────────────────────────────

(defun C:EL-COMODO ( / nome lo pt area)
  (eld:setup-camadas)
  (setq nome (getstring T "\nNome do cômodo (ex: SALA): "))
  (if (= nome "") (setq nome "COMODO"))

  (setq lo (getvar "CLAYER"))
  (setvar "CLAYER" "EL-COMODO")

  (princ "\nTrace o perímetro do cômodo (polilinha fechada) — ENTER para encerrar:")
  (command "._PLINE")

  ; Fecha a polilinha se não foi fechada
  ; (o usuário pode usar C para fechar durante o PLINE)

  ; Rótulo com nome e área
  (princ "\nClique para posicionar o rótulo do cômodo:")
  (setq pt (getpoint))
  (if pt
    (progn
      (setvar "CLAYER" "EL-COMODO-TX")
      (entmake (list '(0 . "TEXT") '(100 . "AcDbEntity") '(100 . "AcDbText")
                     (cons 10 (list (car pt) (cadr pt) 0.0))
                     '(40 . 0.25) (cons 1 nome) '(72 . 4)))
    )
  )

  (setvar "CLAYER" lo)
  (princ (strcat "\nCômodo \"" nome "\" delimitado."))
  (princ)
)

;;; ── EL-CIRCUITO (cria/edita bloco de circuito) ────────────────────────────

(defun C:EL-CIRCUITO ( / )
  (alert
    (strcat
      "CIRCUITO\n\n"
      "Os atributos de circuito ficam nos próprios blocos elétricos.\n"
      "Para editar: clique duas vezes em qualquer bloco e altere\n"
      "o campo CIRCUITO (ex: C-1, C-2, IL-1) e TENSAO (127 ou 220).\n\n"
      "O campo CARGA (VA) pode ser editado para cargas específicas.\n"
      "Após editar, execute EL-DIM para novo dimensionamento."))
  (princ)
)

;;; ── Mensagem de carregamento ────────────────────────────────────────────────

(princ "\nMódulo de Dimensionamento (eletrica_dim.lsp) carregado.")
(princ "\n  EL-MAPA     -- mapa de circuitos no prompt")
(princ "\n  EL-DIM      -- dimensionamento NBR 5410 (CSV + relatório)")
(princ "\n  EL-TABELA   -- insere tabela de dimensionamento no desenho")
(princ "\n  EL-COMODO   -- delimita área de um cômodo")
(princ)
