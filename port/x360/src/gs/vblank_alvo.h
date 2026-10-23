/*
 * F5, etapa 8 (docs/x360/plano/F5/08-gravacao.md, seções 18 a 20): a conta da espera pelo vblank de hardware, em C89 puro e
 * sem o XDK. O gs_d3d.cpp (GsD3d_VblankEspera) a usa; port/x360/tools/teste_vblank.c a simula no Linux com o gcc.
 *
 * Um alvo (o número do vblank de hardware que acaba a próxima espera) anda um por chamada, como a grade do Sleep do
 * vblank_wait (port/src/plat_stub.c):
 *   - o alvo à frente do vblank atual: espera até ele (vb_esperou depois);
 *   - o alvo já passou: não espera (o passo da luta que passou de um vblank não perde a vez: a segunda espera dele acaba no
 *     tempo, e um passo que passou de dois paga a dívida nos seguintes);
 *   - a primeira chamada, ou o alvo VB_LONGE vblanks ou mais atrás (100 ms): um alvo novo, o próximo vblank, sem esperar.
 * (seção 20) A dívida parada: quando outra fila (a gravação, o Present) segura cada passo em exatamente dois vblanks, o alvo
 * atrasado anda junto com o vídeo e a dívida nunca se paga; a espera some (a janela 1 da rodada da seção 19 teve 0 esperas).
 * Por isso, numa sequência de chamadas com o alvo já passado, a soma dos atrasos (em vblanks) de cada bloco de VB_BLOCO
 * chamadas se compara com a menor de um bloco anterior da sequência: se VB_PARADOS blocos seguidos não ficaram abaixo dela,
 * a dívida não está sendo paga, e o alvo recomeça no próximo vblank, esperado já nesta chamada. Esperar já, e não só na
 * chamada seguinte: na trava, o jogo está um passo à frente da tela, parado na entrega à gravação; esperar o vblank aqui
 * troca essa parada por uma espera no vblank, a fila esvazia um passo, a imagem que já estava na fila cobre a vez, e o
 * controle passa a ser lido um passo mais perto da tela (teste_vblank.c: o caminho do controle à tela cai de 6 para 5
 * vblanks, com as mesmas imagens na tela). Sem esperar já, o jogo volta à fila e trava de novo. O atraso anda em vblanks
 * inteiros: um passo de 1,9 vblank paga um vblank a cada dez passos (20 chamadas), e os blocos seguidos sem queda que o
 * recomeço pede (VB_PARADOS + 1 blocos, 24 chamadas, 12 passos, 0,4 s) deixam essa dívida ser paga como antes. O que se
 * perdoa é só o atraso que não se paga em 12 passos.
 */
#ifndef BT3_VBLANK_ALVO_H
#define BT3_VBLANK_ALVO_H

#define VB_LONGE 6 /* vblanks atrás: um alvo novo (100 ms, como a grade do Sleep) */
#define VB_BLOCO 8   /* chamadas por bloco: quatro passos da luta (133 ms) */
#define VB_PARADOS 2 /* blocos seguidos sem a soma cair abaixo da menor: o recomeço */

typedef struct VbAlvo {
    unsigned alvo;      /* o vblank que acaba a próxima espera */
    int tem;            /* 0: sem alvo (a primeira chamada, ou depois do Sleep de reserva) */
    unsigned seguidas;  /* chamadas seguidas com o alvo já passado */
    int somaBloco;      /* a soma dos atrasos (vblanks) das chamadas do bloco corrente */
    int somaMenor;      /* a menor soma de um bloco da sequência; -1: nenhum bloco inteiro ainda */
    int parados;        /* blocos seguidos sem ficar abaixo da menor */
    unsigned passadas;  /* (só crescem) chamadas com o alvo já passado, sem esperar */
    unsigned recomecos; /* alvos recomeçados pela dívida parada */
    unsigned longes;    /* alvos novos por estar VB_LONGE vblanks atrás */
} VbAlvo;

static void vb_zera_seguidas(VbAlvo *a) {
    a->seguidas = 0;
    a->somaBloco = 0;
    a->somaMenor = -1;
    a->parados = 0;
}

/* sem alvo (a primeira chamada, ou a espera foi para o Sleep) */
static void vb_solta(VbAlvo *a) {
    a->tem = 0;
    vb_zera_seguidas(a);
}

/* n: o vblank de hardware atual. 0: espere até o vblank a->alvo e chame vb_esperou; 1: não espere */
static int vb_decide(VbAlvo *a, unsigned n) {
    int atraso = (int)(n - a->alvo);
    if (!a->tem || atraso >= VB_LONGE) {
        if (a->tem) {
            a->longes++;
            a->passadas++;
        }
        a->alvo = n + 1;
        a->tem = 1;
        vb_zera_seguidas(a);
        return 1;
    }
    if (atraso < 0) {
        vb_zera_seguidas(a);
        return 0;
    }
    a->passadas++;
    a->seguidas++;
    a->somaBloco += atraso;
    if (a->seguidas % VB_BLOCO == 0) {
        if (a->somaMenor < 0 || a->somaBloco < a->somaMenor) {
            a->somaMenor = a->somaBloco; /* (o primeiro bloco, ou a dívida caiu) */
            a->parados = 0;
        } else if (++a->parados >= VB_PARADOS) {
            a->alvo = n + 1; /* a dívida parada: espera o próximo vblank já (ver acima) */
            a->recomecos++;
            vb_zera_seguidas(a);
            return 0;
        }
        a->somaBloco = 0;
    }
    a->alvo++;
    return 1;
}

/* depois da espera até o vblank a->alvo */
static void vb_esperou(VbAlvo *a) {
    a->alvo++;
}

#endif
