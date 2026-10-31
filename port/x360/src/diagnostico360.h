/* Chaves exclusivas de diagnóstico. Sem BT3_LOG=1, nem uma chave avulsa arma medidas ou arquivos. */
#ifndef DIAGNOSTICO360_H
#define DIAGNOSTICO360_H
static int Diagnostico360_Chave(const char *nome) {
    static const char *const chaves[] = {
        "BT3_TEMPOS", "BT3_ESTADO_TEMPOS", "BT3_FILME_MEDIDAS", "BT3_SOM_MEDIDAS",
        "BT3_LOG_SINCRONO", "BT3_LOG_FLUSH_MS", "BT3_PICOS", "BT3_PICOS_TELA",
        "BT3_CORTES", "BT3_CORTES_GPU", "BT3_COMUM_GPU", "BT3_CADEIA_CPU",
        "BT3_FILME_LOG", "BT3_SOM_DERIVA", "BT3_MPEG_MEDE", "BT3_FILME_SHOTS",
        "BT3_SHOT", "BT3_SHOT_LISTA", "BT3_SHOT_FROM", "BT3_SHOT_TO", "BT3_SHOT_PASTA", "BT3_SHOT_CHEIA",
        "BT3_PRESENT_LOG", "BT3_INTERP_FRAMES", "BT3_INTERP_DUMP", "BT3_INTERP_LOG",
        "BT3_INTERP_CUTLOG", "BT3_INTERP_STEPLOG", "BT3_TEX_DUMP", "BT3_BGM_DEBUG", "BT3_STAGES_DEBUG",
        "BT3_HASH", "BT3_TRACE", "BT3_TRACE_SEQ", "BT3_TRACE_CAM", "BT3_DUMP", "BT3_CAMCHECK",
        "BT3_GS_VERBOSE", "BT3_SND_VERBOSE", "BT3_SONG_DEBUG", "BT3_TEX_LOG",
        "BT3_PAD_REC", "BT3_PAD_TABLE", "BT3_ENVIO_CONTA", "BT3_TEX_CONTA", "BT3_UNPACK_CONTA",
        "BT3_VU_DUMP", "BT3_VU_CALLS", "BT3_GS_DUMP", "BT3_GS_ZDUMP", "BT3_GS_PROBE",
        "BT3_GS_RACE", "BT3_CLASH_LOG", "BT3_NAMEFONT_TEST", "BT3_NAMEFONT_OUT", "BT3_NAMEFONT_DISC"
    };
    unsigned i;
    for (i = 0; i < sizeof(chaves) / sizeof(chaves[0]); i++) {
        if (strcmp(nome, chaves[i]) == 0) return 1;
    }
    return 0;
}
#endif
