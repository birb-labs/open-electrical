;;; eletrica.lsp -- Plugin Eletrico para BricsCAD
;;; Birb Labs | Instalacoes Eletricas de Baixa Tensao -- ABNT NBR 5410 / 5444
;;; Versao 3.0

;;; Simbologia de tomadas (triangulo):
;;;   (base)  = baixa <= 0,40 m   -- triangulo vazio
;;;   (base)M = media ~1,10 m     -- triangulo hachura
;;;   (base)A = alta  ~1,50 m     -- triangulo solido
;;;
;;; Simbologia de interruptores (circulo-eixo):
;;;   (base)  = baixo             -- circulo vazio
;;;   (base)M = medio             -- circulo hachura
;;;   (base)A = alto              -- circulo solido

(setq *EL-BLOCOS* "eletrica_blocos")

;;; -- CAMADAS ----------------------------------------------------------------

(defun el:mklay (nome cor ltipo / )
  (if (not (tblsearch "LAYER" nome))
    (entmake (list
      '(0 . "LAYER") '(100 . "AcDbSymbolTableRecord") '(100 . "AcDbLayerTableRecord")
      (cons 2 nome) '(70 . 0) (cons 62 cor) (cons 6 ltipo)))))

(defun el:setup-camadas ( / )
  (el:mklay "EL-PONTOS"       4  "Continuous")
  (el:mklay "EL-LUMINARIAS"   3  "Continuous")
  (el:mklay "EL-ELETRODUTO-E" 30 "HIDDEN")
  (el:mklay "EL-ELETRODUTO-A" 1  "Continuous")
  (el:mklay "EL-QUADROS"      5  "Continuous")
  (el:mklay "EL-CABO"         6  "Continuous")
  (el:mklay "EL-TEXTO"        7  "Continuous")
  (el:mklay "EL-COMODO"       8  "Continuous")
  (el:mklay "EL-COMODO-TX"    8  "Continuous")
  (el:mklay "EL-DIM-TAB"      7  "Continuous")
  (princ "\nCamadas eletricas criadas.")
)

;;; -- CARREGAR BLOCOS --------------------------------------------------------

(defun el:carregar-blocos ( / f)
  (setq f (findfile (strcat *EL-BLOCOS* ".dxf")))
  (if (not f) (setq f (findfile (strcat *EL-BLOCOS* ".dwg"))))
  (if f
    (progn
      (setvar "ATTREQ" 0)
      (command "._-INSERT" f "0,0" "1" "1" "0")
      (setvar "ATTREQ" 1)
      (command "._ERASE" "L" "")
      (princ "\nBlocos eletricos carregados.")
      T)
    (progn
      (princ "\n[ERRO] eletrica_blocos.dxf nao encontrado no Support path.")
      (princ "\nExecute ./instalar.sh novamente.")
      nil)))

(defun el:garantir-bloco (nome / )
  (if (not (tblsearch "BLOCK" nome)) (el:carregar-blocos))
  (if (tblsearch "BLOCK" nome) T nil))

;;; -- ATRIBUTO PADRAO (pre-fill antes do INSERT) ----------------------------

(defun el:set-attdef (blkname tag valor / e edata)
  (setq e (tblobjname "BLOCK" blkname))
  (if (not e) (exit))
  (setq e (entnext e))
  (while e
    (setq edata (entget e))
    (cond
      ((= (cdr (assoc 0 edata)) "ATTDEF")
       (if (= (strcase (cdr (assoc 2 edata))) (strcase tag))
         (progn
           (entmod (subst (cons 1 valor) (assoc 1 edata) edata))
           (entupd e)
           (setq e nil))
         (setq e (entnext e))))
      ((= (cdr (assoc 0 edata)) "ENDBLK") (setq e nil))
      (T (setq e (entnext e))))))

;;; -- INSERCAO GENERICA ------------------------------------------------------

(defun el:inserir (bloco camada circ-val desc-val / lo)
  (if (not (el:garantir-bloco bloco))
    (progn (princ (strcat "\n[ERRO] Bloco nao encontrado: " bloco)) (exit)))
  (if (not (tblsearch "LAYER" camada)) (el:setup-camadas))
  (setq lo (getvar "CLAYER"))
  (setvar "CLAYER" camada)
  (el:set-attdef bloco "CIRCUITO" circ-val)
  (el:set-attdef bloco "DESC"     desc-val)
  (setvar "ATTREQ" 0)
  (command "._-INSERT" bloco)
  (setvar "ATTREQ" 1)
  (setvar "CLAYER" lo))

;;; -- PERGUNTAS REUTILIZAVEIS ------------------------------------------------

(defun el:perg-circ ( / c)
  (setq c (getstring (strcat "\nCircuito <" (if *el-circ-last* *el-circ-last* "C-1") ">: ")))
  (if (= c "") (if *el-circ-last* *el-circ-last* "C-1") c))

(defun el:perg-altura ( / a)
  (initget "Baixa Media Alta")
  (setq a (getkword "\nAltura [Baixa/Media/Alta] <Baixa>: "))
  (if (not a) "Baixa" a))

(defun el:perg-mods ( / m)
  (initget "1 2 3")
  (setq m (getkword "\nModulos [1/2/3] <1>: "))
  (if (not m) "1" m))

(defun el:mods-nome (m)
  (cond ((= m "1") "Simples") ((= m "2") "Dupla") (T "Tripla")))

;;; Mapeia nome-base + altura para nome do bloco
(defun el:bloco-alt (base alt)
  (cond ((= alt "Alta")  (strcat base "A"))
        ((= alt "Media") (strcat base "M"))
        (T base)))

;;; -- TOMADAS ----------------------------------------------------------------

(defun C:EL-TUG ( / circ alt mods b)
  (setq circ (el:perg-circ))
  (setq alt  (el:perg-altura))
  (setq mods (el:perg-mods))
  (setq b (el:bloco-alt "TUG" alt))
  (el:inserir b "EL-PONTOS"
    (strcat circ " [" alt "/" mods "x]")
    (strcat "Tomada de Uso Geral 2P+T 10A - " alt " - " (el:mods-nome mods)))
  (princ))

(defun C:EL-TUE ( / circ alt mods b)
  (setq circ (el:perg-circ))
  (setq alt  (el:perg-altura))
  (setq mods (el:perg-mods))
  (setq b (el:bloco-alt "TUE" alt))
  (el:inserir b "EL-PONTOS"
    (strcat circ " [" alt "/" mods "x]")
    (strcat "Tomada de Uso Especifico 2P+T 20A 220V - " alt " - " (el:mods-nome mods)))
  (princ))

(defun C:EL-TUF ( / circ mods)
  (setq circ (el:perg-circ))
  (setq mods (el:perg-mods))
  (el:inserir "TUF" "EL-PONTOS"
    (strcat circ " [" mods "x]")
    (strcat "Tomada Trifasica 3P+T 20A 220V - " (el:mods-nome mods)))
  (princ))

;;; -- INTERRUPTORES ----------------------------------------------------------

(defun C:EL-IS ( / circ alt mods b)
  (setq circ (el:perg-circ))
  (setq alt  (el:perg-altura))
  (setq mods (el:perg-mods))
  (setq b (el:bloco-alt "IS" alt))
  (el:inserir b "EL-PONTOS"
    (strcat circ " [" mods "x]")
    (strcat "Interruptor Simples 10A 250V - " alt " - " (el:mods-nome mods)))
  (princ))

(defun C:EL-ID ( / circ alt mods b)
  (setq circ (el:perg-circ))
  (setq alt  (el:perg-altura))
  (setq mods (el:perg-mods))
  (setq b (el:bloco-alt "ID" alt))
  (el:inserir b "EL-PONTOS"
    (strcat circ " [" mods "x]")
    (strcat "Interruptor Duplo 10A 250V - " alt " - " (el:mods-nome mods)))
  (princ))

(defun C:EL-IP ( / circ mods)
  (setq circ (el:perg-circ))
  (setq mods (el:perg-mods))
  (el:inserir "IP" "EL-PONTOS"
    (strcat circ " [" mods "x]")
    (strcat "Interruptor Paralelo 10A 250V - " (el:mods-nome mods)))
  (princ))

(defun C:EL-IT ( / circ alt mods b)
  (setq circ (el:perg-circ))
  (setq alt  (el:perg-altura))
  (setq mods (el:perg-mods))
  (setq b (el:bloco-alt "IT" alt))
  (el:inserir b "EL-PONTOS"
    (strcat circ " [" mods "x]")
    (strcat "Interruptor Temporizado 10A 250V - " alt " - " (el:mods-nome mods)))
  (princ))

(defun C:EL-IPULSE ( / circ)
  (setq circ (el:perg-circ))
  (el:inserir "IPULSE" "EL-PONTOS"
    circ "Interruptor Pulsador de Campainha 10A 250V")
  (princ))

;;; -- ILUMINACAO -------------------------------------------------------------

(defun el:ins-lum (bloco desc / circ)
  (setq circ (el:perg-circ))
  (el:inserir bloco "EL-LUMINARIAS" circ desc))

(defun C:EL-PLT  () (el:ins-lum "PLT" "Ponto de Luz de Teto")                    (princ))
(defun C:EL-PLP  () (el:ins-lum "PLP" "Ponto de Luz de Parede")                  (princ))
(defun C:EL-LE   () (el:ins-lum "LE"  "Luminaria Embutida (Painel LED/Fluor.)")  (princ))
(defun C:EL-LS   () (el:ins-lum "LS"  "Luminaria de Sobrepor")                   (princ))
(defun C:EL-LEX  () (el:ins-lum "LEX" "Luminaria Externa / Arandela")            (princ))
(defun C:EL-APL  () (el:ins-lum "APL" "Aplique de Parede")                       (princ))

;;; -- QUADROS ----------------------------------------------------------------

(defun el:ins-qd (bloco desc / circ)
  (setq circ (el:perg-circ))
  (el:inserir bloco "EL-QUADROS" circ desc))

(defun C:EL-QD () (el:ins-qd "QD" "Quadro de Distribuicao")       (princ))
(defun C:EL-QG () (el:ins-qd "QG" "Quadro Geral de Forca e Luz")  (princ))

;;; -- PONTOS ESPECIAIS --------------------------------------------------------

(defun el:ins-esp (bloco desc / circ)
  (setq circ (el:perg-circ))
  (el:inserir bloco "EL-PONTOS" circ desc))

(defun C:EL-PTV  () (el:ins-esp "PTV"  "Ponto de TV a Cabo / Antena")      (princ))
(defun C:EL-PTEL () (el:ins-esp "PTEL" "Ponto de Telefone / Logica RJ45")  (princ))
(defun C:EL-PINT () (el:ins-esp "PINT" "Ponto de Interfone")                (princ))
(defun C:EL-CAM  () (el:ins-esp "CAM"  "Campainha / Botoeira")              (princ))
(defun C:EL-DFUM () (el:ins-esp "DFUM" "Detector de Fumaca")                (princ))
(defun C:EL-SPDA () (el:ins-esp "SPDA" "Haste do SPDA (Para-raios)")        (princ))

;;; -- PERCURSOS --------------------------------------------------------------

(defun C:EL-EE ( / lo)
  (setq lo (getvar "CLAYER"))
  (setvar "CLAYER" "EL-ELETRODUTO-E")
  (princ "\nEletroduto EMBUTIDO -- trace o percurso (C=fechar / ENTER=encerrar):")
  (command "._PLINE")
  (setvar "CLAYER" lo) (princ))

(defun C:EL-EA ( / lo)
  (setq lo (getvar "CLAYER"))
  (setvar "CLAYER" "EL-ELETRODUTO-A")
  (princ "\nEletroduto APARENTE -- trace o percurso:")
  (command "._PLINE")
  (setvar "CLAYER" lo) (princ))

(defun C:EL-CABO ( / lo)
  (setq lo (getvar "CLAYER"))
  (setvar "CLAYER" "EL-CABO")
  (princ "\nCabo/Fio -- trace o percurso:")
  (command "._PLINE")
  (setvar "CLAYER" lo) (princ))

;;; -- BOM: LER ATRIBUTO ------------------------------------------------------

(defun el:get-att (ins-ename tag / ent edata val)
  (setq val "" ent (entnext ins-ename))
  (while ent
    (setq edata (entget ent))
    (cond
      ((= (cdr (assoc 0 edata)) "ATTRIB")
       (if (= (strcase (cdr (assoc 2 edata))) (strcase tag))
         (progn (setq val (cdr (assoc 1 edata))) (setq ent nil))
         (setq ent (entnext ent))))
      ((= (cdr (assoc 0 edata)) "SEQEND") (setq ent nil))
      (T (setq ent (entnext ent)))))
  val)

(defun el:desc-padrao (nome)
  (cond
    ((= nome "TUG")   "Tomada de Uso Geral 2P+T 10A")
    ((= nome "TUGM")  "Tomada de Uso Geral 2P+T 10A (Bancada)")
    ((= nome "TUGA")  "Tomada de Uso Geral Alta 2P+T 10A")
    ((= nome "TUE")   "Tomada de Uso Especifico 2P+T 20A 220V")
    ((= nome "TUEM")  "Tomada de Uso Especifico 2P+T 20A 220V (Bancada)")
    ((= nome "TUEA")  "Tomada de Uso Especifico Alta 2P+T 20A 220V")
    ((= nome "TUF")   "Tomada Trifasica 3P+T 20A 220V")
    ((= nome "IS")    "Interruptor Simples 10A 250V")
    ((= nome "ISM")   "Interruptor Simples 10A 250V (Medio)")
    ((= nome "ISA")   "Interruptor Simples Alto 10A 250V")
    ((= nome "ID")    "Interruptor Duplo 10A 250V")
    ((= nome "IDM")   "Interruptor Duplo 10A 250V (Medio)")
    ((= nome "IDA")   "Interruptor Duplo Alto 10A 250V")
    ((= nome "IP")    "Interruptor Paralelo 10A 250V")
    ((= nome "IT")    "Interruptor Temporizado 10A 250V")
    ((= nome "IPULSE") "Interruptor Pulsador de Campainha 10A 250V")
    ((= nome "PLT")   "Ponto de Luz de Teto")
    ((= nome "PLP")   "Ponto de Luz de Parede")
    ((= nome "LE")    "Luminaria Embutida")
    ((= nome "LS")    "Luminaria de Sobrepor")
    ((= nome "LEX")   "Luminaria Externa / Arandela")
    ((= nome "APL")   "Aplique de Parede")
    ((= nome "QD")    "Quadro de Distribuicao")
    ((= nome "QG")    "Quadro Geral de Forca e Luz")
    ((= nome "PTV")   "Ponto de TV")
    ((= nome "PTEL")  "Ponto de Telefone / Logica RJ45")
    ((= nome "PINT")  "Ponto de Interfone")
    ((= nome "CAM")   "Campainha / Botoeira")
    ((= nome "DFUM")  "Detector de Fumaca")
    ((= nome "SPDA")  "Haste do SPDA")
    (T nome)))

(defun el:blocos-eletricos ()
  "TUG,TUGM,TUGA,TUE,TUEM,TUEA,TUF,IS,ISM,ISA,ID,IDM,IDA,IP,IT,IPULSE,PLT,PLP,LE,LS,LEX,APL,QD,QG,PTV,PTEL,PINT,CAM,DFUM,SPDA")

;;; -- BOM: CONTAR E EXPORTAR -------------------------------------------------

(defun el:bom-add (bom bname desc / done result)
  (setq done nil result '())
  (foreach item bom
    (if (and (not done) (= (car item) bname) (= (cadr item) desc))
      (progn (setq result (append result (list (list bname desc (1+ (caddr item)))))) (setq done T))
      (setq result (append result (list item)))))
  (if (not done) (append result (list (list bname desc 1))) result))

(defun el:contar-blocos ( / ss i ename bname desc bom)
  (setq bom '())
  (setq ss (ssget "X" '((0 . "INSERT"))))
  (if ss
    (progn
      (setq i 0)
      (while (< i (sslength ss))
        (setq ename (ssname ss i))
        (setq bname (cdr (assoc 2 (entget ename))))
        (if (wcmatch bname (el:blocos-eletricos))
          (progn
            (setq desc (el:get-att ename "DESC"))
            (if (= desc "") (setq desc (el:desc-padrao bname)))
            (setq bom (el:bom-add bom bname desc))))
        (setq i (1+ i)))))
  bom)

(defun C:EL-BOM ( / bom outfile fname total item unid)
  (setq bom (el:contar-blocos))
  (if (not bom)
    (progn (princ "\nNenhum componente eletrico encontrado.") (exit)))
  (setq fname (strcat (getvar "DWGPREFIX") (getvar "DWGNAME") "_relacao_materiais.csv"))
  (setq outfile (open fname "w"))
  (if (not outfile) (progn (princ "\nErro ao criar CSV.") (exit)))
  (write-line "RELACAO DE MATERIAIS - INSTALACOES ELETRICAS" outfile)
  (write-line "" outfile)
  (write-line "ITEM;SIGLA;DESCRICAO;UNID.;QTDE." outfile)
  (setq total 0 item 1)
  (princ "\n\n-- RELACAO DE MATERIAIS ----------------------------------")
  (foreach c bom
    (setq unid (if (wcmatch (car c) "PLT,PLP,PTV,PTEL,PINT") "pt." "un."))
    (write-line (strcat (itoa item) ";" (car c) ";" (cadr c) ";" unid ";" (itoa (caddr c))) outfile)
    (princ (strcat "\n  " (car c) "  " (itoa (caddr c)) " " unid "  " (cadr c)))
    (setq total (+ total (caddr c)))
    (setq item (1+ item)))
  (write-line "" outfile)
  (write-line (strcat "TOTAL;;" (itoa total)) outfile)
  (close outfile)
  (princ "\n----------------------------------------------------------")
  (princ (strcat "\n  TOTAL: " (itoa total) " pontos/componentes"))
  (princ (strcat "\n  CSV: " fname "\n"))
  (princ))

;;; -- PAINEL DCL -------------------------------------------------------------

(if (null *el-circ-last*) (setq *el-circ-last* "C-1"))
(if (null *el-alt-last*)  (setq *el-alt-last*  0))
(if (null *el-mods-last*) (setq *el-mods-last* 0))
(if (null *el-accion*)    (setq *el-accion*    ""))

(defun el:safe-atoi (s fallback)
  (if s (atoi s) fallback))

(defun el:painel-dispatch (accion circ alt mods / b)
  (cond
    ; Tomadas -- nome do bloco depende da altura
    ((= accion "TUG")
     (setq b (el:bloco-alt "TUG" alt))
     (el:inserir b "EL-PONTOS" (strcat circ " [" alt "/" mods "x]")
       (strcat "Tomada de Uso Geral 2P+T 10A - " alt " - " (el:mods-nome mods))))
    ((= accion "TUE")
     (setq b (el:bloco-alt "TUE" alt))
     (el:inserir b "EL-PONTOS" (strcat circ " [" alt "/" mods "x]")
       (strcat "Tomada de Uso Especifico 2P+T 20A 220V - " alt " - " (el:mods-nome mods))))
    ((= accion "TUF")
     (el:inserir "TUF" "EL-PONTOS" (strcat circ " [" mods "x]")
       (strcat "Tomada Trifasica 3P+T 20A 220V - " (el:mods-nome mods))))
    ; Interruptores -- nome do bloco depende da altura
    ((= accion "IS")
     (setq b (el:bloco-alt "IS" alt))
     (el:inserir b "EL-PONTOS" (strcat circ " [" mods "x]")
       (strcat "Interruptor Simples 10A 250V - " alt " - " (el:mods-nome mods))))
    ((= accion "ID")
     (setq b (el:bloco-alt "ID" alt))
     (el:inserir b "EL-PONTOS" (strcat circ " [" mods "x]")
       (strcat "Interruptor Duplo 10A 250V - " alt " - " (el:mods-nome mods))))
    ((= accion "IP")
     (el:inserir "IP" "EL-PONTOS" (strcat circ " [" mods "x]")
       (strcat "Interruptor Paralelo 10A 250V - " (el:mods-nome mods))))
    ((= accion "IT")
     (setq b (el:bloco-alt "IT" alt))
     (el:inserir b "EL-PONTOS" (strcat circ " [" mods "x]")
       (strcat "Interruptor Temporizado 10A 250V - " alt " - " (el:mods-nome mods))))
    ((= accion "IPULSE")
     (el:inserir "IPULSE" "EL-PONTOS" circ "Interruptor Pulsador de Campainha 10A 250V"))
    ; Iluminacao
    ((= accion "PLT")  (el:inserir "PLT"  "EL-LUMINARIAS" circ "Ponto de Luz de Teto"))
    ((= accion "PLP")  (el:inserir "PLP"  "EL-LUMINARIAS" circ "Ponto de Luz de Parede"))
    ((= accion "LE")   (el:inserir "LE"   "EL-LUMINARIAS" circ "Luminaria Embutida (Painel LED/Fluor.)"))
    ((= accion "LS")   (el:inserir "LS"   "EL-LUMINARIAS" circ "Luminaria de Sobrepor"))
    ((= accion "LEX")  (el:inserir "LEX"  "EL-LUMINARIAS" circ "Luminaria Externa / Arandela"))
    ((= accion "APL")  (el:inserir "APL"  "EL-LUMINARIAS" circ "Aplique de Parede"))
    ; Quadros
    ((= accion "QD")   (el:inserir "QD"   "EL-QUADROS" circ "Quadro de Distribuicao"))
    ((= accion "QG")   (el:inserir "QG"   "EL-QUADROS" circ "Quadro Geral de Forca e Luz"))
    ; Especiais
    ((= accion "PTV")  (el:inserir "PTV"  "EL-PONTOS" circ "Ponto de TV a Cabo / Antena"))
    ((= accion "PTEL") (el:inserir "PTEL" "EL-PONTOS" circ "Ponto de Telefone / Logica RJ45"))
    ((= accion "PINT") (el:inserir "PINT" "EL-PONTOS" circ "Ponto de Interfone"))
    ((= accion "CAM")  (el:inserir "CAM"  "EL-PONTOS" circ "Campainha / Botoeira"))
    ((= accion "DFUM") (el:inserir "DFUM" "EL-PONTOS" circ "Detector de Fumaca"))
    ((= accion "SPDA") (el:inserir "SPDA" "EL-PONTOS" circ "Haste do SPDA (Para-raios)"))
    ; Percursos
    ((= accion "EE")     (C:EL-EE))
    ((= accion "EA")     (C:EL-EA))
    ((= accion "CABO")   (C:EL-CABO))
    ; Comodo
    ((= accion "COMODO") (C:EL-COMODO))
    ; Dimensionamento
    ((= accion "DIM")    (C:EL-DIM))
    ((= accion "MAPA")   (C:EL-MAPA))
    ((= accion "TABELA") (C:EL-TABELA))
    ; BOM
    ((= accion "BOM")    (C:EL-BOM))))

(defun C:EL-PAINEL ( / dcl-id dlg-ok circ alt mods continuar)
  (setq dcl-id (load_dialog "eletrica_panel.dcl"))
  (if (< dcl-id 0)
    (progn (alert "Erro: eletrica_panel.dcl nao encontrado no Support path.") (exit)))

  (setq continuar T)
  (while continuar
    (setq *el-accion* nil)
    (if (not (new_dialog "eletrica_panel" dcl-id))
      (progn (alert "Erro ao abrir painel.") (setq continuar nil))
      (progn
        (set_tile "circ_default" (if *el-circ-last* *el-circ-last* "C-1"))

        (start_list "altura_list")
        (mapcar 'add_list '("Baixa (<=0,40m)" "Media (~1,10m)" "Alta (~1,50m)"))
        (end_list)
        (set_tile "altura_list" (itoa (if *el-alt-last* *el-alt-last* 0)))

        (start_list "mods_list")
        (mapcar 'add_list '("1 modulo" "2 modulos" "3 modulos"))
        (end_list)
        (set_tile "mods_list" (itoa (if *el-mods-last* *el-mods-last* 0)))

        (foreach par '(
          ("btn_tug"    "TUG")    ("btn_tue"    "TUE")    ("btn_tuf"  "TUF")
          ("btn_is"     "IS")     ("btn_id"     "ID")     ("btn_ip"   "IP")
          ("btn_it"     "IT")     ("btn_ipulse" "IPULSE")
          ("btn_plt"    "PLT")    ("btn_plp"    "PLP")
          ("btn_le"     "LE")     ("btn_ls"     "LS")
          ("btn_lex"    "LEX")    ("btn_apl"    "APL")
          ("btn_qd"     "QD")     ("btn_qg"     "QG")
          ("btn_ptv"    "PTV")    ("btn_ptel"   "PTEL")   ("btn_pint" "PINT")
          ("btn_cam"    "CAM")    ("btn_dfum"   "DFUM")   ("btn_spda" "SPDA")
          ("btn_ee"     "EE")     ("btn_ea"     "EA")     ("btn_cabo" "CABO")
          ("btn_comodo" "COMODO") ("btn_dim"    "DIM")
          ("btn_mapa"   "MAPA")   ("btn_tabela" "TABELA")
          ("btn_bom"    "BOM")
        )
          (action_tile (car par)
            (strcat
              "(setq *el-accion* " (chr 34) (cadr par) (chr 34)
              " *el-circ-last* (get_tile " (chr 34) "circ_default" (chr 34) ")"
              " *el-alt-last*  (el:safe-atoi (get_tile " (chr 34) "altura_list" (chr 34) ") *el-alt-last*)"
              " *el-mods-last* (el:safe-atoi (get_tile " (chr 34) "mods_list"   (chr 34) ") *el-mods-last*)"
              ") (done_dialog 1)")))

        (action_tile "cancel" "(done_dialog 0)")
        (setq dlg-ok (start_dialog))
      )
    )

    (if (= dlg-ok 1)
      (progn
        (setq circ (if *el-circ-last* *el-circ-last* "C-1"))
        (setq alt  (nth *el-alt-last* '("Baixa" "Media" "Alta")))
        (setq mods (itoa (1+ *el-mods-last*)))
        (if (member *el-accion* '("BOM" "DIM" "MAPA" "TABELA" "COMODO"))
          (progn (el:painel-dispatch *el-accion* circ alt mods) (setq continuar nil))
          (el:painel-dispatch *el-accion* circ alt mods))
      )
      (setq continuar nil)
    )
  )
  (unload_dialog dcl-id)
  (princ))

;;; -- SETUP ------------------------------------------------------------------

(defun C:EL-RECARREGAR ( / f)
  (setq f (findfile "eletrica.lsp"))
  (if f (progn (load f) (princ "\nPlugin recarregado OK."))
    (princ "\n[ERRO] eletrica.lsp nao encontrado."))
  (princ))

(defun C:EL-SETUP ( / )
  (princ "\n+--------------------------------------------------+")
  (princ "\n|  Plugin Eletrico -- Birb Labs  (ABNT NBR 5410)   |")
  (princ "\n+--------------------------------------------------+")
  (el:setup-camadas)
  (el:carregar-blocos)
  ; Carrega modulo de dimensionamento
  (setq fdim (findfile "eletrica_dim.lsp"))
  (if fdim (load fdim) (princ "\n[AVISO] eletrica_dim.lsp nao encontrado."))
  (princ "\n\nSetup concluido! Abrindo painel...")
  (C:EL-PAINEL)
  (princ))

;;; -- Carregamento automatico do modulo dim -----------------------------------

(setq *EL-DIM-LSP* (findfile "eletrica_dim.lsp"))
(if *EL-DIM-LSP* (load *EL-DIM-LSP*))

;;; -- Mensagem de carregamento ------------------------------------------------

(princ "\nPlugin Eletrico (Birb Labs) v3.0 carregado -- ABNT NBR 5410/5444")
(princ "\n  EL-SETUP    -- configura camadas, blocos e abre painel")
(princ "\n  EL-PAINEL   -- abre o painel de insercao")
(princ "\n  EL-MAPA     -- mapa de circuitos no prompt")
(princ "\n  EL-DIM      -- dimensionamento automatico (CSV)")
(princ "\n  EL-TABELA   -- tabela de dimensionamento no desenho")
(princ "\n  EL-COMODO   -- delimitar area de comodo")
(princ "\n  EL-BOM      -- relacao de materiais (CSV)")
(princ)
