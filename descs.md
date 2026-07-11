# CORREÇÃO DE SIMBOLOGIAS CONFORME NBR 5444

## CONTEXTO

vendo um plugin para o BricsCAD voltado para projetos de instalações elétricas de baixa tensão. O código-fonte está localizado no diretório: `~/Projects/Birb Labs/open-electrical/` a documentação do mesmo se encontra no sub diretorio `/docs`

Após a auditoria inicial, identificou-se que as simbologias dos componentes elétricos **não estão em conformidade com a NBR 5444** (Símbolos gráficos para instalações elétricas prediais). Esta norma, publicada pela ABNT em 1989, estabelece os símbolos gráficos padronizados para representação de dispositivos elétricos em projetos prediais.[reference:0]

## OBJETIVO PRINCIPAL

Você deve **corrigir todas as simbologias** dos componentes listados abaixo, substituindo as representações atuais (que provavelmente são genéricas ou incorretas) pelas **representações exatamente conforme definidas na NBR 5444**.

## FUNDAMENTOS DA NORMA (NBR 5444)

A simbologia da NBR 5444 é baseada em quatro elementos 
geométricos básicos:

| Elemento                 | Representa                                                 |
|--------------------------|------------------------------------------------------------|
| **Círculo**              | Ponto de luz, interruptor e dispositivos embutidos no teto |
| **Triângulo equilátero** | Tomadas em geral                                           |
| **Quadrado**             | Dispositivos no piso ou conversores de energia             |
| **Traço**                | Eletrodutos e condutores                                   |

**Regras importantes:**

- O ponto de luz deve ter diâmetro **maior** que o do interruptor para diferenciá-los
- Um símbolo **circundado** indica que o dispositivo está no **teto**
- Variações no triângulo indicam mudanças de significado, função e **níveis de instalação** (baixa, média, alta)

---

## LISTA DE COMPONENTES E SUAS SIMBOLOGIAS CORRETAS

### 1. QUADROS DE DISTRIBUIÇÃO

| Componente                | Símbolo NBR 5444                                         | Descrição |
|---------------------------|----------------------------------------------------------|-----------|
| **Quadro geral aparente** | Retângulo na proporção 2:1 (largura sendo o dobro da altura) com quinas retas paralelo à parede apenas encostando na mesma, com um triângulo retângulo em seu interior cuja a hipotenusa coincide com a diagonal secundária do retângulo. | Quadro geral de luz e força **aparente** (não embutido) |
| **Quadro geral embutido** | Identico ao **Quadro geral aparente** em forma e orientação, mas cerca de 75% dele se encontra dentro da parede | Quadro geral de luz e força **embutido** na parede |

---

### 2. INTERRUPTORES

| Componente | Símbolo NBR 5444 | Descrição |
|------------|------------------|-----------|
| **Interruptor de uma seção** | Círculo levemente afastado da parede com um traço que parre do polo mais proximo da parede até a  parede propriamente dita. ele também apresenta uma letra em sua vizinhança que representa o circuito a qual ele esta ligado | Círculo (diâmetro menor que o do ponto de luz). A letra minúscula indica o ponto comandado |
| **Interruptor de duas seções** | Idêntico ao **Interruptor de uma seção** porém o circulo é dividido em duas metades por um segmento de reta que parte do polo mais proximo à parede até o lado oposto. cada lado possui uma letra na proximidade | Círculo com duas saídas. As letras minúsculas indicam os pontos comandados |
| **Interruptor de três seções** | Idêntico ao **Interruptor de uma seção** porém o circulo é dividido em três partes iguais por fatias de mesmo angulo onde uma das divisórias coincide com o polo em que a o traço parte. cada fatia tem uma letra na proximidade | Círculo com três saídas. As letras minúsculas indicam os pontos comandados |
| **Interruptor paralelo (Three-Way)** | Idêntico ao **Interruptor de uma seção** porém o circulo é completamente preenchido | Interruptor paralelo ou Three-Way. A letra minúscula indica o ponto comandado |
| **Interruptor intermediário (Four-Way)** | Idêntico ao **Interruptor de duas seções** porém uma das metades é completamente preenchida e ele só possui uma letra | Interruptor intermediário ou Four-Way. A letra minúscula indica o ponto comandado |
| **Botão de campainha** | Idêntico ao **Interruptor de uma seção**  com um circulo completamente preenchido em seu centro | Botão de campainha na parede. Nota: Os símbolos de 7.1 a 7.8 são para plantas e 7.9 a 7.16 para diagramas |

**OBSERVAÇÕES IMPORTANTES:**

- O círculo do interruptor deve ser **menor** que o círculo do ponto de luz
- Interruptor paralelo e intermediário são usados em circuitos de comando de três ou mais pontos

---

### 3. TOMADAS[reference:19]

| Componente       | Símbolo NBR 5444 | Descrição |
|------------------|------------------|-----------|
| **Tomada baixa** | Triângulo equilátero levemente afastado da parede com com um dos lados paralelo à parede, co centro deste mesmo lado parte uma reta que o liga a parede | Tomada de luz na parede, **baixa**  |
| **Tomada média** | Idêntico à **Tomada baixa** mas o lado do triangulo à direita da reta que o liga a parede é totalmente preenchido | Tomada de luz a **meia altura** |
| **Tomada alta**  | Idêntico à **Tomada baixa** mas o triangulo à é totalmente preenchido | Tomada de luz **alta** |
| **Tomada com interruptor de duas seções** | Este tipo de tomada é obrigatoriamente de **Média Altura**, protanto é identica à **Tomada média** porem com o circulo que representa o **Interruptor de duas seções** na sua ponta sem a reta que ligaria o interruptor à parede| Representação combinada: tomada com interruptor de duas seções no mesmo ponto |
| **Tomada dupla com interruptor de uma seção** |  Este tipo de tomada é obrigatoriamente de **Média Altura**, protanto é identica à **Tomada média** porem com o circulo que representa o **Interruptor de uma seção**| Representação combinada: duas tomadas com interruptor de uma seção |

**OBSERVAÇÕES IMPORTANTES:**

- O **triângulo equilátero** é a base para todas as tomadas
- A potência deve ser indicada em VA (exceto se for de 100 VA), assim como o circuito correspondente
- As tomadas **SEM** interruptores podem apresntar de um à três módulos sendo, onde cada modulo pode ter uma altura diferente. para representar uma tomada com mais de um modulo insira o primeiro módulo como se fosse uma tomada simples e em sua ponta coloque o trangulo (sem a reta que ligaria a parede) do segundo modulo e repita o processo com a ponta do segundo modulo e o triangulo do 3 se houver um

---

### 4. PONTOS DE LUZ[reference:27]

| Componente | Símbolo NBR 5444 | Descrição |
|------------|------------------|-----------|
| **Ponto de luz (teto)** | Círculo **maior** com uma corda horizontal que conecta os polos leste e oesta passando pelo centro e um segmento de  reta que parte do centro até o polo sul, dividindo o circulo em 3 setores no do topo consta a notação da potência (EX: 2x32w), no infreior esquerdo consta a notação do interruptor que controla aquele ponto (mesma letra  usada na notação do interruptor), e no inferior direiro consta a notação do número do circuito em que o ponto esta ligado entre traços (hifens) | Ponto de luz incandescente no **teto**. A letra minúscula indica o ponto de comando e o número entre dois traços o circuito correspondente |
| **Ponto de luz na parede** | meio circulo com seu lado reto paralelop a parede em que se encontra dividido em 2 por um segmento de reta que parte de seu unico polo até o centro de seu lado reto, em um dos lados fica a letra do interruptor em outro fica o número do circuito (dessa vez sem nenhum traço) | Ponto de luz incandescente na **parede** (arandela). Deve-se indicar a altura da arandela |

**OBSERVAÇÕES IMPORTANTES:**

- O círculo do ponto de luz deve ter **diâmetro maior** que o do interruptor

---

## ESTRUTURA DE CORREÇÃO ESPERADA

Para cada componente, você deve:

1. **Localizar** no código-fonte onde a simbologia atual está definida (arquivos de desenho, blocos DWG, definições de entidades, ou código de renderização)
2. **Substituir** pela simbologia correta conforme descrito acima
3. **Garantir** que as proporções (diâmetro do círculo, tamanho do triângulo, espessura dos traços) estejam adequadas para visualização em planta baixa
4. **Atualizar** a legenda ou documentação para refletir as novas simbologias

--

## FORMATO DE SAÍDA ESPERADO

Gere um relatório em formato **Markdown** com:

1. **Resumo das alterações realizadas** (lista de componentes corrigidos)
2. **Arquivos modificados** (caminho e descrição da alteração)
3. **Validação** — verifique se todos os 15 componentes foram corrigidos
4. **Próximos passos** — sugestões para testes visuais e validação com engenheiros eletricistas

---

**Execute a correção agora e me apresente o relatório completo ao final.** Você pode modificar o que precisar no código, principalmente as interfaces caso elas não permitam o controle necessário sobre os componetes inseridos

Mãos à obra!