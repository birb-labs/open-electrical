/* eletrica_panel.dcl — Plugin Elétrico Birb Labs
   ABNT NBR 5410 / 5444  |  BricsCAD

   Simbologia:
     Tomadas:      triângulo  vazio=baixo / hachura=médio / sólido=alto
     Interruptores: circ-eixo vazio=baixo / hachura=médio / sólido=alto
*/

eletrica_panel : dialog {
  label = "Plugin Elétrico — Birb Labs  (NBR 5410 / 5444)";

  : column {

    /* ── Configuração do ponto ──────────────────────────────── */
    : boxed_row {
      label = " Configuração do Ponto ";

      : column {
        : text     { label = "Circuito:"; }
        : edit_box { key = "circ_default"; edit_width = 9; }
      }
      : spacer { width = 1; }
      : column {
        : text       { label = "Altura na parede:"; }
        : popup_list { key = "altura_list"; edit_width = 16; }
      }
      : spacer { width = 1; }
      : column {
        : text       { label = "Módulos na placa:"; }
        : popup_list { key = "mods_list"; edit_width = 14; }
      }
    }

    : spacer { height = 0.4; }

    /* ── Linha 1: Tomadas + Interruptores ───────────────────── */
    : row {

      : boxed_column {
        label = " Tomadas ";
        : button { label = "Uso Geral     2P+T 10A  (TUG)"; key = "btn_tug"; }
        : button { label = "Específica    2P+T 20A  (TUE)"; key = "btn_tue"; }
        : button { label = "Trifásica     3P+T 20A  (TUF)"; key = "btn_tuf"; }
      }

      : spacer { width = 0.8; }

      : boxed_column {
        label = " Interruptores ";
        : button { label = "Simples          10A 250V   (IS)"; key = "btn_is";     }
        : button { label = "Duplo            10A 250V   (ID)"; key = "btn_id";     }
        : button { label = "Paralelo 3-way   10A 250V   (IP)"; key = "btn_ip";     }
        : button { label = "Temporizado      10A 250V   (IT)"; key = "btn_it";     }
        : button { label = "Pulsador Campainha 10A 250V (PU)"; key = "btn_ipulse"; }
      }

    }

    : spacer { height = 0.4; }

    /* ── Linha 2: Iluminação + Quadros + Especiais ──────────── */
    : row {

      : boxed_column {
        label = " Iluminação ";
        : button { label = "Ponto Luz Teto          (PLT)"; key = "btn_plt"; }
        : button { label = "Ponto Luz Parede        (PLP)"; key = "btn_plp"; }
        : button { label = "Luminária Embutida       (LE)"; key = "btn_le";  }
        : button { label = "Luminária Sobrepor       (LS)"; key = "btn_ls";  }
        : button { label = "Luminária Ext./Arandela (LEX)"; key = "btn_lex"; }
        : button { label = "Aplique de Parede       (APL)"; key = "btn_apl"; }
      }

      : spacer { width = 0.8; }

      : column {

        : boxed_column {
          label = " Quadros ";
          : button { label = "Quadro de Distribuição  (QD)"; key = "btn_qd"; }
          : button { label = "Quadro Geral F+L        (QG)"; key = "btn_qg"; }
        }

        : spacer { height = 0.3; }

        : boxed_column {
          label = " Sinalização e Dados ";
          : button { label = "TV / Antena            (PTV)";  key = "btn_ptv";  }
          : button { label = "Telefone / RJ45       (PTEL)";  key = "btn_ptel"; }
          : button { label = "Interfone             (PINT)";  key = "btn_pint"; }
          : button { label = "Campainha / Botoeira   (CAM)";  key = "btn_cam";  }
          : button { label = "Detector de Fumaça    (DFUM)";  key = "btn_dfum"; }
          : button { label = "Para-raios SPDA       (SPDA)";  key = "btn_spda"; }
        }

      }

    }

    : spacer { height = 0.4; }

    /* ── Percursos ───────────────────────────────────────────── */
    : boxed_row {
      label = " Percursos — trace com PLINE (C=fechar / ENTER=encerrar) ";
      : button { label = "Eletroduto Embutido";  key = "btn_ee";   }
      : button { label = "Eletroduto Aparente";  key = "btn_ea";   }
      : button { label = "Cabo / Fio";           key = "btn_cabo"; }
      : spacer { width = 0.5; }
      : button { label = "Delimitar Cômodo";     key = "btn_comodo"; }
    }

    : spacer { height = 0.4; }

    /* ── Dimensionamento e Relatórios ────────────────────────── */
    : boxed_row {
      label = " Dimensionamento ABNT NBR 5410 ";
      : button { label = "Mapa de Circuitos";         key = "btn_mapa";   }
      : button { label = "Dimensionar (CSV)";          key = "btn_dim";    }
      : button { label = "Tabela no Desenho";          key = "btn_tabela"; }
    }

    : spacer { height = 0.4; }

    /* ── Rodapé ──────────────────────────────────────────────── */
    : row {
      : button { label = "Relação de Materiais (BOM)"; key = "btn_bom"; }
      : spacer { }
      : button { label = "Fechar"; key = "cancel"; is_cancel = true; }
    }

  }
}
